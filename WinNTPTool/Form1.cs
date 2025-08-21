using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using WinNTPTool.Services;

namespace WinNTPTool
{
    public partial class Form1 : Form
    {
        private readonly NtpClientService _ntp = new NtpClientService();
        private readonly SettingsService _settingsService = new SettingsService();
        private readonly Timer _timer = new Timer();
        private AppSettings _settings;
        public Form1()
        {
            InitializeComponent();
            
            _settings = _settingsService.Load();

            _timer.Interval = Math.Max(5, _settings.PeriodSeconds) * 1000;
            _timer.Tick += (s, e) => SyncNow();
            if (_settings.AutoSync) _timer.Start();

            txtServer.Text = _settings.Server;
            numPort.Value = _settings.Port;
            numPeriod.Value = _settings.PeriodSeconds;
            chkAuto.Checked = _settings.AutoSync;

            //_tray = new NotifyIcon(this.components);
            //_tray.Icon = SystemIcons.Application;
            //_tray.Visible = true;
            //_tray.Text = "WinNTPTool";
            //_tray.ContextMenu = new ContextMenu(new MenuItem[]
            //{
            //    new MenuItem("显示/隐藏", (s,e)=> { if (Visible) Hide(); else { Show(); this.WindowState = FormWindowState.Normal; Activate(); } }),
            //    new MenuItem("立即同步", (s,e)=> SyncNow()),
            //    new MenuItem("切换自动", (s,e)=> { chkAuto.Checked = !chkAuto.Checked; }),
            //    new MenuItem("退出", (s,e)=> { _reallyExit = true; _tray.Visible = false; Close(); })
            //});
            //_tray.DoubleClick += (s,e)=> { if (!Visible) { Show(); this.WindowState = FormWindowState.Normal; Activate(); } else { this.WindowState = FormWindowState.Normal; Activate(); } };
        }

        private void SaveSettings() => _settingsService.Save(_settings);

        private void btnSync_Click(object sender, EventArgs e) => SyncNow();

        private void chkAuto_CheckedChanged(object sender, EventArgs e)
        {
            _settings.AutoSync = chkAuto.Checked;
            if (chkAuto.Checked)
                _timer.Start();
            else 
                _timer.Stop();
            SaveSettings();
        }

        private void numPeriod_ValueChanged(object sender, EventArgs e)
        {
            _settings.PeriodSeconds = (int)numPeriod.Value;
            _timer.Interval = Math.Max(5, _settings.PeriodSeconds) * 1000;
            SaveSettings();
        }

        private void txtServer_TextChanged(object sender, EventArgs e)
        {
            _settings.Server = txtServer.Text;
            SaveSettings();
        }

        private void numPort_ValueChanged(object sender, EventArgs e)
        {
            _settings.Port = (int)numPort.Value;
            SaveSettings();
        }

        private void SyncNow()
        {
            btnSync.Enabled = false;
            lblStatus.Text = "正在同步...";
            try
            {
                var res = _ntp.Query(txtServer.Text, (int)numPort.Value);
                if (!res.Success)
                {
                    lblStatus.Text = "失败: " + res.Error;
                }
                else
                {
                    var targetUtc = DateTime.UtcNow + res.Offset;
                    var set = _ntp.ApplySystemTimeUtc(targetUtc);
                    if (!set.Item1)
                        lblStatus.Text = "查询成功但设置失败: " + set.Item2;
                    else
                        lblStatus.Text = string.Format("时间：{0},同步完成 偏移: {1:F1} ms 延迟: {2:F1} ms", targetUtc.ToString("yyyy/MM/dd HH:mm:ss.fff"), res.Offset.TotalMilliseconds, res.Delay.TotalMilliseconds);
                }
            }
            catch (Exception ex)
            {
                lblStatus.Text = "异常: " + ex.Message;
            }
            finally
            {
                btnSync.Enabled = true;
            }
        }

        //protected override void OnFormClosing(FormClosingEventArgs e)
        //{
        //    if (!_reallyExit && e.CloseReason == CloseReason.UserClosing)
        //    {
        //        e.Cancel = true;
        //        Hide();
        //        return;
        //    }
        //    base.OnFormClosing(e);
        //}

        //protected override void OnResize(EventArgs e)
        //{
        //    base.OnResize(e);
        //    if (WindowState == FormWindowState.Minimized)
        //    {
        //        Hide();
        //    }
        //}
    }
}
