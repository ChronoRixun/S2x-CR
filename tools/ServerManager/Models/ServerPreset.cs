using System;
using System.Collections.Generic;

namespace S2x.ServerManager.Models
{
    internal sealed class RotationEntry
    {
        public string Map;
        public string Gametype;

        // The entry as the file had it. Rows are rebuilt on every save, so without this a key
        // this app does not know would be dropped from the rotation it writes back.
        public Dictionary<string, object> Raw;

        public string Label(bool isZombies)
        {
            return GameData.MapName(Map) + " " + GameData.MiddleDot + " " +
                   (isZombies ? "ZM" : GameData.GametypeShort(Gametype));
        }

        public RotationEntry Copy()
        {
            return new RotationEntry
            {
                Map = Map,
                Gametype = Gametype,
                Raw = (Dictionary<string, object>)ServerPreset.CopyValue(Raw),
            };
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

        // Off the fleet until it is asked for. Still a preset: it keeps its port, its files and
        // its place in the presets folder, and the PowerShell launcher ignores the key.
        public bool Hidden;

        // s2x_autobalance.gsc runs the bots instead of the native one-shot bot_fill: BotFill is
        // then the match size it keeps. Multiplayer only, and never for a launch profile, whose
        // package writes its own cfg.
        public bool AutoBalance;

        // s2x_servercmds.gsc: chat commands (!help, !rules, !discord, !nextmap) and the vote for
        // the next map at match end. Multiplayer only, and never for a launch profile, like
        // auto-balance. Rules and Discord are kept as typed; the cfg gets them cleaned
        // (Services\CfgText.cs) when it is written.
        public bool ChatCommands;
        public readonly List<string> Rules = new List<string>();
        public string Discord = "";
        public bool MapVote;
        public int VoteChoices = DefaultVoteChoices;
        public int VoteSeconds = DefaultVoteSeconds;

        public const int MaxRules = 5;
        public const int DefaultVoteChoices = 3;
        public const int MinVoteChoices = 2;
        public const int MaxVoteChoices = 5;
        public const int DefaultVoteSeconds = 15;
        public const int MinVoteSeconds = 10;
        public const int MaxVoteSeconds = 30;

        // A server a launch profile starts: the profile and the entry in it. Null for a server
        // this app starts itself.
        public string LaunchProfileId;
        public string LaunchEntryKey;

        // Every key as it was read, so writing the file back does not lose anything a newer
        // launcher put there.
        public Dictionary<string, object> Raw = new Dictionary<string, object>(StringComparer.Ordinal);

        public bool IsZombies { get { return string.Equals(Mode, "zombies", StringComparison.OrdinalIgnoreCase); } }
        public bool IsProfile { get { return !string.IsNullOrEmpty(LaunchProfileId) && !string.IsNullOrEmpty(LaunchEntryKey); } }

        /// <summary>Whether a launch of this preset hands its bots to the auto-balance script.</summary>
        public bool UsesAutoBalance { get { return AutoBalance && !IsZombies && !IsProfile; } }

        /// <summary>Whether a launch of this preset turns the chat commands on.</summary>
        public bool UsesChatCommands { get { return ChatCommands && !IsZombies && !IsProfile; } }

        /// <summary>Whether a launch of this preset turns the end-of-match map vote on.</summary>
        public bool UsesMapVote { get { return MapVote && !IsZombies && !IsProfile; } }

        /// <summary>Whether a launch of this preset needs s2x_servercmds.gsc in the game folder.</summary>
        public bool UsesServerCmds { get { return UsesChatCommands || UsesMapVote; } }

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

        /// <summary>
        /// A copy that shares nothing with this one: the launch snapshot, the editor's candidate,
        /// Save as and + New server all change one without the other seeing it.
        /// </summary>
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
                Hidden = Hidden,
                AutoBalance = AutoBalance,
                ChatCommands = ChatCommands,
                Discord = Discord,
                MapVote = MapVote,
                VoteChoices = VoteChoices,
                VoteSeconds = VoteSeconds,
                LaunchProfileId = LaunchProfileId,
                LaunchEntryKey = LaunchEntryKey,
                Raw = (Dictionary<string, object>)CopyValue(Raw) ?? new Dictionary<string, object>(StringComparer.Ordinal),
            };
            foreach (var pair in ScoreLimits) copy.ScoreLimits[pair.Key] = pair.Value;
            foreach (var entry in Rotation) copy.Rotation.Add(entry.Copy());
            copy.ExtraLines.AddRange(ExtraLines);
            copy.Rules.AddRange(Rules);
            return copy;
        }

        /// <summary>
        /// A deep copy of what a preset file holds: nested objects and arrays included, because
        /// writing a preset updates the dictionaries it came from.
        /// </summary>
        public static object CopyValue(object value)
        {
            var map = value as Dictionary<string, object>;
            if (map != null)
            {
                var copy = new Dictionary<string, object>(StringComparer.Ordinal);
                foreach (var pair in map) copy[pair.Key] = CopyValue(pair.Value);
                return copy;
            }
            var list = value as object[];
            if (list != null)
            {
                var copy = new object[list.Length];
                for (int i = 0; i < list.Length; i++) copy[i] = CopyValue(list[i]);
                return copy;
            }
            return value;
        }
    }
}
