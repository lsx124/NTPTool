#include "pch.h"
#include "Ntp.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <string>
#include <mutex>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

namespace
{
    // --- WinSock one-time init ---
    static std::once_flag g_wsaOnce;
    static void EnsureWSA() {
        std::call_once(g_wsaOnce, [](){
            WSADATA wsa{}; WSAStartup(MAKEWORD(2,2), &wsa);
            atexit([](){ WSACleanup(); });
        });
    }

    struct AddrEntry { sockaddr_storage ss{}; int len = 0; int family = AF_UNSPEC; };
    struct AddrCache {
        std::wstring server; unsigned short port = 0; DWORD tick = 0; std::vector<AddrEntry> addrs;
    };
    static AddrCache g_cache;
    static std::mutex g_cacheMtx;

    static bool ResolveAddresses(const std::wstring& server, unsigned short port, std::vector<AddrEntry>& out)
    {
        const DWORD TTL = 60 * 1000; // 1 分钟缓存
        DWORD now = GetTickCount();
        {
            std::lock_guard<std::mutex> lk(g_cacheMtx);
            if (server == g_cache.server && port == g_cache.port && (now - g_cache.tick) < TTL && !g_cache.addrs.empty()) {
                out = g_cache.addrs; return true;
            }
        }

        addrinfoW hints{}; hints.ai_socktype = SOCK_DGRAM; hints.ai_family = AF_UNSPEC; hints.ai_protocol = IPPROTO_UDP; hints.ai_flags = AI_ADDRCONFIG;
        wchar_t portStr[16]{}; _itow_s(port, portStr, 10);
        addrinfoW* res = nullptr;
        int gai = GetAddrInfoW(server.c_str(), portStr, &hints, &res);
        if (gai != 0 || !res) return false;

        std::vector<AddrEntry> tmp;
        for (addrinfoW* p = res; p; p = p->ai_next) {
            if (!p->ai_addr || p->ai_addrlen <= 0) continue;
            AddrEntry e{}; e.family = p->ai_family; e.len = (int)p->ai_addrlen; memcpy(&e.ss, p->ai_addr, p->ai_addrlen); tmp.push_back(e);
        }
        FreeAddrInfoW(res);

        if (tmp.empty()) return false;
        {
            std::lock_guard<std::mutex> lk(g_cacheMtx);
            g_cache.server = server; g_cache.port = port; g_cache.tick = now; g_cache.addrs = tmp;
        }
        out.swap(tmp);
        return true;
    }
    // Difference between Windows FILETIME epoch (1601) and NTP epoch (1900) in seconds
    static const uint64_t SEC_1601_TO_1900 = 9435484800ULL;

    inline uint64_t FileTimeTo100ns(const FILETIME &ft)
    {
        ULARGE_INTEGER u{};
        u.LowPart = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        return u.QuadPart;
    }
    inline FILETIME ToFileTime(uint64_t ft100ns)
    {
        ULARGE_INTEGER u{};
        u.QuadPart = ft100ns;
        FILETIME ft{};
        ft.dwLowDateTime = u.LowPart;
        ft.dwHighDateTime = u.HighPart;
        return ft;
    }

    // Convert FILETIME (100ns since 1601) to NTP 64-bit timestamp seconds/fraction
    inline void FileTimeToNtp(const FILETIME &ft, uint32_t &seconds, uint32_t &fraction)
    {
        uint64_t t100 = FileTimeTo100ns(ft);
        uint64_t totalSec = t100 / 10000000ULL; // 10^7 per second
        uint64_t rem100 = t100 % 10000000ULL;
        int64_t ntpSec = (int64_t)totalSec - (int64_t)SEC_1601_TO_1900;
        if (ntpSec < 0)
            ntpSec = 0; // clamp
        seconds = (uint32_t)ntpSec;
        fraction = (uint32_t)((rem100 * (1ULL << 32)) / 10000000ULL);
    }

    // Convert NTP seconds/fraction to FILETIME (100ns since 1601)
    inline uint64_t NtpToFileTime100ns(uint32_t seconds, uint32_t fraction)
    {
        uint64_t totalSec = (uint64_t)seconds + SEC_1601_TO_1900;
        uint64_t base100 = totalSec * 10000000ULL;
        uint64_t frac100 = ((uint64_t)fraction * 10000000ULL) >> 32; // fraction * 1e7 / 2^32
        return base100 + frac100;
    }

    inline void ReadNtpTimestamp(const uint8_t *buf, uint32_t &sec, uint32_t &frac)
    {
        sec = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
        frac = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | (buf[7]);
    }
    inline void WriteNtpTimestamp(uint8_t *buf, uint32_t sec, uint32_t frac)
    {
        buf[0] = (uint8_t)(sec >> 24);
        buf[1] = (uint8_t)(sec >> 16);
        buf[2] = (uint8_t)(sec >> 8);
        buf[3] = (uint8_t)(sec);
        buf[4] = (uint8_t)(frac >> 24);
        buf[5] = (uint8_t)(frac >> 16);
        buf[6] = (uint8_t)(frac >> 8);
        buf[7] = (uint8_t)(frac);
    }

    inline std::wstring LastErrorMessage(DWORD err)
    {
        wchar_t *buf = nullptr;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, err, 0, (LPWSTR)&buf, 0, nullptr);
        std::wstring s = buf ? buf : L"";
        if (buf)
            LocalFree(buf);
        return s;
    }
}

bool CNtpClient::Query(const std::wstring &server, unsigned short port, int version, CNtpResult &out, int timeoutMs)
{
    out = CNtpResult{};
    if (version != 3 && version != 4)
        version = 4;

    EnsureWSA();

    std::vector<AddrEntry> addrs;
    if (!ResolveAddresses(server, port, addrs)) {
        out.Error = L"DNS 解析失败"; return false;
    }

    SOCKET s = INVALID_SOCKET; bool okSocket = false; AddrEntry sel{}; int selIdx = -1;
    for (size_t i = 0; i < addrs.size(); ++i) {
        s = socket(addrs[i].family, SOCK_DGRAM, IPPROTO_UDP);
        if (s != INVALID_SOCKET) { sel = addrs[i]; selIdx = (int)i; okSocket = true; break; }
    }
    if (!okSocket) { out.Error = L"创建套接字失败"; return false; }

    // timeout
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeoutMs, sizeof(timeoutMs));

    uint8_t buf[48]{};
    // LI(0)<<6 | VN(version)<<3 | Mode(3)
    buf[0] = (uint8_t)((0 << 6) | ((version & 0x7) << 3) | 3);

    // Transmit Timestamp = T1 (client send time)
    SYSTEMTIME st1{};
    FILETIME ft1{};
    GetSystemTime(&st1);
    SystemTimeToFileTime(&st1, &ft1);
    uint32_t t1s = 0, t1f = 0;
    FileTimeToNtp(ft1, t1s, t1f);
    WriteNtpTimestamp(buf + 40, t1s, t1f);

    int sent = sendto(s, (const char *)buf, (int)sizeof(buf), 0, (sockaddr*)&sel.ss, sel.len);
    if (sent != sizeof(buf))
    {
        out.Error = L"发送失败";
        closesocket(s);
        return false;
    }

    // Receive
    sockaddr_storage from{};
    int fromlen = sizeof(from);
    int recvd = recvfrom(s, (char *)buf, (int)sizeof(buf), 0, (sockaddr *)&from, &fromlen);
    SYSTEMTIME st4{};
    FILETIME ft4{};
    GetSystemTime(&st4);
    SystemTimeToFileTime(&st4, &ft4);

    if (recvd < 48)
    {
        out.Error = L"接收超时或数据不足";
        closesocket(s);
        return false;
    }

    closesocket(s);

    // Parse T2 (Receive Timestamp), T3 (Transmit Timestamp)
    uint32_t t2s = 0, t2f = 0, t3s = 0, t3f = 0;
    ReadNtpTimestamp(buf + 32, t2s, t2f); // Receive Timestamp
    ReadNtpTimestamp(buf + 40, t3s, t3f); // Transmit Timestamp

    // Convert all to 100ns ticks relative to 1601
    uint64_t T1 = FileTimeTo100ns(ft1);
    uint64_t T2 = NtpToFileTime100ns(t2s, t2f);
    uint64_t T3 = NtpToFileTime100ns(t3s, t3f);
    uint64_t T4 = FileTimeTo100ns(ft4);

    // Compute offset and delay in 100ns
    int64_t offset100 = (int64_t)(((int64_t)T2 - (int64_t)T1) + ((int64_t)T3 - (int64_t)T4)) / 2;
    int64_t delay100 = (int64_t)(((int64_t)T4 - (int64_t)T1) - ((int64_t)T3 - (int64_t)T2));

    out.OffsetMs = offset100 / 10000.0; // 100ns -> ms
    out.DelayMs = delay100 / 10000.0;

    // target = now(UTC) + offset
    uint64_t target100 = (uint64_t)((int64_t)T4 + offset100);
    FILETIME ftt = ToFileTime(target100);
    FileTimeToSystemTime(&ftt, &out.TargetUtc);

    out.Success = true;
    return true;
}

bool CNtpClient::EnableSystemTimePrivilege(std::wstring *err)
{
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        if (err)
            *err = L"OpenProcessToken 失败: " + LastErrorMessage(GetLastError());
        return false;
    }
    LUID luid{};
    if (!LookupPrivilegeValueW(nullptr, SE_SYSTEMTIME_NAME, &luid))
    {
        if (err)
            *err = L"LookupPrivilegeValue 失败: " + LastErrorMessage(GetLastError());
        CloseHandle(hToken);
        return false;
    }
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr))
    {
        if (err)
            *err = L"AdjustTokenPrivileges 失败: " + LastErrorMessage(GetLastError());
        CloseHandle(hToken);
        return false;
    }
    CloseHandle(hToken);
    return true;
}

bool CNtpClient::ApplySystemTimeUtc(const SYSTEMTIME &utc, std::wstring *err)
{
    if (!EnableSystemTimePrivilege(err))
    {
        // 继续尝试调用 SetSystemTime，以便在已提升但未显式启用权限时也能成功
    }
    if (!SetSystemTime(&utc))
    {
        if (err)
            *err = L"SetSystemTime 失败: " + LastErrorMessage(GetLastError());
        return false;
    }
    return true;
}
