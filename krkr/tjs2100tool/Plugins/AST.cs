using System;
using System.Collections.Generic;
using System.Text;

namespace Artemis
{
    public class AST
    {
        string[] Script;
        Encoding Eco = Encoding.UTF8;
        bool BOOM = false;
        public AST(byte[] Script)
        {
            if (Script[0] == 0xFF && Script[1] == 0xFE)
            {
                BOOM = true;
                byte[] narr = new byte[Script.Length - 2];
                for (int i = 2; i < Script.Length; i++)
                    narr[i - 2] = Script[i];
                this.Script = Eco.GetString(narr).Replace("\r\n", "\n").Split('\n');
                return;
            }
            this.Script = Eco.GetString(Script).Replace("\r\n", "\n").Split('\n');
        }

        public string[] Import()
        {
            List<string> Lines = new List<string>();
            for (int i = 0; i < Script.Length; i++)
            {
                string line = Script[i];

                if (!HasText(line))
                    continue;

                Lines.Add(GetText(line));
            }

            return Lines.ToArray();
        }

        public byte[] Export(string[] Text)
        {
            int Index = 0;
            StringBuilder SB = new StringBuilder();

            foreach (string Line in Script)
            {
                if (!HasText(Line)) {
                    SB.AppendLine(Line);
                    continue;
                }

                SB.AppendLine(SetText(Line, Text[Index++]));
            }

            return Eco.GetBytes(SB.ToString());
        }

        public bool HasText(string Line)
        {
            Line = Line.Trim();
            if (Line.StartsWith("{\"select\"") && Line.Contains("text="))
                return true;

            if (Line.StartsWith("name={\"name\","))
                return true;

            return Line.StartsWith("\"");
        }

        public string GetText(string Line)
        {
            Line = Line.Trim();

            if (Line.StartsWith("{\"select\"") && Line.Contains("text="))
            {
                var Pos = IndexAfterOf(Line, "text=");
                return GetStrAt(Line, Pos);
            }

            if (Line.StartsWith("name={\"name\","))
            {
                var Pos = IndexAfterOf(Line, "name=", 5);
                return GetStrAt(Line, Pos);
            }

            if (!Line.StartsWith("\""))
                throw new Exception("Invalid String Line: " + Line);

            return GetStrAt(Line, 0);
        }

        public string SetText(string Line, string Content)
        {
            Line = Line.Trim();

            if (Line.StartsWith("{\"select\"") && Line.Contains("text="))
            {
                var Pos = IndexAfterOf(Line, "text=");
                return SetStrAt(Line, Pos, Content);
            }

            if (Line.StartsWith("name={\"name\","))
            {
                var Pos = IndexAfterOf(Line, "name=", 5);
                return SetStrAt(Line, Pos, Content);
            }

            if (!Line.StartsWith("\""))
                throw new Exception("Invalid String Line: " + Line);

            return SetStrAt(Line, 0, Content);
        }

        private int IndexAfterOf(string Line, string Content, int StartIndex = 0)
        {
            var Rst = Line.IndexOf(Content, StartIndex);
            if (Rst == -1)
                return Rst;
            return Rst + Content.Length;
        }

        private string GetStrAt(string Line, int At)
        {
            int Len = GetStrLen(Line, At);
            return Unescape(Line.Substring(At + 1, Len - 2));//Remove Quotes
        }

        private string SetStrAt(string Line, int At, string Content)
        {
            int Len = GetStrLen(Line, At);
            var Prefix = Line.Substring(0, At + 1);
            var Sufix = Line.Substring(At + Len - 1);
            return Prefix + Escape(Content) + Sufix;
        }
        private int GetStrLen(string Line, int At)
        {
            char Quote = '"';
            bool Escaped = false;

            for (int i = At; i < Line.Length; i++)
            {
                char Current = Line[i];
                bool IsFirst = i == At;
                bool IsQuote = Current == '"' || Current == '\'';
                bool IsEnd = Quote == Current;
                bool IsEscape = Current == '\\';

                if (IsFirst && IsQuote)
                {
                    Quote = Line[i];
                    continue;
                }

                if (IsEscape && !Escaped)
                {
                    Escaped = true;
                    continue;
                }

                if (Escaped)
                {
                    Escaped = false;
                    continue;
                }

                if (IsEnd)
                    return (i - At) + 1;
            }

            return -1;
        }

        public string Escape(string String)
        {
            StringBuilder SB = new StringBuilder();
            foreach (char c in String)
            {
                if (c == '\n')
                    SB.Append("\\n");
                else if (c == '\\')
                    SB.Append("\\\\");
                else if (c == '\t')
                    SB.Append("\\t");
                else if (c == '\r')
                    SB.Append("\\r");
                else if (c == '"')
                    SB.Append("\\\"");
                else
                    SB.Append(c);
            }
            return SB.ToString();
        }

        public string Unescape(string String)
        {
            StringBuilder SB = new StringBuilder();
            bool Escape = false;
            foreach (char c in String)
            {
                if (c == '\\' & !Escape)
                {
                    Escape = true;
                    continue;
                }
                if (Escape)
                {
                    switch (c.ToString().ToLower()[0])
                    {
                        case '\\':
                            SB.Append('\\');
                            break;
                        case 'n':
                            SB.Append('\n');
                            break;
                        case 't':
                            SB.Append('\t');
                            break;
                        case '"':
                            SB.Append('"');
                            break;
                        case '\'':
                            SB.Append('\'');
                            break;
                        case 'r':
                            SB.Append('\r');
                            break;
                        default:
                            throw new Exception("\\" + c + " Isn't a valid string escape.");
                    }
                    Escape = false;
                }
                else
                    SB.Append(c);
            }

            return SB.ToString();
        }
    }
}
