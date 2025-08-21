#include "pch.h"
#include "Settings.h"
#include <shlobj.h>
#include <sstream>

static std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}

std::wstring CAppSettings::IniPath() const {
    PWSTR appdata = nullptr;
    std::wstring path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
        path = appdata;
        CoTaskMemFree(appdata);
    }
    path = JoinPath(path, L"NTPClient");
    CreateDirectoryW(path.c_str(), nullptr);
    return JoinPath(path, L"settings.ini");
}

void CAppSettings::Load() {
    auto ini = IniPath();
    wchar_t buf[256]{};
    
    // NTP配置
    GetPrivateProfileStringW(L"NTP", L"Server", Server.c_str(), buf, 256, ini.c_str());
    Server = buf;
    Port = (unsigned short)GetPrivateProfileIntW(L"NTP", L"Port", Port, ini.c_str());
    Version = (int)GetPrivateProfileIntW(L"NTP", L"Version", Version, ini.c_str());
    AutoSync = GetPrivateProfileIntW(L"NTP", L"AutoSync", AutoSync ? 1 : 0, ini.c_str()) != 0;
    PeriodSeconds = (unsigned int)GetPrivateProfileIntW(L"NTP", L"Period", PeriodSeconds, ini.c_str());
    
    // IEC 104配置
    GetPrivateProfileStringW(L"IEC104", L"ServerIP", Iec104ServerIP.c_str(), buf, 256, ini.c_str());
    Iec104ServerIP = buf;
    Iec104Port = (unsigned short)GetPrivateProfileIntW(L"IEC104", L"Port", Iec104Port, ini.c_str());
    Iec104CommonAddress = (unsigned short)GetPrivateProfileIntW(L"IEC104", L"CommonAddress", Iec104CommonAddress, ini.c_str());
    Iec104AutoConnect = GetPrivateProfileIntW(L"IEC104", L"AutoConnect", Iec104AutoConnect ? 1 : 0, ini.c_str()) != 0;
    Iec104AutoGeneralCall = GetPrivateProfileIntW(L"IEC104", L"AutoGeneralCall", Iec104AutoGeneralCall ? 1 : 0, ini.c_str()) != 0;
    Iec104HeartbeatSeconds = (unsigned int)GetPrivateProfileIntW(L"IEC104", L"HeartbeatSeconds", Iec104HeartbeatSeconds, ini.c_str());
    
    // 验证参数有效性
    if (Version != 3 && Version != 4) Version = 4;
    if (PeriodSeconds < 5) PeriodSeconds = 5;
    if (Iec104Port == 0) Iec104Port = 2404;
    if (Iec104CommonAddress == 0) Iec104CommonAddress = 1;
    if (Iec104HeartbeatSeconds < 5) Iec104HeartbeatSeconds = 15;
}

void CAppSettings::Save() const {
    auto ini = IniPath();
    wchar_t buf[32]{};
    
    // 保存NTP配置
    WritePrivateProfileStringW(L"NTP", L"Server", Server.c_str(), ini.c_str());
    _itow_s(Port, buf, 10); WritePrivateProfileStringW(L"NTP", L"Port", buf, ini.c_str());
    _itow_s(Version, buf, 10); WritePrivateProfileStringW(L"NTP", L"Version", buf, ini.c_str());
    WritePrivateProfileStringW(L"NTP", L"AutoSync", AutoSync ? L"1" : L"0", ini.c_str());
    _itow_s((int)PeriodSeconds, buf, 10); WritePrivateProfileStringW(L"NTP", L"Period", buf, ini.c_str());
    
    // 保存IEC 104配置
    WritePrivateProfileStringW(L"IEC104", L"ServerIP", Iec104ServerIP.c_str(), ini.c_str());
    _itow_s(Iec104Port, buf, 10); WritePrivateProfileStringW(L"IEC104", L"Port", buf, ini.c_str());
    _itow_s(Iec104CommonAddress, buf, 10); WritePrivateProfileStringW(L"IEC104", L"CommonAddress", buf, ini.c_str());
    WritePrivateProfileStringW(L"IEC104", L"AutoConnect", Iec104AutoConnect ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"IEC104", L"AutoGeneralCall", Iec104AutoGeneralCall ? L"1" : L"0", ini.c_str());
    _itow_s((int)Iec104HeartbeatSeconds, buf, 10); WritePrivateProfileStringW(L"IEC104", L"HeartbeatSeconds", buf, ini.c_str());
}
