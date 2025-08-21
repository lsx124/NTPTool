#pragma once
#include <windows.h>
#include <string>

struct CNtpResult {
    bool Success = false;
    double OffsetMs = 0.0;   // (T2-T1 + T3-T4)/2 in ms
    double DelayMs = 0.0;    // (T4-T1) - (T3-T2) in ms
    SYSTEMTIME TargetUtc{};  // Suggested UTC time to set
    std::wstring Error;      // Error message if any
};

class CNtpClient {
public:
    // version: 3 or 4; timeoutMs default 3000
    bool Query(const std::wstring& server, unsigned short port, int version, CNtpResult& out, int timeoutMs = 3000);
    bool ApplySystemTimeUtc(const SYSTEMTIME& utc, std::wstring* err = nullptr);

private:
    static bool EnableSystemTimePrivilege(std::wstring* err);
};
