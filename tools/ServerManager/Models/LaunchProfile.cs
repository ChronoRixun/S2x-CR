using System;
using System.Collections.Generic;

namespace S2x.ServerManager.Models
{
    /// <summary>
    /// A separately installed server package that starts its own servers: a folder holding
    /// server-manager.json, registered in &lt;game&gt;\s2x\launch-profiles.txt. Its entries are
    /// extra modes in the editor, each started by the package's own script.
    /// </summary>
    internal sealed class LaunchProfile
    {
        public string Folder;
        public string Id;          // null when server-manager.json could not be read
        public string Title;
        public readonly List<LaunchEntry> Entries = new List<LaunchEntry>();

        // Entries left out because their start script is not in the folder, by key, so a
        // launch can say which file is missing rather than that the entry is unknown.
        public readonly Dictionary<string, string> MissingScripts = new Dictionary<string, string>(StringComparer.Ordinal);

        /// <summary>Null, or one sentence for the launch profiles dialog.</summary>
        public string Problem;
    }

    /// <summary>One server the package can start: a map, the mode it is offered as, and the command.</summary>
    internal sealed class LaunchEntry
    {
        public string Key;
        public string Game;        // "zombies" or "mp"
        public string Map;
        public string Mode;        // the label in the editor's mode list
        public string Short;       // the rotation row's tag
        public string Start;       // the whole command line, {placeholders} included
        public string Public;      // what {public} becomes when the server advertises
        public string Log;         // relative to the package folder
        public string Script;      // the start script the command runs, relative to the folder

        /// <summary>
        /// How many of the party the package's own players take: the first digit in the mode's
        /// label ("Zombies (with 3 Bots)"), or -1 when the label names none. The profile file has
        /// no separate count, and the label is what the host reads.
        /// </summary>
        public int Bots
        {
            get
            {
                foreach (var c in Mode ?? "") if (c >= '0' && c <= '9') return c - '0';
                return -1;
            }
        }
    }
}
