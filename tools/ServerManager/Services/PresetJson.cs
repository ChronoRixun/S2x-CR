using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// Writes a preset file the way the PowerShell launcher's Save-Preset writes it:
    /// ConvertTo-Json under Windows PowerShell, then Set-Content -Encoding UTF8. That is
    /// JavaScriptSerializer's escaping (so an apostrophe comes out '), four spaces of
    /// indent measured from the column the block opened at, CRLF, a closing newline and a
    /// byte order mark. Both launchers write the same preset files, so matching the format
    /// keeps a file from churning every time the other one saves it.
    /// </summary>
    internal static class PresetJson
    {
        private const string Nl = "\r\n";
        private static readonly JavaScriptSerializer Escaper = new JavaScriptSerializer();

        public static void Save(string path, Dictionary<string, object> root)
        {
            var folder = Path.GetDirectoryName(path);
            if (!string.IsNullOrEmpty(folder)) Directory.CreateDirectory(folder);
            File.WriteAllText(path, Write(root), new UTF8Encoding(true));
        }

        public static string Write(Dictionary<string, object> root)
        {
            var text = new StringBuilder();
            Value(text, root, 0);
            text.Append(Nl);                     // Set-Content ends the file with a newline
            return text.ToString();
        }

        /// <summary>One value, starting at column <paramref name="column"/> of the current line.</summary>
        private static void Value(StringBuilder text, object value, int column)
        {
            var map = value as IDictionary<string, object>;
            if (map != null)
            {
                text.Append('{').Append(Nl);
                var first = true;
                foreach (var pair in map)
                {
                    if (!first) text.Append(',').Append(Nl);
                    first = false;
                    var key = Scalar(pair.Key);
                    text.Append(' ', column + 4).Append(key).Append(":  ");
                    // The value starts after the key, its colon and two spaces, and whatever
                    // it opens is measured from there.
                    Value(text, pair.Value, column + 4 + key.Length + 3);
                }
                text.Append(Nl).Append(' ', column).Append('}');
                return;
            }

            var list = value as IEnumerable;
            if (list != null && !(value is string))
            {
                text.Append('[').Append(Nl);
                var first = true;
                foreach (var item in list)
                {
                    if (!first) text.Append(',').Append(Nl);
                    first = false;
                    text.Append(' ', column + 4);
                    Value(text, item, column + 4);
                }
                // An empty list leaves the blank line ConvertTo-Json leaves.
                text.Append(Nl).Append(' ', column).Append(']');
                return;
            }

            text.Append(Scalar(value));
        }

        private static string Scalar(object value)
        {
            if (value == null) return "null";
            if (value is bool) return (bool)value ? "true" : "false";
            if (value is string) return Escaper.Serialize(value);
            if (value is int || value is long || value is short || value is byte)
                return Convert.ToString(value, CultureInfo.InvariantCulture);
            return Escaper.Serialize(value);
        }
    }
}
