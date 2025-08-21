
// NTPClientDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "NTPClient.h"
#include "NTPClientDlg.h"
#include "afxdialogex.h"
#include "src/Ntp.h"
#include "src/Settings.h"
#include "src/Version.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum
	{
		IDD = IDD_ABOUTBOX
	};
#endif

protected:
	virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV 支持

	// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// CNTPClientDlg 对话框

CNTPClientDlg::CNTPClientDlg(CWnd *pParent /*=nullptr*/)
	: CDialog(IDD_NTPCLIENT_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	
	// 设置104回调函数
	m_iec104.SetEventCallback([this](const std::wstring& msg) {
		On104Event(msg);
	});
	
	m_iec104.SetClockCallback([this](const SYSTEMTIME& time) {
		On104ClockReceived(time);
	});
}

void CNTPClientDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CNTPClientDlg, CDialog)
ON_WM_SYSCOMMAND()
ON_WM_PAINT()
ON_WM_QUERYDRAGICON()
ON_WM_DESTROY()
ON_WM_TIMER()
ON_BN_CLICKED(IDC_BUTTON1, &CNTPClientDlg::OnBnClickedButton1)
ON_BN_CLICKED(IDC_CHECK1, &CNTPClientDlg::OnBnClickedCheck1)
ON_CBN_SELCHANGE(IDC_COMBO1, &CNTPClientDlg::OnCbnSelchangeCombo1)
ON_EN_KILLFOCUS(IDC_EDIT1, &CNTPClientDlg::OnEnKillfocusEdit1)
ON_EN_KILLFOCUS(IDC_EDIT2, &CNTPClientDlg::OnEnKillfocusEdit2)
ON_EN_KILLFOCUS(IDC_EDIT3, &CNTPClientDlg::OnEnKillfocusEdit3)
ON_BN_CLICKED(IDC_BUTTON2, &CNTPClientDlg::OnBnClicked104Connect)
ON_BN_CLICKED(IDC_BUTTON3, &CNTPClientDlg::OnBnClicked104Disconnect)
ON_BN_CLICKED(IDC_BUTTON4, &CNTPClientDlg::OnBnClicked104GeneralCall)
ON_BN_CLICKED(IDC_BUTTON5, &CNTPClientDlg::OnBnClicked104ReadClock)
ON_BN_CLICKED(IDC_BUTTON6, &CNTPClientDlg::OnBnClicked104SyncClock)
ON_BN_CLICKED(IDC_BUTTON7, &CNTPClientDlg::OnBnClicked104SendS)
ON_BN_CLICKED(IDC_BUTTON8, &CNTPClientDlg::OnBnClicked104Test)
ON_EN_KILLFOCUS(IDC_EDIT5, &CNTPClientDlg::OnEnKillfocusEdit5)
ON_EN_KILLFOCUS(IDC_EDIT6, &CNTPClientDlg::OnEnKillfocusEdit6)
ON_EN_KILLFOCUS(IDC_EDIT7, &CNTPClientDlg::OnEnKillfocusEdit7)
ON_BN_CLICKED(IDC_CHECK2, &CNTPClientDlg::OnBnClickedCheck2)
ON_BN_CLICKED(IDC_CHECK3, &CNTPClientDlg::OnBnClickedCheck3)
ON_MESSAGE(WM_104_EVENT, &CNTPClientDlg::On104EventMessage)
END_MESSAGE_MAP()

// CNTPClientDlg 消息处理程序

BOOL CNTPClientDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu *pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);	 // 设置大图标
	SetIcon(m_hIcon, FALSE); // 设置小图标

	// 载入设置并填充控件
	m_settings.Load();
	
	// 检查管理员权限状态
	BOOL isElevated = FALSE;
	HANDLE hToken = NULL;
	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		TOKEN_ELEVATION elevation;
		DWORD cbSize = sizeof(TOKEN_ELEVATION);
		if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
			isElevated = elevation.TokenIsElevated;
		}
		CloseHandle(hToken);
	}
	
	std::wstring titleStr = GetAppTitle(isElevated);
	CString title = titleStr.c_str();
	SetWindowTextW(title);
	
	SetDlgItemTextW(IDC_EDIT1, m_settings.Server.c_str());
	SetDlgItemInt(IDC_EDIT2, m_settings.Port, FALSE);
	SetDlgItemInt(IDC_EDIT3, m_settings.PeriodSeconds, FALSE);
	CheckDlgButton(IDC_CHECK1, m_settings.AutoSync ? BST_CHECKED : BST_UNCHECKED);
	if (CComboBox *pVer = (CComboBox *)GetDlgItem(IDC_COMBO1))
	{
		if (pVer->GetCount() == 0)
		{
			pVer->AddString(L"NTPv3");
			pVer->AddString(L"NTPv4");
		}
		pVer->SetCurSel(m_settings.Version == 4 ? 1 : 0);
	}
	if (m_settings.AutoSync)
	{
		m_timerId = SetTimer(1, max(5U, m_settings.PeriodSeconds) * 1000, nullptr);
	}

	// 初始化104相关界面
	SetDlgItemTextW(IDC_EDIT5, m_settings.Iec104ServerIP.c_str());  // 使用配置中的IP
	SetDlgItemInt(IDC_EDIT6, m_settings.Iec104Port, FALSE);         // 使用配置中的端口
	SetDlgItemInt(IDC_EDIT7, m_settings.Iec104CommonAddress, FALSE); // 使用配置中的公共地址
	CheckDlgButton(IDC_CHECK2, m_settings.Iec104AutoConnect ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(IDC_CHECK3, m_settings.Iec104AutoGeneralCall ? BST_CHECKED : BST_UNCHECKED);
	
	// 初始状态下104功能按钮禁用
	GetDlgItem(IDC_BUTTON3)->EnableWindow(FALSE);  // 断开按钮
	GetDlgItem(IDC_BUTTON4)->EnableWindow(FALSE);  // 总召按钮
	GetDlgItem(IDC_BUTTON5)->EnableWindow(FALSE);  // 读取时钟
	GetDlgItem(IDC_BUTTON6)->EnableWindow(FALSE);  // 同步时钟
	GetDlgItem(IDC_BUTTON7)->EnableWindow(FALSE);  // 发送S帧
	GetDlgItem(IDC_BUTTON8)->EnableWindow(FALSE);  // 测试帧

	// 如果启用了104自动连接，延迟自动连接
	if (m_settings.Iec104AutoConnect)
	{
		AppendLog(L"104自动连接已启用，1秒后自动连接...");
		SetTimer(2, 1000, nullptr);  // 使用定时器ID 2，1秒后触发自动连接
	}

	return TRUE; // 除非将焦点设置到控件，否则返回 TRUE
}

void CNTPClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CNTPClientDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// 当用户拖动最小化窗口时系统调用此函数取得光标
// 显示。
HCURSOR CNTPClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CNTPClientDlg::OnDestroy()
{
	if (m_timerId)
	{
		KillTimer(m_timerId);
		m_timerId = 0;
	}
	
	// 清理104相关定时器
	KillTimer(2);  // 自动连接定时器
	KillTimer(3);  // 自动总召定时器
	
	// 断开104连接
	if (m_iec104Connected)
	{
		m_iec104.Disconnect();
	}
	
	CDialog::OnDestroy();
}

void CNTPClientDlg::AppendLog(const std::wstring &s)
{
	// 获取现有内容并在过大时截断
	CString content; GetDlgItemTextW(IDC_EDIT4, content);
	if (content.GetLength() > 10240) content.Empty();

	// 构造时间戳前缀 [yyyy-MM-dd HH:mm:ss]
	SYSTEMTIME st{}; GetLocalTime(&st);
	CString line;
	line.Format(L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] %s\r\n",
				st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,st.wMilliseconds,
				s.c_str());

	content += line;
	SetDlgItemTextW(IDC_EDIT4, content);

	// 自动滚动至末尾（若控件为多行编辑框）
	if (CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT4)) {
		int len = pEdit->GetWindowTextLengthW();
		pEdit->SetSel(len, len);
		pEdit->SendMessage(0x00B7 /* EM_SCROLLCARET */, 0, 0);
	}
}

void CNTPClientDlg::RestartTimerFromSettings()
{
	if (m_timerId)
	{
		KillTimer(m_timerId);
		m_timerId = 0;
	}
	if (m_settings.AutoSync)
	{
		m_timerId = SetTimer(1, max(5U, m_settings.PeriodSeconds) * 1000, nullptr);
	}
}

void CNTPClientDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == m_timerId)
	{
		OnBnClickedButton1();
	}
	else if (nIDEvent == 2)  // 104自动连接定时器
	{
		KillTimer(2);  // 只执行一次
		if (m_settings.Iec104AutoConnect && !m_iec104Connected)
		{
			OnBnClicked104Connect();
		}
	}
	else if (nIDEvent == 3)  // 104自动总召定时器
	{
		KillTimer(3);  // 只执行一次
		if (m_iec104.IsStarted())
		{
			OnBnClicked104GeneralCall();
		}
	}
	
	// 定期更新104统计信息
	if (m_iec104Connected)
	{
		Update104Statistics();
	}
	
	CDialog::OnTimer(nIDEvent);
}

void CNTPClientDlg::OnBnClickedButton1()
{
	CString server;
	GetDlgItemTextW(IDC_EDIT1, server);
	UINT port = GetDlgItemInt(IDC_EDIT2);
	int ver = 4;
	if (CComboBox *pVer = (CComboBox *)GetDlgItem(IDC_COMBO1))
	{
		int sel = pVer->GetCurSel();
		ver = (sel == 0) ? 3 : 4;
	}
	CNtpResult res{};
	if (!m_ntp.Query((LPCWSTR)server, (unsigned short)port, ver, res))
	{
		AppendLog(L"失败: " + res.Error);
		return;
	}
	std::wstring err;
	if (!m_ntp.ApplySystemTimeUtc(res.TargetUtc, &err))
	{
		AppendLog(L"查询成功但设置失败: " + err);
		return;
	}
	wchar_t msg[128];
	swprintf_s(msg, L"同步完成 偏移: %.1f ms 延迟: %.1f ms", res.OffsetMs, res.DelayMs);
	AppendLog(msg);
}

void CNTPClientDlg::OnBnClickedCheck1()
{
	m_settings.AutoSync = (IsDlgButtonChecked(IDC_CHECK1) == BST_CHECKED);
	m_settings.Save();
	RestartTimerFromSettings();
}

void CNTPClientDlg::OnCbnSelchangeCombo1()
{
	if (CComboBox *pVer = (CComboBox *)GetDlgItem(IDC_COMBO1))
	{
		int sel = pVer->GetCurSel();
		m_settings.Version = (sel == 0) ? 3 : 4;
		m_settings.Save();
	}
}

void CNTPClientDlg::OnEnKillfocusEdit1()
{
	CString s;
	GetDlgItemTextW(IDC_EDIT1, s);
	m_settings.Server = (LPCWSTR)s;
	m_settings.Save();
}

void CNTPClientDlg::OnEnKillfocusEdit2()
{
	m_settings.Port = (unsigned short)GetDlgItemInt(IDC_EDIT2);
	m_settings.Save();
}

void CNTPClientDlg::OnEnKillfocusEdit3()
{
	m_settings.PeriodSeconds = (unsigned)GetDlgItemInt(IDC_EDIT3);
	if (m_settings.PeriodSeconds < 5)
		m_settings.PeriodSeconds = 5;
	m_settings.Save();
	RestartTimerFromSettings();
}

// IEC 104 消息处理函数

void CNTPClientDlg::OnBnClicked104Connect()
{
	CString ipStr;
	GetDlgItemTextW(IDC_EDIT5, ipStr);
	UINT port = GetDlgItemInt(IDC_EDIT6);

	if (ipStr.IsEmpty())
	{
		AppendLog(L"请输入104服务器IP地址");
		return;
	}

	if (port == 0)
		port = 2404;

	AppendLog(L"正在连接104服务器: " + std::wstring((LPCWSTR)ipStr) + L":" + std::to_wstring(port));

	if (m_iec104.Connect((LPCWSTR)ipStr, (WORD)port))
	{
		m_iec104Connected = true;
		
		// 更新按钮状态
		GetDlgItem(IDC_BUTTON2)->EnableWindow(FALSE);  // 连接按钮
		GetDlgItem(IDC_BUTTON3)->EnableWindow(TRUE);   // 断开按钮
		
		AppendLog(L"104连接成功，正在初始化链路...");
		
		// 异步初始化链路
		if (m_iec104.InitializeLink())
		{
			AppendLog(L"104链路初始化命令发送成功，等待响应...");
			
			// 启用基本功能按钮（等待STARTDT_CON后才启用数据传输相关按钮）
			GetDlgItem(IDC_BUTTON7)->EnableWindow(TRUE);  // 发送S帧
			GetDlgItem(IDC_BUTTON8)->EnableWindow(TRUE);  // 测试帧
			Update104Statistics();
		}
		else
		{
			AppendLog(L"104链路初始化失败");
		}
	}
	else
	{
		AppendLog(L"104连接失败");
	}
}

void CNTPClientDlg::OnBnClicked104Disconnect()
{
	m_iec104.Disconnect();
	m_iec104Connected = false;
	
	// 更新按钮状态
	GetDlgItem(IDC_BUTTON2)->EnableWindow(TRUE);   // 连接按钮
	GetDlgItem(IDC_BUTTON3)->EnableWindow(FALSE);  // 断开按钮
	GetDlgItem(IDC_BUTTON4)->EnableWindow(FALSE);  // 总召按钮
	GetDlgItem(IDC_BUTTON5)->EnableWindow(FALSE);  // 读取时钟
	GetDlgItem(IDC_BUTTON6)->EnableWindow(FALSE);  // 同步时钟
	GetDlgItem(IDC_BUTTON7)->EnableWindow(FALSE);  // 发送S帧
	GetDlgItem(IDC_BUTTON8)->EnableWindow(FALSE);  // 测试帧
	
	AppendLog(L"104已断开连接");
	Update104Statistics();
}

void CNTPClientDlg::OnBnClicked104GeneralCall()
{
	UINT commonAddr = GetDlgItemInt(IDC_EDIT7);
	if (commonAddr == 0)
		commonAddr = 1;
	
	if (m_iec104.SendGeneralCall((WORD)commonAddr))
	{
		AppendLog(L"发送总召命令成功，公共地址: " + std::to_wstring(commonAddr));
	}
	else
	{
		AppendLog(L"发送总召命令失败");
	}
}

void CNTPClientDlg::OnBnClicked104ReadClock()
{
	UINT commonAddr = GetDlgItemInt(IDC_EDIT7);
	if (commonAddr == 0)
		commonAddr = 1;
	
	if (m_iec104.ReadClock((WORD)commonAddr))
	{
		AppendLog(L"发送时钟读取命令，公共地址: " + std::to_wstring(commonAddr));
	}
	else
	{
		AppendLog(L"发送时钟读取命令失败");
	}
}

void CNTPClientDlg::OnBnClicked104SyncClock()
{
	SYSTEMTIME syncTime;
	bool useCustomTime = false;
	
	// 尝试从编辑框获取时间
	CString timeStr;
	GetDlgItemTextW(IDC_EDIT8, timeStr);
	timeStr.Trim();
	
	if (!timeStr.IsEmpty())
	{
		// 解析时间格式：yyyy-MM-dd HH:mm:ss.fff
		int year, month, day, hour, minute, second, millisecond;
		int parsed = swscanf_s(timeStr, L"%d-%d-%d %d:%d:%d.%d",
			&year, &month, &day, &hour, &minute, &second, &millisecond);
		
		if (parsed == 7 && 
			year >= 1900 && year <= 3000 &&
			month >= 1 && month <= 12 &&
			day >= 1 && day <= 31 &&
			hour >= 0 && hour <= 23 &&
			minute >= 0 && minute <= 59 &&
			second >= 0 && second <= 59 &&
			millisecond >= 0 && millisecond <= 999)
		{
			// 时间格式正确，使用编辑框时间
			syncTime.wYear = year;
			syncTime.wMonth = month;
			syncTime.wDay = day;
			syncTime.wHour = hour;
			syncTime.wMinute = minute;
			syncTime.wSecond = second;
			syncTime.wMilliseconds = millisecond;
			syncTime.wDayOfWeek = 0; // 系统会自动计算
			
			useCustomTime = true;
			//AppendLog(L"使用自定义时间进行同步: " + std::wstring((LPCWSTR)timeStr));
		}
		else
		{
			AppendLog(L"时间格式错误，应为 yyyy-MM-dd HH:mm:ss.fff，使用系统时间");
		}
	}
	
	// 如果没有自定义时间或格式错误，使用系统时间
	if (!useCustomTime)
	{
		GetLocalTime(&syncTime);  // 使用本地时间而不是UTC时间
		//AppendLog(L"使用系统时间进行同步");
	}
	
	UINT commonAddr = GetDlgItemInt(IDC_EDIT7);
	if (commonAddr == 0)
		commonAddr = 1;
	
	if (m_iec104.SyncClock(syncTime, (WORD)commonAddr))
	{
		wchar_t syncTimeStr[64];
		swprintf_s(syncTimeStr, L"%04d-%02d-%02d %02d:%02d:%02d.%03d",
			syncTime.wYear, syncTime.wMonth, syncTime.wDay,
			syncTime.wHour, syncTime.wMinute, syncTime.wSecond, syncTime.wMilliseconds);
		
		AppendLog(L"发送时钟同步命令: " + std::wstring(syncTimeStr) + 
				  L", 公共地址: " + std::to_wstring(commonAddr));
	}
	else
	{
		AppendLog(L"发送时钟同步命令失败");
	}
}

void CNTPClientDlg::OnBnClicked104SendS()
{
	if (m_iec104.SendSFrame())
	{
		AppendLog(L"发送S帧成功");
	}
	else
	{
		AppendLog(L"发送S帧失败");
	}
}

void CNTPClientDlg::OnBnClicked104Test()
{
	if (m_iec104.SendTestFrame())
	{
		AppendLog(L"发送测试帧成功");
	}
	else
	{
		AppendLog(L"发送测试帧失败");
	}
}

void CNTPClientDlg::On104Event(const std::wstring& message)
{
	// 在UI线程中更新日志
	std::wstring* pMsg = new std::wstring(L"[104] " + message);
	PostMessage(WM_104_EVENT, 0, (LPARAM)pMsg);
}

void CNTPClientDlg::On104ClockReceived(const SYSTEMTIME& clockTime)
{
	wchar_t timeStr[128];
	swprintf_s(timeStr, L"[104] 收到时钟数据: %04d-%02d-%02d %02d:%02d:%02d.%03d",
		clockTime.wYear, clockTime.wMonth, clockTime.wDay,
		clockTime.wHour, clockTime.wMinute, clockTime.wSecond, clockTime.wMilliseconds);
	
	std::wstring* pMsg = new std::wstring(timeStr);
	PostMessage(WM_104_EVENT, 0, (LPARAM)pMsg);
}

LRESULT CNTPClientDlg::On104EventMessage(WPARAM wParam, LPARAM lParam)
{
	std::wstring* pMsg = (std::wstring*)lParam;
	if (pMsg)
	{
		AppendLog(*pMsg);
		
		// 检查是否是数据传输启动完成的消息
		if (pMsg->find(L"数据传输已启动") != std::wstring::npos)
		{
			// 启用数据传输相关按钮
			GetDlgItem(IDC_BUTTON4)->EnableWindow(TRUE);  // 总召按钮
			GetDlgItem(IDC_BUTTON5)->EnableWindow(TRUE);  // 读取时钟
			GetDlgItem(IDC_BUTTON6)->EnableWindow(TRUE);  // 同步时钟
			
			AppendLog(L"[UI] 104链路初始化完成，功能按钮已启用");
			
			// 如果启用了自动总召，自动执行总召
			if (m_settings.Iec104AutoGeneralCall)
			{
				AppendLog(L"[UI] 自动总召已启用，正在执行总召...");
				// 延迟500ms执行总召，确保状态稳定
				SetTimer(3, 500, nullptr);
			}
		}
		
		delete pMsg;
	}
	
	// 更新统计信息
	Update104Statistics();
	
	return 0;
}

void CNTPClientDlg::Update104Statistics()
{
	if (m_iec104Connected)
	{
		SetDlgItemInt(IDC_STATIC_SENT_FRAMES, m_iec104.GetSentFrames(), FALSE);
		SetDlgItemInt(IDC_STATIC_RECV_FRAMES, m_iec104.GetReceivedFrames(), FALSE);
		SetDlgItemInt(IDC_STATIC_SEND_SEQ, m_iec104.GetSendSeqNum(), FALSE);
		SetDlgItemInt(IDC_STATIC_RECV_SEQ, m_iec104.GetRecvSeqNum(), FALSE);
		
		std::wstring status;
		switch (m_iec104.GetState())
		{
		case Iec104State::DISCONNECTED:
			status = L"未连接";
			break;
		case Iec104State::CONNECTING:
			status = L"连接中...";
			break;
		case Iec104State::CONNECTED:
			status = L"已连接";
			break;
		case Iec104State::STARTED:
			status = L"数据传输中";
			break;
		}
		SetDlgItemTextW(IDC_STATIC_CONN_STATUS, status.c_str());
	}
	else
	{
		SetDlgItemTextW(IDC_STATIC_SENT_FRAMES, L"0");
		SetDlgItemTextW(IDC_STATIC_RECV_FRAMES, L"0");
		SetDlgItemTextW(IDC_STATIC_SEND_SEQ, L"0");
		SetDlgItemTextW(IDC_STATIC_RECV_SEQ, L"0");
		SetDlgItemTextW(IDC_STATIC_CONN_STATUS, L"未连接");
	}
}

// IEC 104配置保存函数

void CNTPClientDlg::OnEnKillfocusEdit5()
{
	CString s;
	GetDlgItemTextW(IDC_EDIT5, s);
	m_settings.Iec104ServerIP = (LPCWSTR)s;
	m_settings.Save();
}

void CNTPClientDlg::OnEnKillfocusEdit6()
{
	UINT port = GetDlgItemInt(IDC_EDIT6);
	if (port == 0) port = 2404;
	m_settings.Iec104Port = (unsigned short)port;
	m_settings.Save();
}

void CNTPClientDlg::OnEnKillfocusEdit7()
{
	UINT addr = GetDlgItemInt(IDC_EDIT7);
	if (addr == 0) addr = 1;
	m_settings.Iec104CommonAddress = (unsigned short)addr;
	m_settings.Save();
}

void CNTPClientDlg::OnBnClickedCheck2()
{
	m_settings.Iec104AutoConnect = (IsDlgButtonChecked(IDC_CHECK2) == BST_CHECKED);
	m_settings.Save();
	
	if (m_settings.Iec104AutoConnect && !m_iec104Connected)
	{
		AppendLog(L"启用104自动连接");
		OnBnClicked104Connect();  // 自动连接
	}
}

void CNTPClientDlg::OnBnClickedCheck3()
{
	m_settings.Iec104AutoGeneralCall = (IsDlgButtonChecked(IDC_CHECK3) == BST_CHECKED);
	m_settings.Save();
	
	if (m_settings.Iec104AutoGeneralCall)
	{
		AppendLog(L"启用104自动总召");
	}
}
