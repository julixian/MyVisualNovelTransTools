#IMPORT System.Linq.dll
using System;
using System.Collections.Generic;
using System.Text;

namespace Plain {
    public class CSV {
        string[] Script;
		uint TextIndex = uint.MaxValue;
		Dictionary<uint, string> Prefix;
        public CSV(byte[] Script) {
            this.Script = Encoding.UTF8.GetString(Script).Replace("\r\n", "\n").Split('\n');
        }
		
        public string[] Import() {
			Prefix = new Dictionary<uint, string>();
			List<string> Lines = new List<string>();
			for (uint i = 0; i < Script.LongLength; i++){
				if (string.IsNullOrWhiteSpace(Script[i]))
					continue;
				string Line = Script[i];
				uint Cnt = (uint)Line.Split(',').LongLength;
				if (Cnt == 0)
					continue;
				
				if (string.IsNullOrWhiteSpace(Line.Split(',')[--Cnt]))
					continue;
				if (TextIndex > Cnt)
					TextIndex = Cnt;
			}
			for (uint i = 0; i < Script.LongLength; i++){
				string[] Split = Script[i].Split(',');
				if (Split.Length < TextIndex+1)
					continue;
				
				string Text = Split[TextIndex];
				for (uint x = TextIndex+1; x < Split.Length; x++)
					Text += ',' + Split[x];
				
				string Trim = Text.TrimStart();
				Prefix[i] = Text.Substring(0, Text.Length-Trim.Length);
				
				Lines.Add(Trim.Replace("\\n", "\n"));
			}
            return Lines.ToArray();
        }
		

        public byte[] Export(string[] Text) {
			StringBuilder SB = new StringBuilder();			
			for (uint i = 0, y = 0; i < Script.LongLength; i++){
				string[] Split = Script[i].Split(',');
				if (Split.Length < TextIndex+1){
					SB.AppendLine(Split[i]);
				}
				
				string Line = Split[0];
				for (uint x = 1; x < TextIndex; x++)
					Line += ',' + Split[x];
						
				SB.AppendLine(Line + ',' + Prefix[i] + Text[y++].Replace("\n", "\\n"));
			}
			
			return Encoding.UTF8.GetBytes(SB.ToString());
		}
    }
}
