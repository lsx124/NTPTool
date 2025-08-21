#pragma once
#include <string>
// 软件版本信息
#define APP_NAME        L"NTP客户端 & 104主站"
#define APP_VERSION     L"1.2.1"
#define APP_TITLE       APP_NAME L"  Ver:" APP_VERSION

// 构建完整标题（包含权限状态）
inline std::wstring GetAppTitle(bool isElevated)
{
    std::wstring title = APP_TITLE;
    title += isElevated ? L" (管理员)" : L" (普通用户)";
    return title;
}
