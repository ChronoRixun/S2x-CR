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
        public const int MaxStartDelay = 120;

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

        // The editor's keys, alongside the launcher's own in the same file.
        public string BotDifficulty = "regular";
        public int MaxPlayers = 18;
        public int MinPlayers = 1;
        public int StartDelay = 60;
        public bool Advertise = true;
        public bool ShuffleOnLaunch;
        public readonly List<string> ExtraLines = new List<string>();

        // Every key as it was read, so writing the file back does not lose anything a newer
        // launcher put there.
        public Dictionary<string, object> Raw = new Dictionary<string, object>(StringComparer.Ordinal);

        public bool IsZombies { get { return string.Equals(Mode, "zombies", StringComparison.OrdinalIgnoreCase); } }

        /// <summary>The cap the game allows: a Zombies party is four, a multiplayer one eighteen.</summary>
        public static int CapCeiling(bool zombies) { return zombies ? 4 : 18; }

        public string PlainName
        {
            get
            {
                var name = GameData.StripColorCodes(ServerName).Trim();
                return name.Length > 0 ? name : (FileName ?? "Unnamed server");
            }
        }

        /// <summary>A copy that shares nothing with this one, for Save as and + New server.</summary>
        public ServerPreset Copy()
        {
            var copy = new ServerPreset
            {
                FilePath = FilePath,
                FileName = FileName,
                ServerName = ServerName,
                Mode = Mode,
                Port = Port,
                BotFill = BotFill,
                BotNames = BotNames,
                SingleRoundDom = SingleRoundDom,
                BotDifficulty = BotDifficulty,
                MaxPlayers = MaxPlayers,
                MinPlayers = MinPlayers,
                StartDelay = StartDelay,
                Advertise = Advertise,
                ShuffleOnLaunch = ShuffleOnLaunch,
                Raw = new Dictionary<string, object>(Raw, StringComparer.Ordinal),
            };
            foreach (var pair in ScoreLimits) copy.ScoreLimits[pair.Key] = pair.Value;
            foreach (var entry in Rotation) copy.Rotation.Add(new RotationEntry { Map = entry.Map, Gametype = entry.Gametype });
            copy.ExtraLines.AddRange(ExtraLines);
            return copy;
        }
    }
}
