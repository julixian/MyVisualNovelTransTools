#IMPORT System.Linq.dll
#IMPORT System.Core.dll
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace AliceTool {
    public class Dump {
        string[] Script;
		Encoding Eco = Encoding.UTF8;
		bool BOOM = false;
		bool? Include = null;//null = message and string, true = message only, false = string only
		
		Dictionary<int, int> CountMap = new Dictionary<int, int>();
		
        public Dump(byte[] Script) {
			if (Script[0] == 0xFF && Script[1] == 0xFE)
			{
				BOOM = true;
				byte[] narr = new byte[Script.Length-2];
				for (int i = 2; i < Script.Length; i++)
					narr[i-2] = Script[i];
				this.Script = Eco.GetString(narr).Replace("\r\n", "\n").Split('\n');
				return;
			}
            this.Script = Eco.GetString(Script).Replace("\r\n", "\n").Split('\n');
        }

        public string[] Import() {
			CountMap = new Dictionary<int, int>();
			List<string> Lines = new List<string>();
			int LastID = -2;
			int LastCountID = -2;
			for (int i = 0; i < Script.Length; i++){
				string line = Script[i];
				string str = GetText(line);
				
				if (str == null) {
					LastID = -2;
					LastCountID = -2;
					continue;
				}
				
				bool Message = line.StartsWith(";m");
				bool Continue = false;
				
				if (Message){
					int ID = GetID(line);
					if (ID == LastCountID+1) {
						CountMap[LastID]++;
						LastCountID = ID;
						Continue = true;
					} else {
						LastID = LastCountID = ID;
						CountMap[LastID] = 0;
					}
				} else {
					LastID = -2;
					LastCountID = -2;
				}
				
				if (line.StartsWith(";s") && (Include == null || Include == false)) {
					if (!Continue)
						Lines.Add(str);
					else 
						Lines[Lines.Count-1] += "\n" + str;
					continue;
				} else if (line.StartsWith(";m") && (Include == null || Include == true)) {
					if (!Continue)
						Lines.Add(str);
					else 
						Lines[Lines.Count-1] += "\n" + str;
					continue;
				}
			}
            return Lines.ToArray();
        }
		
		int GetID(string Line){
			if (!Line.Contains("=") || !Line.StartsWith(";"))
				return -1;
			
			var Number = Line.Split(']').First().Split('[').Last().Trim();
			
			return int.Parse(Number);
		}
		
		string GetText(string Line){
			if (!Line.Contains("=") || !Line.StartsWith(";"))
				return null;
			
			Line = Line.Substring(Line.IndexOf("=") + 1);
			Line = Line.Trim();
			Line = Line.Substring(1, Line.Length-2);
			
			return Encode(Line, false);
		}
		
        public byte[] Export(string[] Text) {
			StringBuilder SB = new StringBuilder();
			for (int i = 0, x = 0; i < Script.Length; i++){
				string line = Script[i];
				string str = GetText(line);
				int ID = GetID(line);
				
				bool Message = line.StartsWith(";m");
				
				int Merges = 0;
				if (CountMap.ContainsKey(ID) && Message){
					Merges = CountMap[ID];
				}
				
				if (str != null) {
					string NewLine = Text[x++];
					var CurrentLines = NewLine.Split('\n');
					
					if (Merges > 0){
						NewLine = CurrentLines.First();
					}
					
					if (line.StartsWith(";s") && (Include == null || Include == true)) {
						SB.AppendLine(SetText(line, NewLine));
					} else if (line.StartsWith(";m") && (Include == null || Include == true)) {
						SB.AppendLine(SetText(line, NewLine));
					}
					
					int Part = 1;
					while (Merges-- > 0 && i < Script.Length - 1){
						bool Last = Merges == 0;
						
						var NextText = "";
						if (Part < CurrentLines.Length)
							NextText = Last ? string.Join("\n", CurrentLines.Skip(Part).ToArray()) : CurrentLines[Part++];
						
						while (i < Script.Length - 1){
							var NextLine = Script[++i];
							var FinalLine = SetText(NextLine, NextText);
							if (FinalLine == null)
								continue;
							SB.AppendLine(FinalLine);
							break;
						}
					}
					continue;
				}
				SB.AppendLine(line);
			}
			
			return Eco.GetBytes(SB.ToString());
        }
		
		string SetText(string Line, string Text) {
			if (!Line.Contains("=") || !Line.StartsWith(";"))
				return null;
			
			Line = Line.Substring(1, Line.IndexOf("="));
			
			return Line + " \"" + Encode(Text, true) + "\"";
		}
		
		string Encode(string String, bool Enable)
        {
            if (Enable)
            {
                string Result = string.Empty;
                foreach (char c in String)
                {
                    if (c == '\n')
                        Result += "\\n";
                    else if (c == '\\')
                        Result += "\\\\";
                    else if (c == '\t')
                        Result += "\\t";
                    else if (c == '\r')
                        Result += "\\r";
                    else if (c == '"')
						Result += "\\\"";
					else
                        Result += c;
                }
                String = Result;
            }
            else
            {
                string Result = string.Empty;
                bool Special = false;
                foreach (char c in String)
                {
                    if (c == '\\' & !Special)
                    {
                        Special = true;
                        continue;
                    }
                    if (Special)
                    {
                        switch (c.ToString().ToLower()[0])
                        {
                            case '\\':
                                Result += '\\';
                                break;
							case '"':
								Result += '"';
								break;
                            case 'n':
                                Result += '\n';
                                break;
                            case 't':
                                Result += '\t';
                                break;
                            case 'r':
                                Result += '\r';
                                break;
                            default:
                                throw new Exception("\\" + c + " Isn't a valid string escape.");
                        }
                        Special = false;
                    }
                    else
                        Result += c;
                }
                String = Result;
            }

            return String;
        }
    }
}
