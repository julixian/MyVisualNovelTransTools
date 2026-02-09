#IMPORT System.Linq.dll
#IMPORT System.Core.dll
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace TXT {
    public class PIXSTUDIO {
        string[] Script;
		Encoding Eco = Encoding.GetEncoding(932);
		bool BOOM = false;
		const string HackyDialogPrefix = "<font></font>";
		
        public PIXSTUDIO(byte[] Script) {
            this.Script = Eco.GetString(Script).Replace("\r\n", "\n").Split('\n');
        }

        public string[] Import() {
			List<string> Lines = new List<string>();
			string Str = string.Empty;
			bool InDiag = false;
			for (int i = 0; i < Script.Length; i++){
				string line = Script[i];
				if (IsDialog(line)){
					InDiag = true;
					
					if (line.StartsWith(HackyDialogPrefix))
						line = line.Replace(HackyDialogPrefix, "");
					
					Str += line + "\\n";
					continue;
				}
				if (InDiag){
					InDiag = false;
					Lines.Add(Str.Substring(0, Str.Length-2));
					Str = string.Empty;
				}
				
				if (IsName(line))
					Lines.Add(GetName(line));
			}
			if (Str != string.Empty)
				Lines.Add(Str);
            return Lines.ToArray();
        }
		
		public bool IsDialog(string Line){
			if (Line.StartsWith("<"))
				return true;
			
			if (Line.StartsWith("＠"))
				return false;
			
			return IsSJIS(GetFirstByte(Line));
		}
	
		public bool IsSJIS(byte Byte){
			if (Byte < 0x80)
				return false;
			if (Byte < 0xA0)
				return true;
			if (Byte < 0xE0)
				return false;
			return true;
		}
		
		public bool IsName(string Line){
			return Line.StartsWith("＠");
		}
		
		public string GetName(string Line){
			return Line.Substring(1).Split(',').First();
		}
		
		public string SetName(string Name, string Line){
			int SufixBegin = Line.IndexOf(",");
			string Sufix = string.Empty;
			if (SufixBegin >= 0)
				Sufix = Line.Substring(SufixBegin);
			return "＠" + Name.Replace(",", "，") + Sufix;
		}
	
		public char[] CharsAfter(string Line, char Char){
			List<char> Chars = new List<char>();
			bool CatchNext = false;
			foreach (char c in Line){
				if (CatchNext && c != Char){//...
					Chars.Add(c);
					CatchNext = false;
					continue;
				}
				if (c == Char)
					CatchNext = true;
			}
			return Chars.ToArray();
		}
		
		public void AppendArray<T>(ref T[] Array, T Value){
			T[] Output = new T[Array.Length+1];
			Array.CopyTo(Output, 0);
			Output[Array.Length] = Value;
			Array = Output;
		}

        public byte[] Export(string[] Text) {
			StringBuilder SB = new StringBuilder();
			int ID = 0;
			bool InDiag = false;
			foreach (string Line in Script){
				if (IsDialog(Line)){
					InDiag = true;
					continue;
				}
				if (InDiag){
					InDiag = false;
					SB.AppendLine(ParseDialog(Text[ID++]));
					SB.AppendLine(Line);
				} else if (IsName(Line))
					SB.AppendLine(SetName(Text[ID++], Line));
				else
					SB.AppendLine(Line);
			}
			return Eco.GetBytes(SB.ToString());
        }
		
		public byte GetFirstByte(string Line){
			if (Line.Length == 0)
				return 0;
			
			var Begin = Eco.GetBytes(Line);
			return Begin.First();
		}
		
		public string ParseDialog(string Dialog){
			Dialog = Dialog.Replace("\\n", "\n");
			string[] Lines = Dialog.Split('\n');
			
			//Hacky Fix to begin lines with ASCII characters
			//Usually this is only needed in the first line of every dialogue
			//But if in the others line, if the line starts with " for example
			//we must put the HackyDialogPrefix as well, since isn't only the "
			//that have this problem, and I'm with lazy to list all characters
			//I just applied in all lines and work like a charm :)
			for (int i = 0; i < Lines.Length; i++){
				var FirstByte = GetFirstByte(Lines[i]);
				if (!IsSJIS(FirstByte) && FirstByte != 0x3C && FirstByte != 0)
					Lines[i] = HackyDialogPrefix + Lines[i];
			}
			
			Dialog = string.Join("\r\n", Lines);
			
			return Dialog;
		}

    }
}
