#include "pch.h"
#include "Iec104Master.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

CIec104Master::CIec104Master()
    : m_socket(INVALID_SOCKET), m_port(IEC104_DEFAULT_PORT), m_state(Iec104State::DISCONNECTED), m_sendSeqNum(0), m_recvSeqNum(0), m_sentFrames(0), m_receivedFrames(0), m_stopReceive(false)
{
    InitializeWinsock();
}

CIec104Master::~CIec104Master()
{
    Disconnect();
    CleanupWinsock();
}

bool CIec104Master::InitializeWinsock()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return result == 0;
}

void CIec104Master::CleanupWinsock()
{
    WSACleanup();
}

bool CIec104Master::Connect(const std::wstring &ipAddress, WORD port)
{
    if (IsConnected())
    {
        LogEvent(L"已经连接，先断开现有连接");
        Disconnect();
    }

    m_ipAddress = ipAddress;
    m_port = port;
    m_state = Iec104State::CONNECTING;

    // 创建socket
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET)
    {
        LogEvent(L"创建socket失败: " + GetLastErrorString());
        m_state = Iec104State::DISCONNECTED;
        return false;
    }

    // 设置socket超时
    DWORD timeout = IEC104_TIMEOUT_MS;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));

    // 转换IP地址
    std::string ipStr(ipAddress.begin(), ipAddress.end());
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ipStr.c_str(), &serverAddr.sin_addr) <= 0)
    {
        LogEvent(L"无效的IP地址: " + ipAddress);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        m_state = Iec104State::DISCONNECTED;
        return false;
    }

    // 连接服务器
    if (connect(m_socket, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        LogEvent(L"连接失败: " + GetLastErrorString());
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        m_state = Iec104State::DISCONNECTED;
        return false;
    }

    m_state = Iec104State::CONNECTED;
    m_sendSeqNum = 0;
    m_recvSeqNum = 0;
    m_sentFrames = 0;
    m_receivedFrames = 0;

    // 启动接收线程
    m_stopReceive = false;
    m_receiveThread = std::thread(&CIec104Master::ReceiveThreadProc, this);

    LogEvent(L"成功连接到 " + ipAddress + L":" + std::to_wstring(port));
    return true;
}

void CIec104Master::Disconnect()
{
    if (m_socket != INVALID_SOCKET)
    {
        // 停止数据传输
        if (IsStarted())
        {
            StopDataTransfer();
        }

        // 停止接收线程
        m_stopReceive = true;
        if (m_receiveThread.joinable())
        {
            m_receiveThread.join();
        }

        // 关闭socket
        std::lock_guard<std::mutex> lock(m_socketMutex);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    m_state = Iec104State::DISCONNECTED;
    LogEvent(L"连接已断开");
}

bool CIec104Master::InitializeLink()
{
    if (!IsConnected())
    {
        LogEvent(L"未连接，无法初始化链路");
        return false;
    }

    LogEvent(L"开始链路初始化...");

    // 异步发送STARTDT_ACT，不等待响应
    return StartDataTransfer();
}

bool CIec104Master::StartDataTransfer()
{
    if (!IsConnected())
    {
        LogEvent(L"未连接，无法启动数据传输");
        return false;
    }

    bool result = SendUFrame(Iec104UFunction::STARTDT_ACT);
    if (result)
    {
        LogEvent(L"发送STARTDT_ACT");
        // 不阻塞等待，让接收线程处理STARTDT_CON响应
    }

    return result;
}

bool CIec104Master::StopDataTransfer()
{
    if (!IsConnected())
    {
        return false;
    }

    bool result = SendUFrame(Iec104UFunction::STOPDT_ACT);
    if (result)
    {
        LogEvent(L"发送STOPDT_ACT");
        m_state = Iec104State::CONNECTED;
    }

    return result;
}

bool CIec104Master::SendGeneralCall(WORD commonAddr)
{
    if (!IsStarted())
    {
        LogEvent(L"数据传输未启动，无法发送总召");
        return false;
    }

    // 构建总召数据
    BYTE data[IEC104_IOA_LEN + 1];   // IOA + QOI
    memset(data, 0, IEC104_IOA_LEN); // IOA=0 (3字节)
    data[IEC104_IOA_LEN] = 0x14;     // QOI=20(站总召)

    bool result = SendIFrame((BYTE)Iec104TypeId::C_IC_NA_1, 0x06, commonAddr, data, sizeof(data));
    if (result)
    {
        LogEvent(L"发送总召命令");
    }

    return result;
}

bool CIec104Master::SendSFrame()
{
    if (!IsConnected())
    {
        return false;
    }

    BYTE frame[6];
    frame[0] = IEC104_START_BYTE;
    frame[1] = 4; // 长度

    // S帧控制域
    frame[2] = 0x01; // S帧标识
    frame[3] = 0x00;

    WORD recvSeq = m_recvSeqNum.load();
    frame[4] = (recvSeq << 1) & 0xFF;
    frame[5] = (recvSeq >> 7) & 0xFF;

    bool result = SendApdu(frame, sizeof(frame));
    if (result)
    {
        LogEvent(L"发送S帧，接收序号: " + std::to_wstring(recvSeq));
    }

    return result;
}

bool CIec104Master::SendTestFrame()
{
    if (!IsConnected())
    {
        return false;
    }

    bool result = SendUFrame(Iec104UFunction::TESTFR_ACT);
    if (result)
    {
        LogEvent(L"发送测试帧");
    }

    return result;
}

bool CIec104Master::ReadClock(WORD commonAddr)
{
    if (!IsStarted())
    {
        LogEvent(L"数据传输未启动，无法读取时钟");
        return false;
    }

    // 构建时钟读取命令数据
    BYTE data[IEC104_IOA_LEN]; // IOA=0 (3字节)
    memset(data, 0, IEC104_IOA_LEN);

    bool result = SendIFrame((BYTE)Iec104TypeId::C_CS_NA_1, 0x05, commonAddr, data, sizeof(data));
    if (result)
    {
        LogEvent(L"发送时钟读取命令");
    }

    return result;
}

bool CIec104Master::SyncClock(const SYSTEMTIME &time, WORD commonAddr)
{
    if (!IsStarted())
    {
        LogEvent(L"数据传输未启动，无法同步时钟");
        return false;
    }

    // 构建时钟同步数据
    BYTE data[IEC104_IOA_LEN + sizeof(Iec104CP56Time)]; // IOA + CP56Time2a
    memset(data, 0, IEC104_IOA_LEN);                    // IOA=0 (3字节)

    // CP56Time2a时间格式
    Iec104CP56Time cp56Time = SystemTimeToCP56(time);
    memcpy(&data[IEC104_IOA_LEN], &cp56Time, sizeof(cp56Time));

    bool result = SendIFrame((BYTE)Iec104TypeId::C_CS_NA_1, 0x06, commonAddr, data, sizeof(data));
    if (result)
    {
        LogEvent(L"发送时钟同步命令");
    }

    return result;
}

bool CIec104Master::SendApdu(const BYTE *data, int length)
{
    if (m_socket == INVALID_SOCKET)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_socketMutex);
    int sent = send(m_socket, (const char *)data, length, 0);

    if (sent == length)
    {

        // 记录发送的报文
        std::wstring hexData = BytesToHexString(data, length);
        LogEvent(L"发送报文: " + hexData);
        m_sentFrames++;
        return true;
    }

    LogEvent(L"发送数据失败: " + GetLastErrorString());
    return false;
}

bool CIec104Master::SendIFrame(BYTE typeId, BYTE cot, WORD commonAddr, const BYTE *data, int dataLen)
{
    if (!IsConnected())
    {
        return false;
    }

    int totalLen = 6 + 6 + dataLen; // APCI(6) + ASDU头(6) + 数据
    std::vector<BYTE> frame(totalLen);

    // APCI
    frame[0] = IEC104_START_BYTE;
    frame[1] = totalLen - 2; // 除去起始字节和长度字节

    // I帧控制域
    WORD sendSeq = m_sendSeqNum.load();
    WORD recvSeq = m_recvSeqNum.load();

    frame[2] = (sendSeq << 1) & 0xFF;
    frame[3] = (sendSeq >> 7) & 0xFF;
    frame[4] = (recvSeq << 1) & 0xFF;
    frame[5] = (recvSeq >> 7) & 0xFF;

    // ASDU
    frame[6] = typeId;
    frame[7] = 0x01; // VSQ: 单个信息对象
    frame[8] = cot;
    frame[9] = 0x00; // COT高字节
    frame[10] = commonAddr & 0xFF;
    frame[11] = (commonAddr >> 8) & 0xFF;

    // 数据
    if (data && dataLen > 0)
    {
        memcpy(&frame[12], data, dataLen);
    }

    bool result = SendApdu(frame.data(), totalLen);
    if (result)
    {
        m_sendSeqNum = (sendSeq + 1) % 32768;
    }

    return result;
}

bool CIec104Master::SendUFrame(Iec104UFunction function)
{
    BYTE frame[6];
    frame[0] = IEC104_START_BYTE;
    frame[1] = 4; // 长度
    frame[2] = (BYTE)function;
    frame[3] = 0x00;
    frame[4] = 0x00;
    frame[5] = 0x00;

    return SendApdu(frame, sizeof(frame));
}

void CIec104Master::ReceiveThreadProc()
{
    BYTE buffer[1024];

    while (!m_stopReceive && IsConnected())
    {
        SOCKET currentSocket = INVALID_SOCKET;

        // 获取当前socket（短时间持锁）
        {
            std::lock_guard<std::mutex> lock(m_socketMutex);
            currentSocket = m_socket;
        }

        if (currentSocket == INVALID_SOCKET)
            break;

        int received = recv(currentSocket, (char *)buffer, sizeof(buffer), 0);

        if (received > 0)
        {
            m_receivedFrames++;
            ProcessReceivedData(buffer, received);
        }
        else if (received == 0)
        {
            LogEvent(L"连接被远程关闭");
            break;
        }
        else
        {
            int error = WSAGetLastError();
            if (error != WSAETIMEDOUT)
            {
                LogEvent(L"接收数据错误: " + GetLastErrorString());
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool CIec104Master::ProcessReceivedData(const BYTE *buffer, int length)
{

    if (length < 6) // 最小帧长度
    {
        return false;
    }

    if (buffer[0] != IEC104_START_BYTE)
    {
        return false;
    }

    BYTE frameLen = buffer[1];
    if (frameLen != length - 2)
    {
        return false;
    }

    // 判断帧类型
    BYTE control0 = buffer[2];
    auto result = false;
    if ((control0 & 0x01) == 0)
    {
        // I帧
        result = ProcessIFrame(buffer, length);
    }
    else if ((control0 & 0x03) == 0x01)
    {
        // S帧
        result = ProcessSFrame(buffer, length);
    }
    else if ((control0 & 0x03) == 0x03)
    {
        // U帧
        result = ProcessUFrame(buffer, length);
    }

    // 记录接收的报文
    std::wstring hexData = BytesToHexString(buffer, length);
    LogEvent(L"接收报文: " + hexData);

    return result;
}

bool CIec104Master::ProcessIFrame(const BYTE *buffer, int length)
{
    if (length < 12) // I帧最小长度
    {
        return false;
    }

    // 提取序号
    WORD sendSeq = ((WORD)buffer[3] << 7) | (buffer[2] >> 1);
    WORD recvSeq = ((WORD)buffer[5] << 7) | (buffer[4] >> 1);

    // 更新接收序号
    m_recvSeqNum = (m_recvSeqNum.load() + 1) % 32768;

    // 解析ASDU
    BYTE typeId = buffer[6];
    BYTE cot = buffer[8];
    WORD commonAddr = ((WORD)buffer[11] << 8) | buffer[10];

    LogEvent(L"收到I帧，类型: " + std::to_wstring(typeId) +
             L", 原因: " + std::to_wstring(cot) +
             L", 地址: " + std::to_wstring(commonAddr));

    // 解析数据
    if (length > 12)
    {
        ParseAsduData(typeId, cot, &buffer[12], length - 12);
    }

    // 发送确认帧
    SendSFrame();

    return true;
}

bool CIec104Master::ProcessUFrame(const BYTE *buffer, int length)
{
    if (length < 6)
    {
        return false;
    }

    Iec104UFunction function = (Iec104UFunction)buffer[2];

    switch (function)
    {
    case Iec104UFunction::STARTDT_CON:
        LogEvent(L"收到STARTDT_CON，数据传输已启动");
        m_state = Iec104State::STARTED;
        break;

    case Iec104UFunction::STOPDT_CON:
        LogEvent(L"收到STOPDT_CON");
        m_state = Iec104State::CONNECTED;
        break;

    case Iec104UFunction::TESTFR_CON:
        LogEvent(L"收到TESTFR_CON");
        break;

    case Iec104UFunction::TESTFR_ACT:
        LogEvent(L"收到TESTFR_ACT，回复TESTFR_CON");
        SendUFrame(Iec104UFunction::TESTFR_CON);
        break;

    default:
        LogEvent(L"收到未知U帧: " + std::to_wstring((BYTE)function));
        break;
    }

    return true;
}

bool CIec104Master::ProcessSFrame(const BYTE *buffer, int length)
{
    if (length < 6)
    {
        return false;
    }

    WORD recvSeq = ((WORD)buffer[5] << 7) | (buffer[4] >> 1);
    LogEvent(L"收到S帧，接收序号: " + std::to_wstring(recvSeq));

    return true;
}

void CIec104Master::ParseAsduData(BYTE typeId, BYTE cot, const BYTE *data, int dataLen)
{
    switch ((Iec104TypeId)typeId)
    {
    case Iec104TypeId::C_CS_NA_1:
        if (cot == 0x07) // 激活确认
        {
            ParseClockData(L"时钟同步:", data, dataLen);
        }
        else if (cot == 0x05)
        {
            ParseClockData(L"时钟读取:", data, dataLen);
        }
        break;

    case Iec104TypeId::C_IC_NA_1:
        if (cot == 0x07) // 激活确认
        {
            LogEvent(L"总召激活确认");
        }
        else if (cot == 0x0A) // 激活终止
        {
            LogEvent(L"总召激活终止");
        }
        break;

    default:
        // 处理其他数据类型
        LogEvent(L"收到数据，类型: " + std::to_wstring(typeId));
        break;
    }
}

void CIec104Master::ParseClockData(const std::wstring &logPrefix, const BYTE *data, int dataLen)
{
    if (dataLen >= IEC104_IOA_LEN + sizeof(Iec104CP56Time)) // IOA + CP56Time2a
    {
        Iec104CP56Time cp56Time;
        memcpy(&cp56Time, &data[IEC104_IOA_LEN], sizeof(cp56Time));

        SYSTEMTIME sysTime = CP56ToSystemTime(cp56Time);
        
        // 获取当前系统时间
        SYSTEMTIME currentTime;
        GetLocalTime(&currentTime);
        
        // 转换为FILETIME进行时间差计算
        FILETIME ftParsed, ftCurrent;
        SystemTimeToFileTime(&sysTime, &ftParsed);
        SystemTimeToFileTime(&currentTime, &ftCurrent);
        
        // 计算时间差（毫秒）
        ULARGE_INTEGER uliParsed, uliCurrent;
        uliParsed.LowPart = ftParsed.dwLowDateTime;
        uliParsed.HighPart = ftParsed.dwHighDateTime;
        uliCurrent.LowPart = ftCurrent.dwLowDateTime;
        uliCurrent.HighPart = ftCurrent.dwHighDateTime;
        
        // FILETIME的单位是100纳秒，转换为毫秒
        __int64 diffMs = (__int64)(uliCurrent.QuadPart - uliParsed.QuadPart) / 10000;
        
        std::wstring timeStr = logPrefix +
                 std::to_wstring(sysTime.wYear) + L"-" +
                 std::to_wstring(sysTime.wMonth) + L"-" +
                 std::to_wstring(sysTime.wDay) + L" " +
                 std::to_wstring(sysTime.wHour) + L":" +
                 std::to_wstring(sysTime.wMinute) + L":" +
                 std::to_wstring(sysTime.wSecond) + L"." +
                 std::to_wstring(sysTime.wMilliseconds);
        
        // 添加时间差信息
        if (diffMs >= 0)
        {
            timeStr += L" (慢 " + std::to_wstring(diffMs/1000.0) + L"s)";
        }
        else
        {
            timeStr += L" (快 " + std::to_wstring(-diffMs/1000.0) + L"s)";
        }
        
        LogEvent(timeStr);

        if (m_clockCallback)
        {
            m_clockCallback(sysTime);
        }
    }
}

Iec104CP56Time CIec104Master::SystemTimeToCP56(const SYSTEMTIME &st)
{
    Iec104CP56Time cp56 = {};

    // 毫秒和秒的组合（16位）
    cp56.milliseconds = st.wSecond * 1000 + st.wMilliseconds;
    
    // 分钟（6位有效）+ 无效位标志
    cp56.minute = st.wMinute & 0x3F;  // bit0-5为分钟
    
    // 小时（5位有效）
    cp56.hour = st.wHour & 0x1F;      // bit0-4为小时
    
    // 日期（5位）+ 星期几（2位）
    cp56.day = (st.wDay & 0x1F);      // bit0-4为日期
    // 添加星期几信息（1=周一，7=周日）
    if (st.wDayOfWeek == 0) // 周日
        cp56.day |= (7 << 5);
    else
        cp56.day |= (st.wDayOfWeek << 5);  // bit5-6为星期
    
    // 月份（4位有效）
    cp56.month = st.wMonth & 0x0F;    // bit0-3为月份
    
    // 年份（7位有效，相对于2000年）
    cp56.year = (st.wYear >= 2000 ? st.wYear - 2000 : st.wYear) & 0x7F;

    return cp56;
}

SYSTEMTIME CIec104Master::CP56ToSystemTime(const Iec104CP56Time &cp56)
{
    SYSTEMTIME st = {};

    // 从毫秒字段提取秒和毫秒
    st.wMilliseconds = cp56.milliseconds % 1000;
    st.wSecond = cp56.milliseconds / 1000;
    
    // 提取分钟（只取低6位）
    st.wMinute = cp56.minute & 0x3F;
    
    // 提取小时（只取低5位）
    st.wHour = cp56.hour & 0x1F;
    
    // 提取日期（只取低5位）
    st.wDay = cp56.day & 0x1F;
    
    // 提取星期几（bit5-6）
    BYTE dayOfWeek = (cp56.day >> 5) & 0x03;
    st.wDayOfWeek = (dayOfWeek == 7) ? 0 : dayOfWeek;  // 7转换为0（周日）
    
    // 提取月份（只取低4位）
    st.wMonth = cp56.month & 0x0F;
    
    // 提取年份（只取低7位，加上2000）
    st.wYear = (cp56.year & 0x7F) + 2000;

    return st;
}

void CIec104Master::LogEvent(const std::wstring &message)
{
    if (m_eventCallback)
    {
        m_eventCallback(message);
    }
}

std::wstring CIec104Master::GetLastErrorString()
{
    DWORD error = WSAGetLastError();
    wchar_t *buffer = nullptr;

    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&buffer,
        0,
        nullptr);

    std::wstring result;
    if (buffer)
    {
        result = buffer;
        LocalFree(buffer);
    }
    else
    {
        result = L"错误代码: " + std::to_wstring(error);
    }

    return result;
}

std::wstring CIec104Master::BytesToHexString(const BYTE *data, int length)
{
    if (!data || length <= 0)
        return L"";

    std::wstringstream ss;
    ss << std::hex << std::uppercase << std::setfill(L'0');

    for (int i = 0; i < length; ++i)
    {
        if (i > 0)
            ss << L" ";
        ss << std::setw(2) << static_cast<int>(data[i]);
    }

    return ss.str();
}
