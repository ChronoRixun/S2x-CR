using System;
using System.Text;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// Text the host typed, made safe to put between the quotes of a <c>set</c> line in
    /// server-&lt;port&gt;.cfg: the chat commands' rules and Discord line. Nothing blocks a save:
    /// the preset keeps the text as typed, and this runs only when the cfg is written.
    /// <para>
    /// A probe of the real engine's command buffer (B11) found that inside a quoted value ';',
    /// '//' (so http:// and https://), '%', '^' colour codes (a lone one at the end too) and
    /// UTF-8 all arrive byte for byte, with no command split and no formatting. What does not:
    /// a double quote ends the value, a line break ends the command, and a backslash just before
    /// the closing quote escapes it. So the text goes as typed apart from those three
    /// (<see cref="Always"/> and the end of <see cref="Fit"/>), trimmed and cut to the length the
    /// script expects.
    /// </para>
    /// </summary>
    internal static class CfgText
    {
        /// <summary>Longest rule, in UTF-8 bytes.</summary>
        public const int RuleBytes = 120;

        /// <summary>Longest Discord line, in UTF-8 bytes.</summary>
        public const int DiscordBytes = 64;

        /// <summary>The value to write for <paramref name="value"/>, at most <paramref name="maxBytes"/> UTF-8 bytes; "" for nothing.</summary>
        public static string Clean(string value, int maxBytes)
        {
            return Fit(Always(value ?? ""), maxBytes);
        }

        /// <summary>
        /// No control characters (C0, DEL, C1, and the Unicode line and paragraph separators),
        /// which would end the command, and no double quote, which would end the value: it
        /// becomes an apostrophe, as in the admin notices.
        /// </summary>
        public static string Always(string value)
        {
            var text = new StringBuilder(value.Length);
            foreach (var c in value)
            {
                if (c < 0x20 || (c >= 0x7F && c <= 0x9F) || c == (char)0x2028 || c == (char)0x2029) continue;
                text.Append(c == '"' ? '\'' : c);
            }
            return text.ToString();
        }

        /// <summary>
        /// Trimmed, cut to <paramref name="maxBytes"/> UTF-8 bytes on a character boundary, and
        /// never ending in a backslash, which the command buffer would read as an escaped closing
        /// quote. A backslash anywhere else stays.
        /// </summary>
        public static string Fit(string value, int maxBytes)
        {
            var text = value.Trim();
            var bytes = 0;
            var cut = text.Length;
            for (int i = 0; i < text.Length; i++)
            {
                var pair = char.IsHighSurrogate(text[i]) && i + 1 < text.Length && char.IsLowSurrogate(text[i + 1]);
                var size = Encoding.UTF8.GetByteCount(text.Substring(i, pair ? 2 : 1));
                if (bytes + size > maxBytes) { cut = i; break; }
                bytes += size;
                if (pair) i++;
            }
            text = text.Substring(0, cut).TrimEnd();
            while (text.EndsWith("\\", StringComparison.Ordinal)) text = text.Substring(0, text.Length - 1).TrimEnd();
            return text;
        }

        /// <summary>How many UTF-8 bytes <paramref name="value"/> takes once cleaned of what <see cref="Always"/> drops.</summary>
        public static int Bytes(string value)
        {
            return Encoding.UTF8.GetByteCount(Always(value ?? "").Trim());
        }
    }
}
