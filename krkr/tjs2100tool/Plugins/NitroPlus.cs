#IMPORT System.Linq.dll
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace NPFilter
{

    public class FullFilter {
        public enum FilterLevel {
            Low, Normal, Max
        }
        private string[] strings;
        private FilterLevel FL;
		
        public FullFilter(string[] Lines, FilterLevel Filter) {
            strings = Lines;
            FL = Filter;
        }
		
		Encoding Enco = Encoding.GetEncoding(932);		
		public FullFilter(byte[] Script) {
            strings = Enco.GetString(Script).Replace("\r\n", "\n").Split('\n');
            FL = FilterLevel.Max;
        }

        private NitroPlusFilter NPF;
        private HighFilter HF;
        private PrefixFilter PF;
		
        public string[] Import() {
            NPF = new NitroPlusFilter(strings);
            string[] Result = NPF.Import();
            if (FL == FilterLevel.Normal || FL == FilterLevel.Max) {
                HF = new HighFilter(Result);
                Result = HF.Split();
            }
            if (FL == FilterLevel.Max) {
                PF = new PrefixFilter(Result);
                Result = PF.Import();
            }
            return Result;
        }

        public byte[] Export(string[] Content) {
            if (FL == FilterLevel.Max)
                Content = PF.Export(Content);
            if (FL == FilterLevel.Normal || FL == FilterLevel.Max)
                Content = HF.Merge(Content);
			
			StringBuilder Compiler = new StringBuilder();
			
			
			Content = NPF.Export(Content);
            for (uint i = 0; i < Content.Length; i++){					
                Compiler.AppendLine(Content[i]);
			}
			
            return Enco.GetBytes(Compiler.ToString());
        }
    }

    internal enum Break {
        No, One, Double
    }
    public class NitroPlusFilter
    {
        string[] FullScript;
        private StrPos[] StrInfo = new StrPos[0];
        public Dictionary<string, string> Tags = new Dictionary<string, string>();
        private Dictionary<uint, Break> Breaks = new Dictionary<uint, Break>();
        public Dictionary<string, bool> TagsBreaks = new Dictionary<string, bool>(); 
        private Dictionary<string, string> SmartTags = new Dictionary<string, string>();
        public NitroPlusFilter(string[] Script) {
            FullScript = Script;
        }

        private const string Start = "<PRE";
        private const string End = "</PRE>";
        public string[] Import() {
            Tags = new Dictionary<string, string>();
            Breaks = new Dictionary<uint, Break>();
            TagsBreaks = new Dictionary<string, bool>();
            SmartTags = new Dictionary<string, string>();
            string[] Content = new string[0];

            uint OpenTag = 0;
            uint CloseTag = 0;
            for (uint i = 0; i < FullScript.Length; i++) {
                string Line = FullScript[i];
                if (Line.StartsWith(Start)) {
                    OpenTag = i;
                    continue;
                }
                if (Line.EndsWith(End)) {
                    CloseTag = i;
                    string Str = GetStr(OpenTag, CloseTag);
                    if (Str.EndsWith("\\t")) {
                        Str = Str.Substring(0, Str.Length - 2);
                        Breaks.Add(i, Break.Double);
                    } else if (Str.EndsWith("\\n")) {
                        Str = Str.Substring(0, Str.Length - 2);
                        Breaks.Add(i, Break.One);
                    } else {
                        Breaks.Add(i, Break.No);
                    }

                    Array.Resize(ref Content, Content.Length + 1);
                    Content[Content.Length - 1] = Str;
                    continue;
                }
            }
            return Content;
        }

        public string[] Export(string[] Content) {
            if (Content.Length != StrInfo.Length)
                throw new Exception("Invalid String Count");

            string[] Result = new string[FullScript.Length];
            FullScript.CopyTo(Result, 0);

            for (uint i = (uint)Content.Length - 1; ; i--) {
                if (Breaks.ContainsKey(i))
                    switch (Breaks[i]) {
                        case Break.Double:
                            Content[i] += "\\t";
                            break;
                        case Break.One:
                            Content[i] += "\\n";
                            break;
                    }
                SetStr(Content[i], StrInfo[i], ref Result);
                
                if (i < int.MaxValue && (int)i - 1 < 0)//For Condition
                    break;
            }

            return Result;
        }

        private void SetStr(string Str, StrPos Pos, ref string[] Script) {
            string[] Start = new string[Pos.Start];
            Array.Copy(Script, Start, Start.Length);

            string[] End = new string[Script.Length - Pos.Ends];
            Array.Copy(Script, (int)Pos.Ends, End, 0, End.Length);

            ParseTags(ref Str);
            string[] Content = Str.Split('\n');
			
			bool InScript = false;
			for (uint i = Pos.Start, x = 0; i < Pos.Ends; i++){
				string Line = Script[i];
				if (Line.StartsWith("[") && Line.EndsWith("]")){
					int Cnt1 = Line.Split('[').Length;
					int Cnt2 = Line.Split(']').Length;
					if (Cnt1 == 2 && Cnt2 == 2){
						InsertAt(ref Content, Line, x++);
						continue;
					}
				}
				if (Line.TrimStart().StartsWith("//")){
					InsertAt(ref Content, Line, x++);
					continue;					
				}
				if (Line.TrimStart().StartsWith("{")){
					if (Line.TrimEnd().EndsWith("}")){
						InsertAt(ref Content, Line, x++);
						continue;
					}
					InScript = true;
					InsertAt(ref Content, Line, x++);
					continue;
				}
				if (Line.TrimEnd().EndsWith("}")) {
					InScript = false;
					InsertAt(ref Content, Line, x++);
					continue;
				}
				if (InScript){
					InsertAt(ref Content, Line, x++);
					continue;
				}
				
				x++;
			}

            //Merge Content
            Script = new string[Start.Length + Content.Length + End.Length];
            Start.CopyTo(Script, 0);
            Content.CopyTo(Script, Start.Length);
            End.CopyTo(Script, Start.Length + Content.Length);
        }

		private void InsertAt(ref string[] Strings, string Value, uint At){
			string[] Start = new string[At];
			string[] End = new string[Strings.Length - At];
			for (uint i = 0; i < At; i++){
				Start[i] = Strings[i];
			}
			for (uint i = 0; i < End.Length; i++){
				End[i] = Strings[i+At];
			}
			Strings = new string[Strings.Length + 1];
			Start.CopyTo(Strings, 0);
			Strings[At] = Value;
			End.CopyTo(Strings, At + 1);
		}
		
        private void ParseTags(ref string Str) {
            Str = Str.Replace("\\n", "\n");
            Str = Str.Replace("\\t", "\n\n");
            string Result = string.Empty;
            string Tag = string.Empty;
            bool InTag = false;
            for (int i = 0; i < Str.Length; i++) {
                if (Str[i] == '<') {
                    InTag = true;
                }
                if (Str[i] == '>') {
                    InTag = false;
                    Tag += '>';
                    if (!Tags.ContainsKey(Tag))
                        throw new Exception(string.Format("\"{0}\" don't is a valid Tag", Tag));
                    Result += Tags[Tag];
                    if (TagsBreaks[Tag])
                        Result += "\n";
                    Tag = string.Empty;
                    continue;
                }
                if (InTag)
                    Tag += Str[i];
                else
                    Result += Str[i];
            }
            Str = Result;
        }

        private string GetStr(uint Open, uint Close) {
            string Str = string.Empty;
			bool InScript = false;
			
            for (uint i = Open + 1; i < Close; i++) {
                bool IsLast = i + 1 >= Close;
                string Line = FullScript[i];
				if (Line.StartsWith("[") && Line.EndsWith("]")){
					int Cnt1 = Line.Split('[').Length;
					int Cnt2 = Line.Split(']').Length;
					if (Cnt1 == 2 && Cnt2 == 2)
						continue;
				}
				if (Line.TrimStart().StartsWith("//")){
					continue;
				}
				if (Line.TrimStart().StartsWith("{")){
					if (Line.TrimEnd().EndsWith("}"))
						continue;
					InScript = true;
					continue;
				}
				if (Line.TrimEnd().EndsWith("}")) {
					InScript = false;
					continue;
				}
				if (InScript)
					continue;
				
				
                if (!IsLast)
                    Str += Line + "\\n";
            }
            //cut the last \\n
            if (!string.IsNullOrEmpty(Str)) {
                Str = Str.Substring(0, Str.Length - 2);
                Str = Str.Replace("\\n\\n", "\\t");
                ClearTags(ref Str);
            }
            RegisterStr(Open + 1, Close - 1);
            return Str;
        }

        private void ClearTags(ref string Str) {
            string Result = string.Empty;
            string Tag = string.Empty;
            bool InTag = false;
            for (int i = 0; i < Str.Length; i++) {
                if (Str[i] == '<') {
                    InTag = true;
                }
                if (Str[i] == '>') {
                    InTag = false;
                    Tag += '>';
                    bool Break = false;
                    if (i + 2 < Str.Length)
                        Break = Str[i + 1] == '\\' && Str[i + 2] == 'n';
                    string SmartTag = RegisterTag(Tag, Break);
                    Result += SmartTag;
                    if (Break)
                        i += 2;
                    Tag = string.Empty;
                    continue;
                }
                if (InTag)
                    Tag += Str[i];
                else
                    Result += Str[i];
            }
            Str = Result;
        }

        private string RegisterTag(string Tag, bool BreakLine) {
            bool Break = false;
            if (SmartTags.ContainsKey(Tag)) {
                string t = SmartTags[Tag];
                if (TagsBreaks[t] == BreakLine)
                    return t;
                else
                    Break = true;
            }
            if (SmartTags.ContainsKey(Tag.Replace(">", " B>")))
                return SmartTags[Tag.Replace(">", " B>")];
            string NewTag = string.Format(Break ? "<{0} B>": "<{0}>", Tags.Count.ToString("X"));
            Tags.Add(NewTag, Tag);
            if (Break)
                Tag = Tag.Replace(">", " B>");
            SmartTags.Add(Tag, NewTag);
            TagsBreaks.Add(NewTag, BreakLine);
            return NewTag;
        }

        private void RegisterStr(uint Start, uint End) {
            Array.Resize(ref StrInfo, StrInfo.Length + 1);
            StrInfo[StrInfo.Length - 1] = new StrPos() {
                Start = Start,
                Ends = End
            };
        }
    }

    public class PrefixFilter {
        private string[] Script;
        private Dictionary<uint, LineInfo> Table;
        public PrefixFilter(string[] HFScript) {
            Script = HFScript;
        }

        public string[] Import() {
            Table = new Dictionary<uint, LineInfo>();
            string[] Result = new string[Script.Length];
            for (uint i = 0; i < Result.Length; i++) {
                string Line = Script[i];
                bool Ready = false;
                LineInfo LI = new LineInfo();
                LI.Pre = PrefixType.None;
				LI.Prefix = string.Empty;
				LI.Sufix = string.Empty;
                int j = 0;
				
				if (Line.StartsWith("\\n")){
					LI.Pre = PrefixType.Start;
					LI.Prefix = "\\n";
					string tl = Line.Substring(2, Line.Length - 2);
					string tp = tl.TrimStart(' ', '　', '\t', '\n', '\r');
					if (tp.Length != tl.Length){
						LI.Prefix += tl.Substring(0, tl.Length - tp.Length);
					}
				}
				
				string tmp = Line.TrimStart(' ', '　', '\t', '\n', '\r');
				if (tmp.Length != Line.Length){
					LI.Prefix += Line.Substring(0, Line.Length - tmp.Length);
					LI.Pre = PrefixType.Start;
				}

                if (Line.StartsWith("<") && !Ready) {
                    string tag = "<";
                    while (j < Line.Length && Line[j] != '>')
                        tag += Line[++j];
                    if (!tag.EndsWith(">")) {//Corrupted Tag
                        Ready = true;
                        LI.Pre = PrefixType.None;
                    }
                    else {

                        LI.Pre = PrefixType.Start;
                        LI.Prefix += tag;
                    }
                }
				
				tmp = Line.TrimEnd(' ', '　', '\t', '\n', '\r');
				if (tmp.Length != Line.Length){
					LI.Sufix = Line.Substring(tmp.Length, Line.Length - tmp.Length);
                    LI.Pre = LI.Pre == PrefixType.Start ? PrefixType.Both : PrefixType.End;
				}
				
                if (Line.EndsWith(">") && j < Line.Length && !Ready) {
                    string tag = "<";
                    j = Line.Length;
                    while (j > 0 && Line[--j] != '<')
                        continue;
                    while (j < Line.Length && Line[j] != '>')
                        tag += Line[++j];
                    if (!tag.EndsWith(">")) {//Corrupted Tag
                        Ready = true;
                    }
                    else {
						if (LI.Pre != PrefixType.Both)
							LI.Pre = LI.Pre == PrefixType.Start ? PrefixType.Both : PrefixType.End;
                        LI.Sufix = tag + LI.Sufix;
                    }
                }
                Table.Add(i, LI);
                if (LI.Pre == PrefixType.Start || LI.Pre == PrefixType.Both)
                    Line = Line.Substring(LI.Prefix.Length, Line.Length - LI.Prefix.Length);
                if (LI.Pre == PrefixType.End || LI.Pre == PrefixType.Both)
                    Line = Line.Substring(0, Line.Length - LI.Sufix.Length);
                Result[i] = Line;
            }
            return Result;
        }

        public string[] Export(string[] Content) {
            string[] Result = Content;
            for (uint i = 0; i < Result.Length; i++) {
                LineInfo LI = Table[i];
                if (LI.Pre == PrefixType.Start || LI.Pre == PrefixType.Both)
                    Result[i] = LI.Prefix + Result[i];
                if (LI.Pre == PrefixType.End || LI.Pre == PrefixType.Both)
                    Result[i] += LI.Sufix;
            }
            return Result;
        }

        private enum PrefixType {
            Start, End, Both, None
        }
        private struct LineInfo {
            public string Prefix;
            public string Sufix;
            public PrefixType Pre;
        }
    }
    public class HighFilter {
        string[] OriginalStrings;
        string[] SplitedStrings;
        Dictionary<uint, uint> ParseIndex = new Dictionary<uint, uint>();
        public HighFilter(string[] Strings) {
            OriginalStrings = Strings;
        }

        bool NoContent = false;
        public string[] Split() {
            NoContent = false;
            string FullString = string.Empty;
            for (uint i = 0, u = 0; i < OriginalStrings.Length; i++) {
                RegisterStr(OriginalStrings[i], ref u, i);
                FullString += OriginalStrings[i] + "\\t";
            }
            if (string.IsNullOrEmpty(FullString)) {
                NoContent = true;
                return OriginalStrings;
            }
            FullString = FullString.Substring(0, FullString.Length - 2);
            FullString = FullString.Replace("\\t", "\t");
            SplitedStrings = FullString.Split('\t');
            return SplitedStrings;
        }

        private void RegisterStr(string str, ref uint Pos, uint id) {
            str = str.Replace("\\t", "\t");
            string[] Strs = str.Split('\t');
            for (uint i = 0; i < Strs.Length; Pos++, i++)
                ParseIndex.Add(Pos, id);
        }

        public string[] Merge(string[] Content) {
            if (NoContent)
                return Content;
            string[] Result = new string[OriginalStrings.Length];
            for (uint i = 0; i < Content.Length; i++) {
                Result[ParseIndex[i]] += Content[i] + "\\t";
            }
            for (uint i = 0; i < Result.Length; i++)
                Result[i] = Result[i].Substring(0, Result[i].Length - 2);
            return Result;
        }

    }
    internal struct StrPos {
        public uint Start;
        public uint Ends;
    }
}
