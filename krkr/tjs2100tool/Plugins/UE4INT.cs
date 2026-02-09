#IMPORT System.Linq.dll
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace INI {
    public class UE {
        string[] Script;
		Encoding Eco = Encoding.GetEncoding(932);
		bool BOOM = false;
        public UE(byte[] Script) {
			if (Script[0] == 0xFF && Script[1] == 0xFE){
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
			List<string> Lines = new List<string>();
			for (uint i = 0; i < Script.Length; i += 1){
				string Line = Script[i];
				if (!IsStr(Line))
					continue;
				Line = GetLine(Line);
				Lines.Add(Line.Replace("\\\"", "\"").Replace("/n", "\n"));				
			}
            return Lines.ToArray();
        }
		
		public int GetStrIndex(string Line){
			int StartIndex = -1;
			if (Line.ToLower().Contains("desc=")){
				StartIndex = Line.ToLower().IndexOf("desc=") + "desc=".Length;				
			}
			if (Line.ToLower().Contains("text=")){
				StartIndex = Line.ToLower().IndexOf("text=") + "text=".Length;				
			}
			if (StartIndex == -1)
				return -1;
			StartIndex++;
			return StartIndex;
		}
		
		public string GetLine(string Line){
			int StartIndex = GetStrIndex(Line);
			string Rst = "";
			for (int i = StartIndex; i < Line.Length; i++){
				char c = Line[i];
				if (c == '"' && Line[i-1] != '\\')
					break;
				Rst += c;
			}
			return Rst;
		}
		
		public int StrLen(string Line) {
			return GetLine(Line).Length;
		}
		
		public bool IsStr(string Line){
			return Line.ToLower().Contains("text=") || Line.ToLower().Contains("desc=");	
		}
		
		public void AppendArray<T>(ref T[] Array, T Value){
			T[] Output = new T[Array.Length+1];
			Array.CopyTo(Output, 0);
			Output[Array.Length] = Value;
			Array = Output;
		}

        public byte[] Export(string[] Text) {
            StringBuilder Compiler = new StringBuilder();
            for (int i = 0, t = 0; i < Script.Length; i++){
				string Line = Script[i];
				if (!IsStr(Line)){
					Compiler.AppendLine(Line);
					continue;
				} 
				int SI = GetStrIndex(Line);
				int Len = StrLen(Line);
				if (Len == 0){
					Compiler.AppendLine(Line);
					continue;
				} 
				string Begin = Line.Substring(0, SI);
				string End = Line.Substring(SI + Len, Line.Length - (SI + Len));
				Compiler.AppendLine(Begin + Text[t++].Replace("\"", "\\\"").Replace("\n", "/n") + End);
			}
			
            byte[] barr = Eco.GetBytes(Compiler.ToString());
			if (BOOM){
				byte[] Out = new byte[barr.Length+2];
				Out[0] = 0xFF;
				Out[1] = 0xFE;
				barr.CopyTo(Out, 0);
				barr = Out;
			}
			return barr;
        }

    }
}
