#pragma once
#include <Windows.h>
#include <winioctl.h>
#include <winternl.h>
#include <TlHelp32.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include "stealth.h"
#include "../kdmapper/include/kernel_rw.h"

namespace Memory {

inline DWORD  g_pid = 0;
inline uintptr_t g_baseAddress = 0;
inline size_t    g_moduleSize = 0;

inline HANDLE g_proc = nullptr;
inline int g_attachMethod = 0;

inline KernelRW::Context g_sysCtx = {};
inline KernelRW::Context g_gameCtx = {};
inline bool g_usePhysRW = false;

inline HANDLE g_scanProc = nullptr;

static void BeginScan() {
    if (g_usePhysRW && g_pid && DynAPI::pOpenProcess) {
        g_scanProc = DynAPI::pOpenProcess(PROCESS_VM_READ, FALSE, g_pid);
    }
}

static void EndScan() {
    if (g_scanProc) {
        CloseHandle(g_scanProc);
        g_scanProc = nullptr;
    }
}

template<typename T>
static T Read(uintptr_t addr) {
    T val{};
    if (addr < 0x1000 || addr > 0x7FFFFFFFFFFF) return val;
    if (g_scanProc && DynAPI::pRPM) {
        DynAPI::pRPM(g_scanProc, (LPCVOID)addr, &val, sizeof(T), nullptr);
        return val;
    }
    if (g_usePhysRW) {
        KernelRW::ReadBuffer(g_gameCtx, (DWORD64)addr, &val, sizeof(T));
    } else if (DynAPI::pRPM && g_proc) {
        DynAPI::pRPM(g_proc, (LPCVOID)addr, &val, sizeof(T), nullptr);
    }
    return val;
}

static bool ReadBuffer(uintptr_t addr, void* buf, size_t size) {
    if (addr < 0x1000 || addr > 0x7FFFFFFFFFFF) return false;
    if (g_scanProc && DynAPI::pRPM) {
        return DynAPI::pRPM(g_scanProc, (LPCVOID)addr, buf, size, nullptr) != FALSE;
    }
    if (g_usePhysRW) {
        return KernelRW::ReadBuffer(g_gameCtx, (DWORD64)addr, buf, size);
    } else if (DynAPI::pRPM && g_proc) {
        return DynAPI::pRPM(g_proc, (LPCVOID)addr, buf, size, nullptr) != FALSE;
    }
    return false;
}

template<typename T>
static bool Write(uintptr_t addr, const T& val) {
    if (addr < 0x1000 || addr > 0x7FFFFFFFFFFF) return false;
    if (g_usePhysRW) {
        return KernelRW::WriteBuffer(g_gameCtx, (DWORD64)addr, &val, sizeof(T));
    } else if (DynAPI::pWPM && g_proc) {
        return DynAPI::pWPM(g_proc, (LPVOID)addr, &val, sizeof(T), nullptr) != FALSE;
    }
    return false;
}

static bool IsValidPtr(uint64_t p) {
    return p > 0x10000 && p < 0x7FFFFFFFFFFF;
}

static DWORD FindProcess(const wchar_t* name) {
    if (!DynAPI::pSnapshot || !DynAPI::pP32First || !DynAPI::pP32Next) return 0;
    HANDLE snap = DynAPI::pSnapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = {sizeof(pe)};
    DWORD pid = 0;
    if (DynAPI::pP32First(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (DynAPI::pP32Next(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static bool AttachPhysicalDirect(DWORD pid, const KernelRW::Context& gameCtx,
                                  uintptr_t base, size_t moduleSize) {
    g_pid = pid;
    g_gameCtx = gameCtx;
    g_usePhysRW = true;
    g_attachMethod = 3;
    g_baseAddress = base;
    g_moduleSize = moduleSize;
    return true;
}

static bool AttachUsermode(DWORD pid) {
    if (!DynAPI::pOpenProcess) return false;
    g_pid = pid;
    g_proc = DynAPI::pOpenProcess(
        PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    if (!g_proc) { g_pid = 0; return false; }
    g_attachMethod = 2;
    return true;
}

static size_t ReadModuleSizeFromPE(uintptr_t base) {
    IMAGE_DOS_HEADER dos{};
    if (!ReadBuffer(base, &dos, sizeof(dos))) return 0;
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS64 nt{};
    if (!ReadBuffer(base + dos.e_lfanew, &nt, sizeof(nt))) return 0;
    if (nt.Signature != IMAGE_NT_SIGNATURE) return 0;
    return (size_t)nt.OptionalHeader.SizeOfImage;
}

static uintptr_t GetModuleBasePEB(HANDLE proc) {
    typedef NTSTATUS(NTAPI* NtQueryInformationProcess_t)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    static auto NtQIP = (NtQueryInformationProcess_t)GetProcAddress(
        GetModuleHandleA(XS("ntdll.dll").c_str()), XS("NtQueryInformationProcess").c_str());
    if (!NtQIP) return 0;

    PROCESS_BASIC_INFORMATION pbi = {};
    NTSTATUS st = NtQIP(proc, 0, &pbi, sizeof(pbi), nullptr);
    if (st != 0 || !pbi.PebBaseAddress) return 0;

    uintptr_t imageBase = 0;
    if (!DynAPI::pRPM) return 0;
    if (!DynAPI::pRPM(proc, (BYTE*)pbi.PebBaseAddress + 0x10, &imageBase, sizeof(imageBase), nullptr))
        return 0;
    return imageBase;
}

static void Detach() {
    if (g_usePhysRW) {
        g_usePhysRW = false;
        g_gameCtx = {};
    }
    if (g_proc) {
        CloseHandle(g_proc);
        g_proc = nullptr;
    }
    g_attachMethod = 0;
    g_pid = 0;
    g_baseAddress = 0;
    g_moduleSize = 0;
}

static bool IsProcessAlive() {
    if (!g_pid) return false;
    if (!DynAPI::pSnapshot || !DynAPI::pP32First || !DynAPI::pP32Next) return false;
    HANDLE snap = DynAPI::pSnapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = {sizeof(pe)};
    bool found = false;
    if (DynAPI::pP32First(snap, &pe)) {
        do {
            if (pe.th32ProcessID == g_pid) { found = true; break; }
        } while (DynAPI::pP32Next(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

inline size_t g_scanChunkSize = 0x100000;

static uintptr_t PatternScan(uintptr_t base, size_t size, const uint8_t* pattern, const char* mask) {
    size_t patLen = strlen(mask);
    if (patLen == 0 || size < patLen) return 0;

    size_t chunkSize = g_scanChunkSize;
    for (size_t offset = 0; offset < size; offset += chunkSize - patLen) {
        size_t scanSize = ((chunkSize) < (size - offset)) ? (chunkSize) : (size - offset);
        std::vector<uint8_t> buf(scanSize);
        if (!ReadBuffer(base + offset, buf.data(), scanSize)) continue;

        for (size_t i = 0; i <= scanSize - patLen; i++) {
            bool found = true;
            for (size_t j = 0; j < patLen; j++) {
                if (mask[j] == '?') continue;
                if (buf[i + j] != pattern[j]) { found = false; break; }
            }
            if (found) return base + offset + i;
        }
    }
    return 0;
}

} // namespace Memory
