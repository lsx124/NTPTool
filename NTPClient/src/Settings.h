#pragma once
#include <string>
#include <windows.h>

struct CAppSettings {
    // NTP相关配置
    std::wstring Server = L"time.windows.com";
    unsigned short Port = 123;
    int Version = 4; // 3 或 4
    bool AutoSync = false;
    unsigned int PeriodSeconds = 300; // 最小 5

    // IEC 104相关配置
    std::wstring Iec104ServerIP = L"192.168.1.100";
    unsigned short Iec104Port = 2404;
    unsigned short Iec104CommonAddress = 1;
    bool Iec104AutoConnect = false;
    bool Iec104AutoGeneralCall = false;
    unsigned int Iec104HeartbeatSeconds = 15;

    std::wstring IniPath() const;
    void Load();
    void Save() const;
};
