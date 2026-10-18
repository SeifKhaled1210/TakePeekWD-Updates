#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <cstdint>
#include <intrin.h>
#include <atomic>

// ============================================================================
// TakePeek 2.0 WD — Stealth / Anti-Detection
// Compile-time string encryption, dynamic imports, anti-debug, PE erasure
// ============================================================================

// -- Compile-time XOR string encryption ------------------------------------

namespace xor_detail {
    constexpr char key(int i) { return static_cast<char>((i * 0x37 + 0x13) ^ 0x5A); }
    constexpr wchar_t wkey(int i) { return static_cast<wchar_t>((i * 0x2B + 0x17) ^ 0x4D); }

    template<int N>
    struct Enc {
        char data[N];
        constexpr Enc(const char(&s)[N]) : data{} {
            for (int i = 0; i < N; i++) data[i] = s[i] ^ key(i);
        }
        std::string dec() const {
            std::string r(N - 1, '\0');
            for (int i = 0; i < N - 1; i++) r[i] = data[i] ^ key(i);
            return r;
        }
    };

    template<int N>
    struct WEnc {
        wchar_t data[N];
        constexpr WEnc(const wchar_t(&s)[N]) : data{} {
            for (int i = 0; i < N; i++) data[i] = s[i] ^ wkey(i);
        }
        std::wstring dec() const {
            std::wstring r(N - 1, L'\0');
            for (int i = 0; i < N - 1; i++) r[i] = data[i] ^ wkey(i);
            return r;
        }
    };
}

#define XS(s)  ([]{ constexpr ::xor_detail::Enc<sizeof(s)> _e(s); return _e.dec(); }())
#define XSW(s) ([]{ constexpr ::xor_detail::WEnc<sizeof(s)/sizeof(wchar_t)> _e(s); return _e.dec(); }())

// -- Dynamic API resolution (hides sensitive functions from IAT) -----------

namespace DynAPI {
    using fn_OpenProcess    = HANDLE(WINAPI*)(DWORD, BOOL, DWORD);
    using fn_RPM            = BOOL(WINAPI*)(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
    using fn_WPM            = BOOL(WINAPI*)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
    using fn_Snapshot        = HANDLE(WINAPI*)(DWORD, DWORD);
    using fn_P32First       = BOOL(WINAPI*)(HANDLE, LPPROCESSENTRY32W);
    using fn_P32Next        = BOOL(WINAPI*)(HANDLE, LPPROCESSENTRY32W);
    using fn_M32First       = BOOL(WINAPI*)(HANDLE, LPMODULEENTRY32W);
    using fn_M32Next        = BOOL(WINAPI*)(HANDLE, LPMODULEENTRY32W);
    using fn_GetExitCode    = BOOL(WINAPI*)(HANDLE, LPDWORD);
    using fn_AsyncKeyState  = SHORT(WINAPI*)(int);
    using fn_VirtualProtect = BOOL(WINAPI*)(LPVOID, SIZE_T, DWORD, PDWORD);

    inline fn_OpenProcess    pOpenProcess    = nullptr;
    inline fn_RPM            pRPM            = nullptr;
    inline fn_WPM            pWPM            = nullptr;
    inline fn_Snapshot       pSnapshot       = nullptr;
    inline fn_P32First       pP32First       = nullptr;
    inline fn_P32Next        pP32Next        = nullptr;
    inline fn_M32First       pM32First       = nullptr;
    inline fn_M32Next        pM32Next        = nullptr;
    inline fn_GetExitCode    pGetExitCode    = nullptr;
    inline fn_AsyncKeyState  pAsyncKeyState  = nullptr;
    inline fn_VirtualProtect pVirtualProtect = nullptr;

    static bool Init() {
        HMODULE k32 = GetModuleHandleA(XS("kernel32.dll").c_str());
        if (!k32) return false;

        pOpenProcess    = (fn_OpenProcess)   GetProcAddress(k32, XS("OpenProcess").c_str());
        pRPM            = (fn_RPM)           GetProcAddress(k32, XS("ReadProcessMemory").c_str());
        pWPM            = (fn_WPM)           GetProcAddress(k32, XS("WriteProcessMemory").c_str());
        pSnapshot       = (fn_Snapshot)      GetProcAddress(k32, XS("CreateToolhelp32Snapshot").c_str());
        pP32First       = (fn_P32First)      GetProcAddress(k32, XS("Process32FirstW").c_str());
        pP32Next        = (fn_P32Next)       GetProcAddress(k32, XS("Process32NextW").c_str());
        pM32First       = (fn_M32First)      GetProcAddress(k32, XS("Module32FirstW").c_str());
        pM32Next        = (fn_M32Next)       GetProcAddress(k32, XS("Module32NextW").c_str());
        pGetExitCode    = (fn_GetExitCode)   GetProcAddress(k32, XS("GetExitCodeProcess").c_str());
        pVirtualProtect = (fn_VirtualProtect)GetProcAddress(k32, XS("VirtualProtect").c_str());

        HMODULE u32 = GetModuleHandleA(XS("user32.dll").c_str());
        if (u32) {
            pAsyncKeyState = (fn_AsyncKeyState)GetProcAddress(u32, XS("GetAsyncKeyState").c_str());
        }

        return pRPM && pOpenProcess && pSnapshot;
    }
}

// -- Stealth utilities -----------------------------------------------------

namespace Stealth {

    static std::wstring RandomClassName(int len = 12) {
        static const wchar_t charset[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        std::wstring result(len, L'\0');
        uint64_t seed = __rdtsc();
        for (int i = 0; i < len; i++) {
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            result[i] = charset[(seed >> 33) % (sizeof(charset) / sizeof(wchar_t) - 1)];
        }
        return result;
    }

    static void ErasePEHeaders() {
        HMODULE hMod = GetModuleHandle(nullptr);
        if (!hMod) return;
        DWORD old = 0;
        if (DynAPI::pVirtualProtect) {
            DynAPI::pVirtualProtect(hMod, 0x1000, PAGE_READWRITE, &old);
            SecureZeroMemory(hMod, 0x1000);
            DynAPI::pVirtualProtect(hMod, 0x1000, old, &old);
        } else {
            VirtualProtect(hMod, 0x1000, PAGE_READWRITE, &old);
            SecureZeroMemory(hMod, 0x1000);
            VirtualProtect(hMod, 0x1000, old, &old);
        }
    }

    static bool IsBeingDebugged() {
        if (IsDebuggerPresent()) return true;

        BOOL remote = FALSE;
        CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote);
        if (remote) return true;

        // PEB NtGlobalFlag — set by debugger heap flags
        unsigned __int64 peb = __readgsqword(0x60);
        if (peb) {
            DWORD ntGlobalFlag = *(DWORD*)(peb + 0xBC);
            if (ntGlobalFlag & 0x70) return true;
            // BeingDebugged byte in PEB (backup for hooked IsDebuggerPresent)
            if (*(BYTE*)(peb + 0x02)) return true;
        }

        // NtQueryInformationProcess — ProcessDebugPort (0x07)
        {
            using fn_NTQIP = LONG(WINAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
            HMODULE ntdll = GetModuleHandleA(XS("ntdll.dll").c_str());
            if (ntdll) {
                auto pNTQIP = (fn_NTQIP)GetProcAddress(ntdll, XS("NtQueryInformationProcess").c_str());
                if (pNTQIP) {
                    ULONG_PTR debugPort = 0;
                    LONG status = pNTQIP(GetCurrentProcess(), 7, &debugPort, sizeof(debugPort), nullptr);
                    if (status == 0 && debugPort != 0) return true;

                    // ProcessDebugObjectHandle (0x1E)
                    HANDLE debugObj = nullptr;
                    status = pNTQIP(GetCurrentProcess(), 0x1E, &debugObj, sizeof(debugObj), nullptr);
                    if (status == 0 && debugObj != nullptr) return true;

                    // ProcessDebugFlags (0x1F) — returns 0 when debugged
                    DWORD debugFlags = 1;
                    status = pNTQIP(GetCurrentProcess(), 0x1F, &debugFlags, sizeof(debugFlags), nullptr);
                    if (status == 0 && debugFlags == 0) return true;
                }
            }
        }

        // Hardware breakpoint detection via thread context
        {
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(GetCurrentThread(), &ctx)) {
                if (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3) return true;
            }
        }

        return false;
    }

    // Timing-based anti-debug — debugger single-stepping inflates tick delta
    // Uses 500ms threshold and best-of-3 to avoid false positives from OS preemption
    static bool TimingCheck() {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        int slowCount = 0;
        for (int attempt = 0; attempt < 3; attempt++) {
            LARGE_INTEGER t1, t2;
            QueryPerformanceCounter(&t1);
            volatile int dummy = 0;
            for (int i = 0; i < 100; i++) dummy += i;
            QueryPerformanceCounter(&t2);
            double ms = (double)(t2.QuadPart - t1.QuadPart) / freq.QuadPart * 1000.0;
            if (ms > 500.0) slowCount++;
        }
        return slowCount >= 2;
    }

    // CRC32 integrity check on own .text section
    static uint32_t s_textCRC = 0;
    static uintptr_t s_textBase = 0;
    static size_t s_textSize = 0;

    static uint32_t ComputeTextCRC() {
        if (!s_textBase || !s_textSize) return 0;
        uint32_t crc = 0xFFFFFFFF;
        const uint8_t* p = (const uint8_t*)s_textBase;
        for (size_t i = 0; i < s_textSize; i++) {
            crc ^= p[i];
            for (int j = 0; j < 8; j++)
                crc = (crc >> 1) ^ (0xEDB88320 & (-(int32_t)(crc & 1)));
        }
        return crc ^ 0xFFFFFFFF;
    }

    static bool InitIntegrityCheck() {
        HMODULE hMod = GetModuleHandle(nullptr);
        if (!hMod) return false;
        auto dos = (IMAGE_DOS_HEADER*)hMod;
        auto nt = (IMAGE_NT_HEADERS*)((uint8_t*)hMod + dos->e_lfanew);
        auto section = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, section++) {
            if (memcmp(section->Name, ".text", 5) == 0) {
                s_textBase = (uintptr_t)hMod + section->VirtualAddress;
                s_textSize = section->Misc.VirtualSize;
                s_textCRC = ComputeTextCRC();
                return true;
            }
        }
        return false;
    }

    static bool IntegrityViolated() {
        if (!s_textCRC) return false;
        return ComputeTextCRC() != s_textCRC;
    }

    static void JitteredSleep(int baseMs, int jitterMs = 4) {
        uint64_t t = __rdtsc();
        int jitter = static_cast<int>(t % (jitterMs * 2 + 1)) - jitterMs;
        int delay = baseMs + jitter;
        if (delay < 1) delay = 1;
        Sleep(delay);
    }

    // Background watchdog thread — periodically checks for debuggers and patching
    static inline std::atomic<bool> s_watchdogRunning{false};

    static void WatchdogLog(const char* reason) {
        char logPath[MAX_PATH];
        GetModuleFileNameA(nullptr, logPath, MAX_PATH);
        std::string lp(logPath);
        lp = lp.substr(0, lp.find_last_of("\\/") + 1) + "watchdog.log";
        HANDLE hFile = CreateFileA(lp.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            SYSTEMTIME st; GetLocalTime(&st);
            char buf[256];
            int n = sprintf_s(buf, "[%04d-%02d-%02d %02d:%02d:%02d] WATCHDOG KILL: %s\r\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, reason);
            DWORD written; WriteFile(hFile, buf, n, &written, nullptr);
            CloseHandle(hFile);
        }
    }

    static void WatchdogThread() {
        int warmup = 3;
        while (s_watchdogRunning.load()) {
            if (warmup > 0) { warmup--; JitteredSleep(3000, 500); continue; }
            bool dbg = IsBeingDebugged();
            bool tmg = !dbg && TimingCheck();
            bool itg = !dbg && !tmg && IntegrityViolated();
            if (dbg || tmg || itg) {
                const char* reason = dbg ? "debugger_detected" : (tmg ? "timing_anomaly" : "integrity_violated");
                WatchdogLog(reason);
                HMODULE hMod = GetModuleHandle(nullptr);
                if (hMod) {
                    DWORD old = 0;
                    VirtualProtect(hMod, 0x1000, PAGE_READWRITE, &old);
                    SecureZeroMemory(hMod, 0x1000);
                }
                TerminateProcess(GetCurrentProcess(), 0);
            }
            JitteredSleep(3000, 500);
        }
    }

    static void StartWatchdog() {
        s_watchdogRunning.store(true);
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            WatchdogThread();
            return 0;
        }, nullptr, 0, nullptr);
    }

    static void StopWatchdog() {
        s_watchdogRunning.store(false);
    }
}
