using System;
using System.IO;
using System.Xml.Serialization;

namespace WinNTPTool.Services
{
    [Serializable]
    public class AppSettings
    {
        public string Server = "pool.ntp.org";
        public int Port = 123;
        public int PeriodSeconds = 300;
        public bool AutoSync = false;
    }

    public class SettingsService
    {
        private readonly string _path;
        public SettingsService(string path = null)
        {
            _path = path ?? GetDefaultPath();
        }

        public AppSettings Load()
        {
            try
            {
                if (File.Exists(_path))
                {
                    var ser = new XmlSerializer(typeof(AppSettings));
                    using (var fs = File.OpenRead(_path))
                        return (AppSettings)ser.Deserialize(fs);
                }
            }
            catch { }
            return new AppSettings();
        }

        public void Save(AppSettings s)
        {
            try
            {
                var dir = Path.GetDirectoryName(_path);
                if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
                var ser = new XmlSerializer(typeof(AppSettings));
                using (var fs = File.Create(_path))
                    ser.Serialize(fs, s);
            }
            catch { }
        }

        public static string GetDefaultPath()
        {
            var root = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            var dir = Path.Combine(root, "NTPTool");
            return Path.Combine(dir, "settings.xml");
        }
    }
}
