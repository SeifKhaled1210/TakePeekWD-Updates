#pragma once
#include <Windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <intrin.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <ctime>
#include "../SDK/stealth.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")

namespace TpAuth {

template<size_t N, char K>
struct ObfString {
    char data[N];
    constexpr ObfString(const char(&src)[N]) : data{} {
        for (size_t i = 0; i < N; i++) data[i] = src[i] ^ K;
    }
    std::string decode() const {
        std::string out(N - 1, '\0');
        for (size_t i = 0; i < N - 1; i++) out[i] = data[i] ^ K;
        return out;
    }
};

#define OBF(s) []{ constexpr auto o = ObfString<sizeof(s), 0x5A>(s); return o; }().decode()

inline std::wstring GetServerHost() {
    auto s = OBF("212.47.72.201");
    return std::wstring(s.begin(), s.end());
}
inline INTERNET_PORT  SERVER_PORT = 7580;
inline std::string GetHmacSecret() { return OBF("tpk_wd_2026_hmac_9f3a7c1b"); }
inline std::string GetAppId()      { return OBF("wardogs"); }

inline std::string g_sessionToken;
inline std::string g_cachedHwid;

struct AuthResponse {
    bool success = false;
    std::string error;
    std::string tier;
    std::string expires_at;
    std::string message;
    std::string session_token;
    bool hwid_locked = false;
    bool reauth = false;
};

static std::string GetCpuId() {
    int info[4] = {};
    __cpuid(info, 0);
    std::ostringstream ss;
    ss << std::hex << info[0] << info[1] << info[2] << info[3];
    __cpuid(info, 1);
    ss << info[0] << info[3];
    return ss.str();
}

static std::string ReadReg(HKEY root, const char* subkey, const char* value) {
    char buf[256] = {};
    DWORD len = sizeof(buf) - 1;
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(root, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, value, nullptr, nullptr, (LPBYTE)buf, &len);
        RegCloseKey(hKey);
    }
    return std::string(buf);
}

static std::string Sha256(const std::string& data) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string result;

    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return "";

    if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        if (CryptHashData(hHash, (const BYTE*)data.c_str(), (DWORD)data.size(), 0)) {
            DWORD hashLen = 32;
            BYTE hash[32];
            if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
                std::ostringstream ss;
                for (DWORD i = 0; i < hashLen; i++)
                    ss << std::hex << std::setfill('0') << std::setw(2) << (int)hash[i];
                result = ss.str();
            }
        }
        CryptDestroyHash(hHash);
    }
    CryptReleaseContext(hProv, 0);
    return result;
}

static std::string HmacSha256(const std::string& key, const std::string& data) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHmac = 0;
    HCRYPTKEY hKey = 0;
    std::string result;

    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return "";

    struct {
        BLOBHEADER hdr;
        DWORD keyLen;
        BYTE keyData[256];
    } keyBlob = {};
    keyBlob.hdr.bType = PLAINTEXTKEYBLOB;
    keyBlob.hdr.bVersion = CUR_BLOB_VERSION;
    keyBlob.hdr.aiKeyAlg = CALG_RC2;
    keyBlob.keyLen = (DWORD)key.size();
    memcpy(keyBlob.keyData, key.c_str(), key.size());

    if (CryptImportKey(hProv, (BYTE*)&keyBlob, sizeof(BLOBHEADER) + sizeof(DWORD) + (DWORD)key.size(), 0, CRYPT_IPSEC_HMAC_KEY, &hKey)) {
        if (CryptCreateHash(hProv, CALG_HMAC, hKey, 0, &hHmac)) {
            HMAC_INFO hmacInfo = {};
            hmacInfo.HashAlgid = CALG_SHA_256;
            if (CryptSetHashParam(hHmac, HP_HMAC_INFO, (BYTE*)&hmacInfo, 0)) {
                if (CryptHashData(hHmac, (const BYTE*)data.c_str(), (DWORD)data.size(), 0)) {
                    DWORD hashLen = 32;
                    BYTE hash[32];
                    if (CryptGetHashParam(hHmac, HP_HASHVAL, hash, &hashLen, 0)) {
                        std::ostringstream ss;
                        for (DWORD i = 0; i < hashLen; i++)
                            ss << std::hex << std::setfill('0') << std::setw(2) << (int)hash[i];
                        result = ss.str();
                    }
                }
            }
            CryptDestroyHash(hHmac);
        }
        CryptDestroyKey(hKey);
    }
    CryptReleaseContext(hProv, 0);
    return result;
}

static std::string GetHWID() {
    std::string composite;
    composite += "CPU:" + GetCpuId() + "|";
    composite += "MID:" + ReadReg(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Cryptography", "MachineGuid") + "|";
    composite += "BIO:" + ReadReg(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemSerialNumber") + "|";
    composite += "MBD:" + ReadReg(HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\BIOS", "BaseBoardProduct") + "|";
    composite += "PID:" + ReadReg(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductId") + "|";

    DWORD serial = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
    std::ostringstream vs;
    vs << std::hex << serial;
    composite += "VOL:" + vs.str();

    return Sha256(composite);
}

static std::string HttpPost(const wchar_t* path, const std::string& jsonBody) {
    std::string response;
    std::wstring host = GetServerHost();

    HINTERNET hSession = WinHttpOpen(XSW(L"Mozilla/5.0").c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), SERVER_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    // Accept self-signed certificate
    DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                     SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                     SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));

    DWORD resolveTimeout = 10000;
    DWORD connectTimeout = 15000;
    DWORD sendTimeout = 10000;
    DWORD recvTimeout = 10000;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RESOLVE_TIMEOUT, &resolveTimeout, sizeof(resolveTimeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &connectTimeout, sizeof(connectTimeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &sendTimeout, sizeof(sendTimeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &recvTimeout, sizeof(recvTimeout));

    std::string sig = HmacSha256(GetHmacSecret(), jsonBody);
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!sig.empty()) {
        headers += L"X-Signature: ";
        headers += std::wstring(sig.begin(), sig.end());
        headers += L"\r\n";
    }

    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.size(),
        (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.size(), (DWORD)jsonBody.size(), 0);

    if (sent && WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD bytesAvailable = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
            std::vector<char> buf(bytesAvailable + 1, 0);
            DWORD bytesRead = 0;
            WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
            response.append(buf.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

static std::string JsonGet(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        size_t start = pos + 1;
        size_t end = json.find('"', start);
        if (end == std::string::npos) return "";
        return json.substr(start, end - start);
    }
    size_t start = pos;
    size_t end = json.find_first_of(",}", start);
    if (end == std::string::npos) end = json.size();
    std::string val = json.substr(start, end - start);
    while (!val.empty() && (val.back() == ' ' || val.back() == '\n' || val.back() == '\r')) val.pop_back();
    return val;
}

static bool JsonBool(const std::string& json, const std::string& key) {
    return JsonGet(json, key) == "true";
}

static AuthResponse Validate(const std::string& licenseKey, const std::string& clientVersion) {
    AuthResponse resp;
    std::string hwid = GetHWID();
    if (hwid.empty()) { resp.error = "Failed to generate HWID"; return resp; }
    g_cachedHwid = hwid;

    long long ts = static_cast<long long>(std::time(nullptr));
    std::string body = "{\"license_key\":\"" + licenseKey +
        "\",\"hwid\":\"" + hwid +
        "\",\"client_version\":\"" + clientVersion +
        "\",\"app_id\":\"" + GetAppId() +
        "\",\"timestamp\":" + std::to_string(ts) + "}";

    std::string raw = HttpPost(XSW(L"/api/validate").c_str(), body);
    if (raw.empty()) { resp.error = "Cannot connect to license server"; return resp; }

    resp.success = JsonBool(raw, "success");
    resp.error = JsonGet(raw, "error");
    resp.tier = JsonGet(raw, "tier");
    resp.expires_at = JsonGet(raw, "expires_at");
    resp.message = JsonGet(raw, "message");
    resp.session_token = JsonGet(raw, "session_token");
    resp.hwid_locked = JsonBool(raw, "hwid_locked");

    if (resp.success && !resp.session_token.empty()) {
        g_sessionToken = resp.session_token;
    }

    if (!resp.success && resp.error.empty()) resp.error = "Unknown error";
    return resp;
}

static bool CheckStatus() {
    std::string body = "{\"app_id\":\"" + GetAppId() + "\"}";
    std::string raw = HttpPost(XSW(L"/api/status").c_str(), body);
    if (raw.empty()) return true;
    if (JsonBool(raw, "kill_switch")) return false;
    return true;
}

struct HeartbeatResponse {
    bool success = false;
    bool kill_switch = false;
    bool reauth = false;
    std::string error;
};

static HeartbeatResponse Heartbeat() {
    HeartbeatResponse resp;
    if (g_sessionToken.empty() || g_cachedHwid.empty()) {
        resp.reauth = true;
        resp.error = "No session";
        return resp;
    }

    std::string body = "{\"session_token\":\"" + g_sessionToken +
        "\",\"hwid\":\"" + g_cachedHwid + "\"}";

    std::string raw = HttpPost(XSW(L"/api/heartbeat").c_str(), body);
    if (raw.empty()) { resp.success = true; return resp; }

    resp.success = JsonBool(raw, "success");
    resp.kill_switch = JsonBool(raw, "kill_switch");
    resp.reauth = JsonBool(raw, "reauth");
    resp.error = JsonGet(raw, "error");
    return resp;
}

struct ResetHwidResponse {
    bool success = false;
    std::string error;
    int resets_used = 0;
    int resets_max = 0;
};

static ResetHwidResponse ResetHwid(const std::string& licenseKey) {
    ResetHwidResponse resp;
    std::string body = "{\"license_key\":\"" + licenseKey +
        "\",\"app_id\":\"" + GetAppId() + "\"}";

    std::string raw = HttpPost(XSW(L"/api/reset-hwid").c_str(), body);
    if (raw.empty()) { resp.error = "Cannot connect to server"; return resp; }

    resp.success = JsonBool(raw, "success");
    resp.error = JsonGet(raw, "error");
    std::string used = JsonGet(raw, "resets_used");
    std::string max_ = JsonGet(raw, "resets_max");
    if (!used.empty()) resp.resets_used = atoi(used.c_str());
    if (!max_.empty()) resp.resets_max = atoi(max_.c_str());
    return resp;
}

static void ClearSession() {
    g_sessionToken.clear();
    SecureZeroMemory(g_cachedHwid.data(), g_cachedHwid.size());
    g_cachedHwid.clear();
}

} // namespace TpAuth
