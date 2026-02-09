#IMPORT System.Linq.dll
#IMPORT System.Core.dll
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Text;

namespace KrKrFilter
{
    public class KSFilter
    {
        /*
		KiriKiri Kag Script Filter - By Marcussacana
		This will try filter all text from a ks script, but keep in mind
		the KS have a dynamic format, this filter don't work to any game.
		*/

        //Change the Encoding Here, Eco = Read, Eco2 = Write
        //Sample Encoding:
        //Encoding.UTF8;
        //Encoding.GetEncoding(932); //SJIS
        //Encoding.Unicode; //UTF16
        //Encoding.Default; //Operating System Default Encoding
        private Encoding Eco = Encoding.UTF8;
        private Encoding Eco2 = Encoding.UTF8;

        //Some few games break the line in the source script to break in
        //the game as well, if this is the case keep this enabled.
        private bool AutoMergeLines = false;

        //Auto replace " to special quote characters to prevent escape problems
        private bool SpecialQuotes = true;

        private string[] Lines = new string[0];
        private Dictionary<uint, string> Prefix = new Dictionary<uint, string>();
        private Dictionary<uint, string> Sufix = new Dictionary<uint, string>();
        private Dictionary<uint, string> Prefix2 = new Dictionary<uint, string>();
        private Dictionary<uint, string> Sufix2 = new Dictionary<uint, string>();

        public KSFilter(byte[] Script)
        {
            if (Script[0] == 0xFE && Script[1] == 0xFE && Script[2] == 0x01 && Script[3] == 0xFF && Script[4] == 0xFE)
            {
                Script = Decode(Script);
            }

            string Decoded = Eco.GetString(Script);
            Decoded = Decoded.Replace("\r\n", "\n");
            Lines = Decoded.Replace("\r", "\n").Split('\n');
        }

        public byte[] Decode(byte[] Script)
        {
            byte[] Rst = new byte[Script.Length - 5];
            for (int i = 5; i < Script.Length; i++)
            {
                byte b = Script[i];
                Rst[i - 5] = (byte)((DB((byte)(b >> 4)) << 4) + (DB(b) & 0xFF));
            }
            return Rst;
        }

        private byte DB(byte b)
        {
            byte[] Arr = new byte[] { 0, 2, 1, 3, 8, 10, 9, 11, 4, 6, 5, 7, 12, 14, 13, 15 };
            return Arr[b & 0xF];
        }

        public string[] Import()
        {
            uint ID = 0;
            List<string> Dialogues = new List<string>();
            bool Continue = false;
            bool InScript = false;
            foreach (string Line in Lines)
            {
                if (Line.Trim() == "[iscript]" || Line.Trim() == "@iscript")
                    InScript = true;
                if (Line.Trim() == "[endscript]" || Line.Trim() == "@endscript")
                    InScript = false;
                if (InScript)
                {
                    Continue = false;
                    continue;
                }

                if (IsString(Line))
                {
                    var CurrentLine = LineWork(true, ID++, Line).Replace("[r]", "\n");
                    if (Continue)
                    {
                        Dialogues[Dialogues.Count - 1] += "\n" + CurrentLine;

                        Prefix[ID - 1] = "";
                        Sufix[ID - 1] = "";

                    }
                    else
                    {
                        Dialogues.Add(CurrentLine);
                        Continue = AutoMergeLines;
                    }

                }
                else Continue = false;

                if (ContainsTextOnTag(Line))
                {
                    Dialogues.AddRange(GetTagText(Line));
                    Continue = false;
                }
            }
            return Dialogues.ToArray();
        }

        public byte[] Export(string[] Content)
        {
            string Result = "";
            bool Continue = false;
            bool InScript = false;
            for (uint i = 0, x = 0, z = 0; i < Lines.Length; i++)
            {
                string Line = Lines[i];
                if (Line.Trim() == "[iscript]" || Line.Trim() == "@iscript")
                    InScript = true;
                if (Line.Trim() == "[endscript]" || Line.Trim() == "@endscript")
                    InScript = false;

                if (InScript)
                {
                    Result += Lines[i] + "\r\n";
                    Continue = false;
                    continue;
                }

                string NewLine = null;
                bool IsDiag = IsString(Line);
                if (IsDiag)
                {
                    if (!Continue)
                    {
                        NewLine = LineWork(false, z++, Content[x++].Replace("\n", "[r]")).Replace("\n", "\r\n");
                        Continue = AutoMergeLines;
                    }
                    else
                    {
                        z++;
                        continue;
                    }
                }
                else Continue = false;

                if (ContainsTextOnTag(NewLine ?? Lines[i]))
                {
                    Continue = false;
                    int Count = GetTagText(NewLine ?? Line).Length;
                    Result += SetTagText(NewLine ?? Line, Content.Skip((int)x).Take(Count).ToArray());
                    NewLine = null;
                    x += (uint)Count;
                }
                else if (!IsDiag)
                {
                    Result += Lines[i];
                }

                if (NewLine != null)
                    Result += NewLine;

                Result += "\r\n";
            }
            return Eco2.GetBytes(Result);
        }

        string[] STags = new string[] { "chara_" };//Non Text tags with similar Text Tag Proprieties
        string[] TTag = new string[] { "t", "n", "char", "chara", "actor", "txt", "name", "disp", "text" };
        private bool ContainsTextOnTag(string Line)
        {
            if (Line == string.Empty)
                return false;
            /*if (Line[0] == '@')
				return false;*/
            foreach (string T in STags)
            {
                if (Line.Contains(T))
                    return false;
            }
            foreach (string T in TTag)
            {
                if (Line.Contains(T + "="))
                {
                    return true;
                }
            }
            return false;
        }

        private string[] GetTagText(string Line)
        {
            List<string> Results = new List<string>();
            foreach (string T in STags)
            {
                if (Line.Contains(T))
                    return Results.ToArray();
            }
            foreach (string T in TTag)
            {
                var Value = GetTagPropValue(Line, T);
                if (string.IsNullOrEmpty(Value))
                    continue;
                Results.Add(Value);
            }
            return Results.ToArray();
        }

        private string SetTagText(string Line, string[] Values)
        {
            int i = 0;
            foreach (string T in STags)
            {
                if (Line.Contains(T))
                    return Line;
            }
            foreach (string T in TTag)
            {
                var Value = GetTagPropValue(Line, T);
                if (string.IsNullOrEmpty(Value))
                    continue;
                Line = SetTagPropValue(Line, T, Values[i++]);
            }
            return Line;
        }


        private string GetTagPropValue(string Tag, string Prop)
        {
            if (!Tag.Contains(Prop) || !Tag.Contains(" "))
                return null;
            Tag = Tag.Substring(Tag.IndexOf(" ") + 1);
            while (true)
            {
                if (!Tag.Contains("="))
                    break;

                while (Tag.Split('=').First().Contains(" "))
                    Tag = Tag.Substring(Tag.IndexOf(" ") + 1);

                string CurProp = Tag.Split('=').First().TrimStart();
                bool RightProp = CurProp == Prop;
                Tag = Tag.Substring(Tag.IndexOf("=") + 1);
                string Value = string.Empty;
                if (Tag.First() == '\'' || Tag.First() == '"')
                {
                    char Close = Tag.First();
                    bool Escape = false;
                    while (true)
                    {
                        Tag = Tag.Substring(1);
                        if (Escape)
                        {
                            Escape = false;
                            Value += Tag.First();
                            continue;
                        }
                        if (Tag.First() == '\\')
                        {
                            Escape = true;
                            continue;
                        }
                        if (Tag.First() == Close)
                        {
                            Tag = Tag.Substring(1);
                            break;
                        }
                        Value += Tag.First();
                    }
                }
                else if (Tag.Contains(" "))
                {
                    Value = Tag.Split(' ').First();
                    Tag = Tag.Substring(Tag.IndexOf(" ") + 1);
                }
                else
                {
                    Value = Tag.Split(']').First();
                    Tag = Tag.Substring(Tag.IndexOf("]") + 1);
                }
                if (RightProp)
                    return Value;
            }
            return null;
        }

        private string SetTagPropValue(string Tag, string Prop, string NValue)
        {
            var Possibilities = new string[] {
                string.Format(" {0}={1}", Prop, GetTagPropValue(Tag, Prop)),
                string.Format(" {0}=\"{1}\"", Prop, GetTagPropValue(Tag, Prop)),
                string.Format(" {0}='{1}'", Prop, GetTagPropValue(Tag, Prop)),
            };
            if (SpecialQuotes)
            {
                if (NValue.StartsWith("\""))
                    NValue = "“" + NValue.Substring(1);

                if (NValue.EndsWith("\""))
                    NValue = NValue.Substring(0, NValue.Length - 1) + "”";

                NValue = NValue.Replace(" \"", " “").Replace("\" ", "” ");

                for (int i = 0; i < NValue.Length; i++)
                {
                    char? Last = (i - 1 >= 0) ? (char?)NValue[i - 1] : null;
                    char? Next = (i + 1 < NValue.Length) ? (char?)NValue[i + 1] : null;
                    char Current = NValue[i];
                    if (Current != '"')
                        continue;

                    int QuoteType = 1;

                    if (Last != null && char.IsPunctuation((char)Last))
                        QuoteType = 0;

                    string Part = "";
                    if (Last != null)
                        Part += (char)Last;
                    Part += Current;
                    if (Next != null)
                        Part += (char)Next;

                    NValue = NValue.Replace(Part, Part.Replace("\"", QuoteType == 1 ? "”" : "“"));
                }
            }
            foreach (var Possibility in Possibilities)
            {
                if (!Tag.Contains(Possibility))
                    continue;
                return Tag.Replace(Possibility, " " + Prop + "=" + (NValue.Contains(" ") ? "\"" + NValue.Replace("\"", "\\\"") + "\"" : NValue));
            }
            return Tag;
        }

        private string LineWork(bool Mode, uint ID, string Line, bool While = false)
        {
            string[] Tags = new string[]
            { ";", " ", "[cm]", "[hr]", "[wt]", "[line1]", "[line2]", "[line3]", "[line4]", "[line5]", "[line6]",
            "[line7]", "[line8]", "[line9]", "[r]", "[l]" , "\\", "[SYSTEM_MENU_ON_OPENING]",
            "[SYSTEM_MENU_ON]", "[FUNC_LOAD_PLUGIN]", "[plc]", "[style pitch=-1]", "[resetstyle]",
            "―", "　", "[endlink]", "[np]", "[T_NEXT]", "[c]", "[>>]", "[<<]", "地" };

            string ResultLine = Line;
            if (Mode)
            {
                if (ResultLine.Contains("[") && ResultLine.Contains("]"))
                {
                    string New = string.Empty;
                    string Tag = string.Empty;
                    bool InTag = false;
                    bool IsFurigana = false;
                    for (int i = 0; i < ResultLine.Length; i++)
                    {
                        char c = ResultLine[i];
                        if (c == '[')
                            InTag = true;
                        if (c == ']')
                        {
                            Tag += ']';
                            if (IsFurigana)
                                Tag = Tag.Trim('[', ']').Split('\'')[0];//0 = kanji, 1 = furigana
                            InTag = false;
                            IsFurigana = false;
                            New += Tag;
                            Tag = string.Empty;
                            continue;
                        }
                        if (c == '\'' && InTag)
                            IsFurigana = true;

                        if (InTag)
                            Tag += c;
                        else
                            New += c;
                    }
                    ResultLine = New;
                }

                string[] DynTags = new string[] { "[charaname", "[link", "[cname" };
                foreach (string DynTag in DynTags)
                {
                    if (ResultLine.Trim().StartsWith(DynTag))
                    {
                        if (!Prefix.ContainsKey(ID))
                            Prefix[ID] = string.Empty;

                        int Bef = ResultLine.IndexOf("]") + 1;
                        Prefix[ID] += ResultLine.Substring(0, Bef);
                        ResultLine = ResultLine.Substring(Bef, ResultLine.Length - Bef);
                    }
                }

                for (uint i = 0; i < Tags.Length; i++)
                {
                    if (ResultLine.StartsWith(Tags[i]))
                    {
                        if (!Prefix.ContainsKey(ID))
                            Prefix[ID] = string.Empty;
                        Prefix[ID] += Tags[i];
                        ResultLine = ResultLine.Substring(Tags[i].Length, ResultLine.Length - Tags[i].Length);
                    }
                    if (ResultLine.EndsWith(Tags[i]))
                    {
                        if (!Sufix.ContainsKey(ID))
                            Sufix[ID] = string.Empty;
                        Sufix[ID] = Tags[i] + Sufix[ID];
                        ResultLine = ResultLine.Substring(0, ResultLine.Length - Tags[i].Length);
                    }
                }

                while (ResultLine.EndsWith("]") && !ResultLine.EndsWith("\\]") && ResultLine.Contains("["))
                {
                    if (!Sufix.ContainsKey(ID))
                        Sufix[ID] = string.Empty;
                    Sufix[ID] = Sufix[ID] + ResultLine.Substring(ResultLine.LastIndexOf("["), ResultLine.Length - ResultLine.LastIndexOf("["));
                    ResultLine = ResultLine.Substring(0, ResultLine.LastIndexOf("["));
                }

                while (ResultLine.StartsWith("[") && !ResultLine.EndsWith("\\[") && ResultLine.Contains("]"))
                {
                    if (!Prefix.ContainsKey(ID))
                        Prefix[ID] = string.Empty;
                    Prefix[ID] += ResultLine.Substring(0, ResultLine.IndexOf("]") + 1);
                    ResultLine = ResultLine.Substring(ResultLine.IndexOf("]") + 1, (ResultLine.Length - ResultLine.IndexOf("]")) - 1);
                }

            }
            else
            {
                if (Prefix.ContainsKey(ID))
                    ResultLine = Prefix[ID] + ResultLine;
                if (Sufix.ContainsKey(ID))
                    ResultLine = ResultLine + Sufix[ID];
            }
            while (!While && Mode)
            {
                string L = LineWork(true, ID, ResultLine, true);
                if (ResultLine == L)
                {
                    ResultLine = L;
                    break;
                }
                else
                {
                    ResultLine = L;
                }
            }
            return ResultLine;
        }

        private string TrimTags(string Line)
        {
            while (Line.EndsWith("]") && !Line.EndsWith("\\]") && Line.Contains("["))
            {
                Line = Line.Substring(0, Line.LastIndexOf("["));
            }

            while (Line.StartsWith("[") && !Line.EndsWith("\\[") && Line.Contains("]"))
            {
                Line = Line.Substring(Line.IndexOf("]") + 1, (Line.Length - Line.IndexOf("]")) - 1);
            }

            return Line;
        }

        private bool IsString(string Line)
        {
            Line = Line.Trim(' ', '/', '\\', '\n', '\r', '.', '\t');
            if (string.IsNullOrEmpty(Line) || Line.Length < 3)
                return false;
            char Start = Line[0];
            bool TagTest = false;
            if (Line.StartsWith("["))
            {
                for (int i = 0, c = 0; i < Line.Length; i++)
                {
                    char l = Line[i];
                    if (l == ']')
                        c--;
                    if (l == '[')
                        c++;
                    if (c == 0 && i + 1 == Line.Length)
                    {
                        TagTest = true;
                        break;
                    }
                    else if (c == 0 && i + 1 < Line.Length)
                    {
                        if (Line[i + 1] == '[')
                        {
                            c = 0;
                            continue;
                        }
                        break;
                    }
                }
            }

            return !(Start == '@' || Start == '*' || Start == ';' || Start == 0xFEFF || Start == '【' || TagTest /*|| (TrimTags(Line).Contains("[") && TrimTags(Line).Contains("="))*/); //It's Right?
        }

    }
}

