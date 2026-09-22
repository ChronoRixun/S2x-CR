using System;
using System.Collections.Generic;

namespace S2x.ServerManager.Models
{
    internal sealed class RotationEntry
    {
        public string Map;
        public string Gametype;

        public string Label(bool isZombies)
        {
            return GameData.MapName(Map) + " " + GameData.MiddleDot + " " +
                   (isZombies ? "ZM" : GameData.GametypeShort(Gametype));
        }
    }

    /// <summary>
    /// One <game>\s2x\presets\*.json file, written by the PowerShell launcher's Get-CurrentConfig.
    /// A server in this app is a preset plus its port.
    /// </summary>
    internal sealed class ServerPreset
    {
        public string FilePath;
        public string FileName;          // base name, what the launcher's preset box shows
        public string ServerName = "";   // sv_hostname, colour codes included
        public string Mode = "mp";
        public int Port = 27016;
        public int BotFill = 12;
        public string BotNames = "nostalgia";
        public bool SingleRoundDom = true;
        public readonly Dictionary<string, int> ScoreLimits = new Dictionary<string, int>(StringComparer.Ordinal);
        public readonly List<RotationEntry> Rotation = new List<RotationEntry>();

        // Every key as it was read, so a later slice can write the file back without losing
        // anything a newer launcher put there.
        public Dictionary<string, object> Raw = new Dictionary<string, object>(StringComparer.Ordinal);

        public bool IsZombies { get { return string.Equals(Mode, "zombies", StringComparison.OrdinalIgnoreCase); } }

        /// <summary>Player cap. The preset format has no cap key yet; the editor slice adds one.</summary>
        public int PlayerCap
        {
            get
            {
                object raw;
                int cap;
                if (Raw.TryGetValue("playerCap", out raw) && int.TryParse(Convert.ToString(raw), out cap) && cap > 0) return cap;
                return IsZombies ? 4 : 18;
            }
        }

        public string PlainName
        {
            get
            {
                var name = GameData.StripColorCodes(ServerName).Trim();
                return name.Length > 0 ? name : (FileName ?? "Unnamed server");
            }
        }
    }
}
