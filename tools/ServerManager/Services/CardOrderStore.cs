using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

namespace S2x.ServerManager.Services
{
    /// <summary>Presentation state only, scoped to this game's preset directory.</summary>
    internal sealed class CardOrderStore
    {
        public string FilePath { get; private set; }
        public CardOrderStore(string gameDir, string presetDir)
        {
            string scope;
            using (var hash = SHA256.Create())
                scope = BitConverter.ToString(hash.ComputeHash(Encoding.UTF8.GetBytes(
                    PresetStore.Key(presetDir).TrimEnd('\\', '/').ToUpperInvariant()))).Replace("-", "");
            FilePath = Path.Combine(gameDir, "s2x", "server-manager", "card-order-" + scope + ".json");
        }
        public List<string> Load()
        {
            try
            {
                if (!File.Exists(FilePath)) return new List<string>();
                var keys = new JavaScriptSerializer().Deserialize<List<string>>(File.ReadAllText(FilePath));
                return (keys ?? new List<string>()).Where(k => !string.IsNullOrWhiteSpace(k))
                    .Select(PresetStore.Key).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            }
            catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException || ex is ArgumentException || ex is InvalidOperationException)
            { return new List<string>(); } // A damaged preference must not prevent opening the fleet.
        }
        public void Save(IEnumerable<string> keys)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(FilePath));
            var temp = FilePath + "." + Guid.NewGuid().ToString("N") + ".writing";
            try
            {
                File.WriteAllText(temp, new JavaScriptSerializer().Serialize(keys.Select(PresetStore.Key)
                    .Distinct(StringComparer.OrdinalIgnoreCase).ToArray()), new UTF8Encoding(false));
                if (File.Exists(FilePath)) File.Replace(temp, FilePath, null);
                else File.Move(temp, FilePath);
            }
            finally { if (File.Exists(temp)) File.Delete(temp); }
        }
    }
}
