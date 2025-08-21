using System;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;

namespace WinNTPTool.Services
{
    public enum NtpProtocol { UDP }

    public sealed class NtpResult
    {
        public DateTime TransmitTimeUtc { get; set; }
        public DateTime ReceiveTimeUtc { get; set; }
        public DateTime OriginateTimeUtc { get; set; }
        public DateTime DestinationTimeUtc { get; set; }
        public TimeSpan Offset { get; set; }
        public TimeSpan Delay { get; set; }
        public string Server { get; set; }
        public int Port { get; set; }
        public bool Success { get; set; }
        public string Error { get; set; }
    }

    public class NtpClientService
    {
        private static readonly DateTime NtpEpoch = new DateTime(1900, 1, 1, 0, 0, 0, DateTimeKind.Utc);

        public NtpResult Query(string server, int port)
        {
            try
            {
                var endpoint = Resolve(server, port);
                using (var udp = new UdpClient(endpoint.AddressFamily))
                {
                    udp.Client.ReceiveTimeout = 3000;
                    udp.Client.SendTimeout = 3000;

                    var request = new byte[48];
                    request[0] = 0x1B;
                    var t1 = DateTime.UtcNow;
                    WriteTimestamp(request, 40, t1);

                    udp.Send(request, request.Length, endpoint);
                    IPEndPoint remoteEP = null;
                    var resp = udp.Receive(ref remoteEP);
                    var t4 = DateTime.UtcNow;

                    if (resp.Length < 48)
                        return new NtpResult { Success = false, Error = "响应长度不足", Server = server, Port = port };

                    var t2 = ReadTimestamp(resp, 32);
                    var t3 = ReadTimestamp(resp, 40);

                    var delay = (t4 - t1) - (t3 - t2);
                    var offsetTicks = ((t2 - t1).Ticks + (t3 - t4).Ticks) / 2;
                    var offset = new TimeSpan(offsetTicks);

                    return new NtpResult
                    {
                        Success = true,
                        Server = server,
                        Port = port,
                        OriginateTimeUtc = t1,
                        ReceiveTimeUtc = t2,
                        TransmitTimeUtc = t3,
                        DestinationTimeUtc = t4,
                        Delay = delay,
                        Offset = offset
                    };
                }
            }
            catch (Exception ex)
            {
                return new NtpResult { Success = false, Error = ex.Message, Server = server, Port = port };
            }
        }

        private static IPEndPoint Resolve(string host, int port)
        {
            IPAddress ip;
            if (IPAddress.TryParse(host, out ip))
                return new IPEndPoint(ip, port);
            var addresses = Dns.GetHostAddresses(host);
            if (addresses.Length == 0) throw new InvalidOperationException("无法解析服务器: " + host);
            return new IPEndPoint(addresses[0], port);
        }

        private static void WriteTimestamp(byte[] dest, int offset, DateTime utc)
        {
            if (utc.Kind != DateTimeKind.Utc) utc = utc.ToUniversalTime();
            var seconds = (uint)(utc - NtpEpoch).TotalSeconds;
            var tsBase = new DateTime(utc.Year, utc.Month, utc.Day, utc.Hour, utc.Minute, utc.Second, DateTimeKind.Utc);
            var fraction = (uint)((utc - tsBase).TotalSeconds * uint.MaxValue);
            WriteUInt32BE(dest, offset + 0, seconds);
            WriteUInt32BE(dest, offset + 4, fraction);
        }

        private static DateTime ReadTimestamp(byte[] src, int offset)
        {
            var seconds = ReadUInt32BE(src, offset + 0);
            var fraction = ReadUInt32BE(src, offset + 4);
            var fracSeconds = fraction / (double)uint.MaxValue;
            return NtpEpoch.AddSeconds(seconds + fracSeconds);
        }

        private static void WriteUInt32BE(byte[] buffer, int offset, uint value)
        {
            buffer[offset + 0] = (byte)(value >> 24);
            buffer[offset + 1] = (byte)(value >> 16);
            buffer[offset + 2] = (byte)(value >> 8);
            buffer[offset + 3] = (byte)(value);
        }

        private static uint ReadUInt32BE(byte[] buffer, int offset)
        {
            return (uint)(buffer[offset + 0] << 24 | buffer[offset + 1] << 16 | buffer[offset + 2] << 8 | buffer[offset + 3]);
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct SYSTEMTIME
        {
            public ushort Year, Month, DayOfWeek, Day, Hour, Minute, Second, Milliseconds;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetSystemTime(ref SYSTEMTIME st);

        public Tuple<bool, string> ApplySystemTimeUtc(DateTime utc)
        {
            try
            {
                var st = new SYSTEMTIME
                {
                    Year = (ushort)utc.Year,
                    Month = (ushort)utc.Month,
                    Day = (ushort)utc.Day,
                    DayOfWeek = (ushort)utc.DayOfWeek,
                    Hour = (ushort)utc.Hour,
                    Minute = (ushort)utc.Minute,
                    Second = (ushort)utc.Second,
                    Milliseconds = (ushort)utc.Millisecond
                };
                var ok = SetSystemTime(ref st);
                if (!ok) return Tuple.Create(false, "设置系统时间失败，需要管理员权限");
                return Tuple.Create(true, (string)null);
            }
            catch (Exception ex)
            {
                return Tuple.Create(false, ex.Message);
            }
        }
    }
}
