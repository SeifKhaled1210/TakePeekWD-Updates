#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include <fstream>

#include "include/gdrv_mapper.h"
#include "include/driver_resource.hpp"

namespace DriverLoader {

    static std::wstring GetDeviceNameFilePath() {
        wchar_t tempDir[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, tempDir);
        return std::wstring(tempDir) + L"dxgi_cache.tmp";
    }

    static void SaveDeviceName(const std::string& name) {
        std::wstring path = GetDeviceNameFilePath();
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD written = 0;
        WriteFile(hFile, name.c_str(), (DWORD)name.size(), &written, nullptr);
        CloseHandle(hFile);
    }

    static std::string LoadSavedDeviceName() {
        std::wstring path = GetDeviceNameFilePath();
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, 0, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return "";
        char buf[32] = {};
        DWORD bytesRead = 0;
        ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, nullptr);
        CloseHandle(hFile);
        if (bytesRead < 3 || bytesRead > 15) return "";
        return std::string(buf, bytesRead);
    }

    static void ClearSavedDeviceName() {
        DeleteFileW(GetDeviceNameFilePath().c_str());
    }

    static bool TryExistingDriver(const std::string& savedName) {
        std::string path = "\\\\.\\" + savedName;
        HANDLE hDev = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDev == INVALID_HANDLE_VALUE) return false;
        CloseHandle(hDev);
        return true;
    }

    static std::string GenerateRandomDeviceName(int len = 6) {
        const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        char name[16] = {};
        if (len > 15) len = 15;
        LARGE_INTEGER pc;
        QueryPerformanceCounter(&pc);
        uint64_t seed = pc.QuadPart ^ GetTickCount64() ^ (uint64_t)GetCurrentProcessId();
        for (int i = 0; i < len; i++) {
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            name[i] = charset[(seed >> 33) % (sizeof(charset) - 1)];
        }
        return std::string(name);
    }

    static int PatchDriverDeviceNameBytes(std::vector<uint8_t>& data, const char* newName) {
        const char* original = "TpkWd0";
        size_t nameLen = strlen(original);
        size_t newLen = strlen(newName);
        if (newLen != nameLen) return 0;

        uint8_t origWide[16], newWide[16];
        for (size_t i = 0; i < nameLen; i++) {
            origWide[i * 2] = (uint8_t)original[i];
            origWide[i * 2 + 1] = 0x00;
            newWide[i * 2] = (uint8_t)newName[i];
            newWide[i * 2 + 1] = 0x00;
        }
        size_t wideBytes = nameLen * 2;

        int patches = 0;
        for (size_t i = 0; i + wideBytes <= data.size(); i++) {
            if (memcmp(&data[i], origWide, wideBytes) == 0) {
                memcpy(&data[i], newWide, wideBytes);
                patches++;
            }
        }
        return patches;
    }

    static bool RunPowerShell(const wchar_t* cmd) {
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {};
        wchar_t cmdLine[512];
        swprintf_s(cmdLine, L"powershell.exe -NoProfile -NonInteractive -Command \"%s\"", cmd);
        if (!CreateProcessW(nullptr, cmdLine, nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
            return false;
        WaitForSingleObject(pi.hProcess, 5000);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }

    static void AddTempDefenderExclusion(wchar_t* tempDir) {
        GetTempPathW(MAX_PATH, tempDir);
        size_t len = wcslen(tempDir);
        if (len > 0 && tempDir[len - 1] == L'\\') tempDir[len - 1] = L'\0';

        wchar_t psCmd[256];
        swprintf_s(psCmd, L"Add-MpPreference -ExclusionPath '%s'", tempDir);
        if (RunPowerShell(psCmd)) {
            Sleep(500);
            return;
        }

        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows Defender\\Exclusions\\Paths",
            0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            DWORD val = 0;
            RegSetValueExW(hKey, tempDir, 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
            RegCloseKey(hKey);
            Sleep(500);
        }
    }

    static void RemoveTempDefenderExclusion(const wchar_t* tempDir) {
        wchar_t psCmd[256];
        swprintf_s(psCmd, L"Remove-MpPreference -ExclusionPath '%s'", tempDir);
        if (RunPowerShell(psCmd)) return;

        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows Defender\\Exclusions\\Paths",
            0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, tempDir);
            RegCloseKey(hKey);
        }
    }

    static bool DisableVulnerableDriverBlocklist() {
        bool ok = false;

        HKEY hKey = nullptr;
        LSTATUS st = RegCreateKeyExW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\CI\\Config",
            0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr);
        if (st == ERROR_SUCCESS) {
            DWORD zero = 0;
            st = RegSetValueExW(hKey, L"VulnerableDriverBlocklistEnable", 0,
                REG_DWORD, (const BYTE*)&zero, sizeof(zero));
            RegCloseKey(hKey);
            if (st == ERROR_SUCCESS) ok = true;
        }

        if (!ok) {
            wchar_t cmd[] = L"Set-ItemProperty -Path 'HKLM:\\SYSTEM\\CurrentControlSet\\Control\\CI\\Config' -Name 'VulnerableDriverBlocklistEnable' -Value 0 -Type DWord";
            ok = RunPowerShell(cmd);
        }

        HKEY hvciKey = nullptr;
        st = RegCreateKeyExW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity",
            0, nullptr, 0, KEY_SET_VALUE, nullptr, &hvciKey, nullptr);
        if (st == ERROR_SUCCESS) {
            DWORD zero = 0;
            RegSetValueExW(hvciKey, L"Enabled", 0, REG_DWORD, (const BYTE*)&zero, sizeof(zero));
            RegCloseKey(hvciKey);
        }

        HKEY vbsKey = nullptr;
        st = RegCreateKeyExW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
            0, nullptr, 0, KEY_SET_VALUE, nullptr, &vbsKey, nullptr);
        if (st == ERROR_SUCCESS) {
            DWORD zero = 0;
            RegSetValueExW(vbsKey, L"EnableVirtualizationBasedSecurity", 0,
                REG_DWORD, (const BYTE*)&zero, sizeof(zero));
            RegCloseKey(vbsKey);
        }

        return ok;
    }

    static void RestoreVulnerableDriverBlocklist() {
        DWORD one = 1;

        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\CI\\Config",
            0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, L"VulnerableDriverBlocklistEnable", 0,
                REG_DWORD, (const BYTE*)&one, sizeof(one));
            RegCloseKey(hKey);
        }

        HKEY hvciKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity",
            0, KEY_SET_VALUE, &hvciKey) == ERROR_SUCCESS) {
            RegSetValueExW(hvciKey, L"Enabled", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
            RegCloseKey(hvciKey);
        }

        HKEY vbsKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
            0, KEY_SET_VALUE, &vbsKey) == ERROR_SUCCESS) {
            RegSetValueExW(vbsKey, L"EnableVirtualizationBasedSecurity", 0,
                REG_DWORD, (const BYTE*)&one, sizeof(one));
            RegCloseKey(vbsKey);
        }
    }

    static bool LoadEmbeddedDriver(std::string& statusOut, const char* deviceName = nullptr) {
        if (takepeek_driver_resource::driver_size <= 1) {
            statusOut = "Driver bytes not embedded (placeholder)";
            return false;
        }

        wchar_t tempDir[MAX_PATH] = {};
        AddTempDefenderExclusion(tempDir);
        DisableVulnerableDriverBlocklist();

        // Prepare driver data with patched device name
        std::vector<uint8_t> driverData(
            takepeek_driver_resource::driver,
            takepeek_driver_resource::driver + takepeek_driver_resource::driver_size);

        std::string driverFilename = "tpkwd";
        if (deviceName && strlen(deviceName) == 6) {
            int patched = PatchDriverDeviceNameBytes(driverData, deviceName);
            statusOut = "Patched " + std::to_string(patched) + " device name refs -> " + deviceName;
            driverFilename = std::string(deviceName);
        }

        // Service name must differ from device name — the driver calls IoCreateDriver(\Driver\{deviceName})
        // which collides with NtLoadDriver's \Driver\{serviceName} if they match
        std::wstring wFilename = L"tp_";
        wFilename += std::wstring(driverFilename.begin(), driverFilename.end());
        wFilename += L".sys";

        std::string loadStatus;
        bool ok = GdrvMapper::LoadDriverWithCIBypass(
            driverData.data(),
            driverData.size(),
            wFilename.c_str(),
            loadStatus);

        RemoveTempDefenderExclusion(tempDir);
        RestoreVulnerableDriverBlocklist();

        if (!ok) {
            statusOut = loadStatus;
            return false;
        }

        statusOut = loadStatus;
        return true;
    }
}
