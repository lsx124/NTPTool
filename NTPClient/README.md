# NTPClient (MFC 集成组件)

本目录提供可直接集成到 MFC 对话框项目（如“NTPClient”）的核心功能：
- NTP 客户端（UDP，支持 NTP v3/v4，计算偏移/延迟，设置系统时间）
- 配置持久化（服务器、端口、版本、自动/周期），使用 INI 文件

## 集成步骤（示例：对话框类 CMainDlg）
1) 将 `src` 下文件加入你的工程：
   - `src/NtpClient.h`, `src/NtpClient.cpp`
   - `src/Settings.h`, `src/Settings.cpp`
2) 工程设置：
   - 使用 MFC（动态或静态均可），字符集建议 Unicode
   - 链接库：`Ws2_32.lib`
3) 对话框头文件新增成员：
   ```cpp
   #include "NtpClient.h"
   #include "Settings.h"
   
   class CMainDlg : public CDialogEx {
       // ...
       CNtpClient   m_ntp;
       CAppSettings m_settings;
       UINT_PTR     m_timerId = 0; // 自动对时计时器
       // ...
   };
   ```
4) 在 `OnInitDialog` 中加载配置并刷新控件：
   ```cpp
   BOOL CMainDlg::OnInitDialog() {
       CDialogEx::OnInitDialog();
       m_settings.Load();
       SetDlgItemTextW(IDC_EDIT_SERVER, m_settings.Server);
       SetDlgItemInt(IDC_EDIT_PORT, m_settings.Port, FALSE);
       CComboBox* pVer = (CComboBox*)GetDlgItem(IDC_COMBO_VERSION);
       if (pVer) { pVer->SetCurSel(m_settings.Version == 4 ? 1 : 0); }
       SetDlgItemInt(IDC_EDIT_INTERVAL, m_settings.PeriodSeconds, FALSE);
       CheckDlgButton(IDC_CHECK_AUTO, m_settings.AutoSync ? BST_CHECKED : BST_UNCHECKED);
       if (m_settings.AutoSync) m_timerId = SetTimer(1, max(5U, m_settings.PeriodSeconds) * 1000, nullptr);
       return TRUE;
   }
   ```
5) 手动对时按钮处理：
   ```cpp
   void CMainDlg::OnBnClickedSync() {
       CString server; GetDlgItemTextW(IDC_EDIT_SERVER, server);
       UINT port = GetDlgItemInt(IDC_EDIT_PORT);
       CComboBox* pVer = (CComboBox*)GetDlgItem(IDC_COMBO_VERSION);
       int sel = pVer ? pVer->GetCurSel() : 0; int ver = (sel == 1) ? 4 : 3;
       CNtpResult res{};
       std::wstring err;
       if (!m_ntp.Query((LPCWSTR)server, (unsigned short)port, ver, res)) {
           SetDlgItemTextW(IDC_STATIC_STATUS, res.Error.c_str());
           return;
       }
       if (!m_ntp.ApplySystemTimeUtc(res.TargetUtc, &err)) {
           std::wstring msg = L"查询成功但设置失败: "+err; SetDlgItemTextW(IDC_STATIC_STATUS, msg.c_str());
           return;
       }
       wchar_t msg[128]; swprintf_s(msg, L"同步完成 偏移: %.1f ms 延迟: %.1f ms", res.OffsetMs, res.DelayMs);
       SetDlgItemTextW(IDC_STATIC_STATUS, msg);
   }
   ```
6) 自动对时：勾选/间隔变更时保存配置并重启计时器；在 `OnTimer` 调用按钮逻辑。
   ```cpp
   void CMainDlg::RestartTimerFromSettings() {
       if (m_timerId) { KillTimer(m_timerId); m_timerId = 0; }
       if (m_settings.AutoSync) {
           UINT ms = max(5U, m_settings.PeriodSeconds) * 1000;
           m_timerId = SetTimer(1, ms, nullptr);
       }
   }
   
   void CMainDlg::OnTimer(UINT_PTR nIDEvent) {
       if (nIDEvent == m_timerId) OnBnClickedSync();
       CDialogEx::OnTimer(nIDEvent);
   }
   ```
7) 配置保存：在对应控件事件更新 `m_settings` 并 `m_settings.Save()`，然后 `RestartTimerFromSettings()`。

提示：设置系统时间需要管理员权限（以管理员运行，或在清单中请求 requireAdministrator）。

如需我继续生成 MFC 资源/对话框模板示例，请告诉我你的控件 ID 布局。
