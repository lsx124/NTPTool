
// NTPClientDlg.h: 头文件
//

#pragma once

#include "src/Ntp.h"
#include "src/Settings.h"
#include "src/Iec104Master.h"

// 自定义消息
#define WM_104_EVENT (WM_USER + 1)


// CNTPClientDlg 对话框
class CNTPClientDlg : public CDialog
{
// 构造
public:
	CNTPClientDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_NTPCLIENT_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 功能与配置
	CNtpClient   m_ntp;
	CAppSettings m_settings;
	UINT_PTR     m_timerId = 0;

	// IEC 104 Master
	CIec104Master m_iec104;
	bool m_iec104Connected = false;

	// 辅助
	void RestartTimerFromSettings();
	void AppendLog(const std::wstring& s);

	// IEC 104相关方法
	void On104Event(const std::wstring& message);
	void On104ClockReceived(const SYSTEMTIME& clockTime);
	void Update104Statistics();

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedButton1();   // 立即同步
	afx_msg void OnBnClickedCheck1();    // 自动对时
	afx_msg void OnCbnSelchangeCombo1(); // 版本变更
	afx_msg void OnEnKillfocusEdit1();   // 服务器
	afx_msg void OnEnKillfocusEdit2();   // 端口
	afx_msg void OnEnKillfocusEdit3();   // 周期
	
	// IEC 104相关消息处理
	afx_msg void OnBnClicked104Connect();     // 104连接
	afx_msg void OnBnClicked104Disconnect();  // 104断开
	afx_msg void OnBnClicked104GeneralCall(); // 总召
	afx_msg void OnBnClicked104ReadClock();   // 读取时钟
	afx_msg void OnBnClicked104SyncClock();   // 同步时钟
	afx_msg void OnBnClicked104SendS();       // 发送S帧
	afx_msg void OnBnClicked104Test();        // 测试帧
	
	// IEC 104配置相关消息处理
	afx_msg void OnEnKillfocusEdit5();        // 104 IP地址
	afx_msg void OnEnKillfocusEdit6();        // 104端口
	afx_msg void OnEnKillfocusEdit7();        // 公共地址
	afx_msg void OnBnClickedCheck2();         // 104自动连接
	afx_msg void OnBnClickedCheck3();         // 104自动总召
	afx_msg LRESULT On104EventMessage(WPARAM wParam, LPARAM lParam);  // 104事件消息
	DECLARE_MESSAGE_MAP()
};
