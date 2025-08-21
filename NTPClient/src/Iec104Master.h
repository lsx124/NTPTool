#pragma once
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

#pragma comment(lib, "ws2_32.lib")

// IEC 104 协议常量
constexpr BYTE IEC104_START_BYTE = 0x68;
constexpr WORD IEC104_DEFAULT_PORT = 2404;
constexpr int IEC104_IOA_LEN = 3;              // 信息体地址长度（3字节）
constexpr DWORD IEC104_TIMEOUT_MS = 10000;
constexpr DWORD IEC104_HEARTBEAT_MS = 15000;
constexpr DWORD IEC104_T1_TIMEOUT_MS = 15000;  // 发送或测试APDU的超时
constexpr DWORD IEC104_T2_TIMEOUT_MS = 10000;  // 确认收到APDU的超时
constexpr DWORD IEC104_T3_TIMEOUT_MS = 20000;  // 发送测试帧的超时

// IEC 104 APCI类型
enum class Iec104ApciType : BYTE
{
    I_FORMAT = 0x00,  // 信息帧
    S_FORMAT = 0x01,  // 监视帧
    U_FORMAT = 0x03   //未编号帧
};

// IEC 104 U帧功能
enum class Iec104UFunction : BYTE
{
    STARTDT_ACT = 0x07,   // 启动数据传输激活
    STARTDT_CON = 0x0B,   // 启动数据传输确认
    STOPDT_ACT = 0x13,    // 停止数据传输激活
    STOPDT_CON = 0x17,    // 停止数据传输确认
    TESTFR_ACT = 0x43,    // 测试帧激活
    TESTFR_CON = 0x83     // 测试帧确认
};

// IEC 104 ASDU类型标识
enum class Iec104TypeId : BYTE
{
    C_IC_NA_1 = 100,      // 站总召唤命令
    C_CS_NA_1 = 103,      // 时钟同步命令
    M_SP_NA_1 = 1,        // 单点信息
    M_DP_NA_1 = 3,        // 双点信息
    M_ME_NA_1 = 9,        // 测量值，归一化值
    M_ME_NB_1 = 11,       // 测量值，标度化值
    M_ME_NC_1 = 13        // 测量值，短浮点数
};

// IEC 104 ASDU结构
#pragma pack(push, 1)
struct Iec104Apci
{
    BYTE start;           // 启动字符 0x68
    BYTE length;          // APDU长度
    BYTE control[4];      // 控制域
};

struct Iec104Asdu
{
    BYTE typeId;          // 类型标识
    BYTE vsq;             // 可变结构限定词
    BYTE cot;             // 传送原因
    BYTE addr[2];         // 公共地址
};

struct Iec104CP56Time
{
    WORD milliseconds;    // 毫秒和秒的组合：bit0-15为毫秒(0-59999)
    BYTE minute;          // 分钟：bit0-5为分(0-59)，bit6无效，bit7=IV(无效位)
    BYTE hour;            // 小时：bit0-4为时(0-23)，bit5-6无效，bit7=SU(夏令时)
    BYTE day;             // 日：bit0-4为日(1-31)，bit5-6为星期(1-7)，bit7无效
    BYTE month;           // 月：bit0-3为月(1-12)，bit4-7无效
    BYTE year;            // 年：bit0-6为年(0-99，相对于2000年)，bit7无效
};
#pragma pack(pop)

// IEC 104连接状态
enum class Iec104State
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    STARTED
};

// 数据点结构
struct Iec104DataPoint
{
    WORD address;         // 信息对象地址
    BYTE type;           // 数据类型
    BYTE quality;        // 品质描述
    union
    {
        BOOL boolValue;
        WORD wordValue;
        DWORD dwordValue;
        float floatValue;
    } value;
    SYSTEMTIME timestamp;
};

// 104通信结果
struct Iec104Result
{
    bool success = false;
    std::wstring error;
    std::vector<Iec104DataPoint> dataPoints;
    SYSTEMTIME clockValue{};
};

// 事件回调函数类型
using Iec104EventCallback = std::function<void(const std::wstring&)>;
using Iec104DataCallback = std::function<void(const std::vector<Iec104DataPoint>&)>;
using Iec104ClockCallback = std::function<void(const SYSTEMTIME&)>;

class CIec104Master
{
public:
    CIec104Master();
    ~CIec104Master();

    // 连接管理
    bool Connect(const std::wstring& ipAddress, WORD port = IEC104_DEFAULT_PORT);
    void Disconnect();
    bool IsConnected() const { return m_state == Iec104State::CONNECTED || m_state == Iec104State::STARTED; }
    bool IsStarted() const { return m_state == Iec104State::STARTED; }
    Iec104State GetState() const { return m_state; }

    // 协议功能
    bool InitializeLink();          // 链路初始化
    bool StartDataTransfer();       // 启动数据传输
    bool StopDataTransfer();        // 停止数据传输
    bool SendGeneralCall(WORD commonAddr = 1);  // 总召
    bool SendSFrame();              // 发送S帧
    bool SendTestFrame();           // 发送测试帧
    bool ReadClock(WORD commonAddr = 1);        // 读取时钟
    bool SyncClock(const SYSTEMTIME& time, WORD commonAddr = 1);  // 时钟同步

    // 回调设置
    void SetEventCallback(Iec104EventCallback callback) { m_eventCallback = callback; }
    void SetDataCallback(Iec104DataCallback callback) { m_dataCallback = callback; }
    void SetClockCallback(Iec104ClockCallback callback) { m_clockCallback = callback; }

    // 获取统计信息
    DWORD GetSentFrames() const { return m_sentFrames; }
    DWORD GetReceivedFrames() const { return m_receivedFrames; }
    WORD GetSendSeqNum() const { return m_sendSeqNum; }
    WORD GetRecvSeqNum() const { return m_recvSeqNum; }

private:
    // 网络相关
    SOCKET m_socket;
    std::wstring m_ipAddress;
    WORD m_port;
    std::atomic<Iec104State> m_state;

    // 序号管理
    std::atomic<WORD> m_sendSeqNum;
    std::atomic<WORD> m_recvSeqNum;
    
    // 统计信息
    std::atomic<DWORD> m_sentFrames;
    std::atomic<DWORD> m_receivedFrames;

    // 线程和同步
    std::thread m_receiveThread;
    std::atomic<bool> m_stopReceive;
    mutable std::mutex m_socketMutex;
    mutable std::mutex m_seqMutex;

    // 回调函数
    Iec104EventCallback m_eventCallback;
    Iec104DataCallback m_dataCallback;
    Iec104ClockCallback m_clockCallback;

    // 内部方法
    static bool InitializeWinsock();
    static void CleanupWinsock();
    
    bool SendApdu(const BYTE* data, int length);
    bool SendIFrame(BYTE typeId, BYTE cot, WORD commonAddr, const BYTE* data = nullptr, int dataLen = 0);
    bool SendUFrame(Iec104UFunction function);
    //bool SendSFrame();
    
    void ReceiveThreadProc();
    bool ProcessReceivedData(const BYTE* buffer, int length);
    bool ProcessIFrame(const BYTE* buffer, int length);
    bool ProcessUFrame(const BYTE* buffer, int length);
    bool ProcessSFrame(const BYTE* buffer, int length);
    
    void ParseAsduData(BYTE typeId, BYTE cot, const BYTE* data, int dataLen);
    void ParseClockData(const std::wstring& logPrefix, const BYTE* data, int dataLen);

    static Iec104CP56Time SystemTimeToCP56(const SYSTEMTIME& st);
    static SYSTEMTIME CP56ToSystemTime(const Iec104CP56Time& cp56);
    
    void LogEvent(const std::wstring& message);
    std::wstring GetLastErrorString();
    
    // 报文日志辅助函数
    static std::wstring BytesToHexString(const BYTE* data, int length);
};
