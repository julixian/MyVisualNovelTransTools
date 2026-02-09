#IMPORT System.Linq.dll
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace AVGSys {
    public class SCD {
        string[] Script;
		Encoding Eco = Encoding.GetEncoding(932);
        public SCD(byte[] Script) {
            this.Script = Eco.GetString(Script).Replace("\r\n", "\n").Split('\n');
        }

        public string[] Import() {
			List<string> Lines = new List<string>();
			bool InText = false;
			string Dialog = string.Empty;
			for (uint i = 0; i < Script.Length; i++){
				string Line = Script[i];
				InText |= Line.StartsWith("*TEXT");
				if (Line == "" && InText){
					InText = false;
					Lines.Add(Dialog);
					Dialog = string.Empty;
				}
				if (!InText || Line.StartsWith("*TEXT"))
					continue;
				
				Dialog += (Dialog == string.Empty ? "" : "\n") + Line;
			}
			
            return Lines.ToArray();
        }
		
		public void AppendArray<T>(ref T[] Array, T Value){
			T[] Output = new T[Array.Length+1];
			Array.CopyTo(Output, 0);
			Output[Array.Length] = Value;
			Array = Output;
		}

        public byte[] Export(string[] Text) {
            StringBuilder Compiler = new StringBuilder();
			bool InText = false;
			for (uint i = 0, x = 0; i < Script.Length; i++){
				string Line = Script[i];
				InText |= Line.StartsWith("*TEXT");
				if (Line == "" && InText){
					InText = false;
					Compiler.AppendLine(Text[x++].Replace("\n", "\r\n").Replace(",", "、"));
					//Compiler.AppendLine();
				}
				if (!InText || Line.StartsWith("*TEXT")){
					Compiler.AppendLine(Line);
				}
			}
			
			return Eco.GetBytes(Compiler.ToString());
        }

    }
}
