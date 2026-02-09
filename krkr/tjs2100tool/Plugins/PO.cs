#IMPORT System.Core.dll
#IMPORT System.Linq.dll
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace PO {
    public class Plain {
        string[] Script;
		Encoding Eco = Encoding.UTF8;
		bool BOOM = false;
        public Plain(byte[] Script) {
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
			List<string> Lines = new List<string>();
			int Status = 0;
			for (int i = 0; i < Script.Length; i++) {
				string Line = Script[i];
				switch (Status)
				{
					case 0:
						if (Line.StartsWith("msgid "))
						{
							if (Skip(Line, " ").Trim() == "\"\"")
							{
								Status = 1;
								Lines.Add(string.Empty);
								break;
							}
							Lines.Add(Inner(Line, "\"", "\""));
						}
						break;
					case 1:
						if (!Line.StartsWith("\"") || !Line.EndsWith("\""))
						{
							Status = 0;
							break;
						}
						Lines[Lines.Count - 1] += Inner(Line, "\"", "\"");
						break;
				}
			}
			for (int i = 0; i < Lines.Count; i++) {
				Lines[i] = Unescape(Lines[i]);
			}
            return Lines.ToArray();
        }

		private string Skip(string Text, string Sub) {
			int Index = Text.IndexOf(Sub) + Sub.Length;
			if (Index - Sub.Length == -1)
				throw new Exception();
			return Text.Substring(Index);
		}

		private string Inner(string Text, string Begin, string End) {
			string Val = Skip(Text, Begin);
			int Index = Val.LastIndexOf(End);
			return Val.Substring(0, Index);
		}

		private string Unescape(string Text) {
			string Result = string.Empty;
			bool Escaped = false;
			foreach (var Char in Text) {
				if (Escaped) {
					switch (char.ToLower(Char)) {
						case '\\':
							Result += "\\";
							break;
						case '"':
							Result += "\"";
							break;
						case 'n':
							Result += "\n";
							break;
						case 'r':
							Result += "\r";
							break;
						case 't':
							Result += "\t";
							break;
					}
					Escaped = false;
					continue;
				}
				if (Char == '\\') {
					Escaped = true;
					continue;
				}
				Result += Char;
			}

			return Result;
		}

		private string Escape(string Text) {
			string Result = string.Empty;
			foreach (var Char in Text) {
				switch (Char) {
					case '\n':
						Result += "\\n";
						break;
					case '\t':
						Result += "\\t";
						break;
					case '\r':
						Result += "\\r";
						break;
					case '\\':
						Result += "\\\\";
						break;
					case '"':
						Result += "\\\"";
						break;
					default:
						Result += Char;
						break;
				}
			}
			return Result;
		}

        public byte[] Export(string[] Text) {
			StringBuilder SB = new StringBuilder();
			for (int i = 0, x = 0; i < Script.Length; i++)
			{
				string Line = Script[i];
				if (!Line.StartsWith("msgstr ")) {
					SB.AppendLine(Line);
					continue;
				}

				SB.AppendLine("msgstr \""+Escape(Text[x++])+"\"");
			}

			return Eco.GetBytes(SB.ToString());
        }

    }
}
