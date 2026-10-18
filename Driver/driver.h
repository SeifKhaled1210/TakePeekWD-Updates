#pragma once

#include <cstdint>
#include <cstddef>
#include <windows.h>
#include <winioctl.h>
#include "shared.h"
#include "../SDK/stealth.h"

#define IOCTL_PROCESS_REQUEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

inline std::string g_driverDeviceName = XS("TpkWd0");

class DriverComm {
public:
    DriverComm() = default;
    ~DriverComm() { Cleanup(); }

    bool Init() {
        std::string path = XS("\\\\.\\") + g_driverDeviceName;
        m_hDevice = CreateFileA(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr);
        if (m_hDevice == INVALID_HANDLE_VALUE) return false;

        REQUEST_DATA req = { 0 };
        req.command = CMD_PING;
        if (!SendRequest(&req)) { Cleanup(); return false; }

        m_active = (req.result == 0x50544548 || req.result == 0x4B524E4C);
        m_isPteHook = (req.result == 0x50544548);
        if (!m_active) { Cleanup(); }
        return m_active;
    }

    bool IsConnected() const { return m_active; }
    bool IsPteHook() const { return m_isPteHook; }

    void Cleanup() {
        if (m_hDevice != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hDevice);
            m_hDevice = INVALID_HANDLE_VALUE;
        }
        m_active = false;
        m_pid = 0;
    }

    bool BindProcess(uint32_t pid) {
        m_pid = pid;
        return m_active;
    }

    uintptr_t GetModuleBase(uint32_t pid, const wchar_t* moduleName) {
        REQUEST_DATA req = { 0 };
        req.command = CMD_MODULE_BASE;
        req.pid = pid;
        wcsncpy_s(req.module_name, 64, moduleName, _TRUNCATE);
        SendRequest(&req);
        return (uintptr_t)req.result;
    }

    uintptr_t GetPeb(uint32_t pid) {
        REQUEST_DATA req = { 0 };
        req.command = CMD_GET_PEB;
        req.pid = pid;
        SendRequest(&req);
        return (uintptr_t)req.result;
    }

    bool ReadMemory(uint32_t pid, uintptr_t address, void* buffer, size_t size) {
        REQUEST_DATA req = { 0 };
        req.command = CMD_READ;
        req.pid = pid;
        req.address = address;
        req.buffer = (unsigned __int64)buffer;
        req.size = size;
        return SendRequest(&req) && req.result;
    }

    bool WriteMemory(uint32_t pid, uintptr_t address, void* buffer, size_t size) {
        REQUEST_DATA req = { 0 };
        req.command = CMD_WRITE;
        req.pid = pid;
        req.address = address;
        req.buffer = (unsigned __int64)buffer;
        req.size = size;
        return SendRequest(&req) && req.result;
    }

    uintptr_t AllocMemory(uint32_t pid, size_t size, DWORD protect) {
        REQUEST_DATA req = { 0 };
        req.command = CMD_ALLOC;
        req.pid = pid;
        req.size = size;
        req.protect = protect;
        SendRequest(&req);
        return (uintptr_t)req.result;
    }

    void FreeMemory(uint32_t pid, uintptr_t address) {
        REQUEST_DATA req = { 0 };
        req.command = CMD_FREE;
        req.pid = pid;
        req.result = address;
        SendRequest(&req);
    }

    void ProtectMemory(uint32_t pid, uintptr_t address, size_t size, DWORD protect) {
        REQUEST_DATA req = { 0 };
        req.command = CMD_PROTECT;
        req.pid = pid;
        req.address = address;
        req.size = size;
        req.protect = protect;
        SendRequest(&req);
    }

    bool MouseMove(int32_t dx, int32_t dy) {
        (void)dx; (void)dy;
        return false;
    }

private:
    bool SendRequest(REQUEST_DATA* req) {
        if (m_hDevice == INVALID_HANDLE_VALUE) return false;
        req->magic = REQUEST_MAGIC;
        DWORD bytesReturned = 0;
        return DeviceIoControl(
            m_hDevice, IOCTL_PROCESS_REQUEST,
            req, sizeof(REQUEST_DATA),
            req, sizeof(REQUEST_DATA),
            &bytesReturned, nullptr) != 0;
    }

    HANDLE   m_hDevice  = INVALID_HANDLE_VALUE;
    bool     m_active   = false;
    bool     m_isPteHook = false;
    uint32_t m_pid      = 0;
};
