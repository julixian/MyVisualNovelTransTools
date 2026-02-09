#IMPORT System.Linq.dll
#IMPORT System.Core.dll
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace BS5 {
    public class AoKana {

		//0 = English, 1 = Japanese, 2 = Chinese, 3 = Chinese
		const int Language = 0;

        string[] Script;
		Encoding Eco = Encoding.UTF8;
		bool BOOM = false;

        public AoKana(byte[] Script) {
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
			for (int i = 0; i < Script.Length; i++){
				if (!IsDialog(Script[i]))
					continue;
				Lines.AddRange(GetDialogs(Script[i]));
			}
            return Lines.ToArray();
        }
		//␅select 0:"Because I want you to know how to fly." 1:"Because Ms. Kagami asked me to." OUTLINE:"In response to her question, I reply..."␅select 0:"飛べるようになって欲しかったから" 1:"先生に言われたから" OUTLINE:"【選択肢】明日香の問いに…"␅select 0:"因为希望你学会飞行" 1:"因为老师让我教你" OUTLINE:"该如何回答明日香的问题……"␅select 0:"因為希望你學會飛行" 1:"因為老師讓我教你" OUTLINE:"該如何回答明日香的問題……"
		//␂【Masaya】：That's because...␂【晶也】：「そんなの……」␂【晶也】：「那是因为……」␂【晶也】：「那是因為……」
		//␂　 ...Damn, that's <i>way</i> too cliché. It's too embarrassing to mention to others, even. Still, what can I do if that's what comes to mind?␂……言うのも恥ずかしいぐらいベタだけど、すんなり思い浮かんだんだから仕方ない。␂……一点新意都没有，我都不好意思说出来。但没办法，这就是我最自然的想象。␂……一點新意都沒有，我都不好意思說出來。但沒辦法，這就是我最自然的想像。

		public bool IsDialog(string Line) {
			if (Line.StartsWith("␂") || Line.StartsWith("␅"))
				return true;
			return false;
		}

		const string OutlinePrefix = "OUTLINE:\"";
		public string[] GetDialogs(string Line){
			if (Line.StartsWith("␂"))
			{
				string[] Translations = Line.Split('␂').Skip(1).ToArray();
				string Dialog = Translations[Language];
				if (Dialog.Contains("："))
					return Dialog.Split('：');
				return new string[] { Dialog.Substring(1) };
			}
			if (Line.StartsWith("␅"))
			{
				string[] Translations = Line.Split('␅').Skip(1).ToArray();
				string Choices = Translations[Language];
				List<string> Lines = new List<string>();

				for (int i = 0; ; i++){
					int IndexOf = Choices.IndexOf(i + ":\"");
					if (IndexOf == -1)
					{
						IndexOf = Choices.IndexOf(OutlinePrefix);
						if (IndexOf == -1)
							break;
						IndexOf += OutlinePrefix.Length;
						Lines.Add(Choices.Substring(IndexOf, GetStrLen(Choices, IndexOf)));
						break;
					}
					IndexOf += i.ToString().Length + 2;
					Lines.Add(Choices.Substring(IndexOf, GetStrLen(Choices, IndexOf)));
				}

				return Lines.ToArray();
			}
			throw new Exception("Invalid Dialog Line");
		}

		public string UpdateDialog(string OriLine, string[] Strings){
			int OriLen = GetDialogs(OriLine).Length;
			if (OriLen != Strings.Length)
				throw new Exception("Invalid Array Length");
			if (OriLine.StartsWith("␂"))
			{
				string[] Translations = OriLine.Split('␂');
				Translations[Language + 1] = Strings.Length == 1 ? "　" + Strings[0] : string.Join("：", Strings);
				return string.Join("␂", Translations);
			}
			if (OriLine.StartsWith("␅"))
			{
				string[] Translations = OriLine.Split('␅');
				bool HaveOutline = OriLine.Contains(OutlinePrefix);
				string NewLine = "select ";
				for (int i = 0; i < Strings.Length; i++){
					string Prefix = i + ":";
					if (i + 1 >= Strings.Length)
						Prefix = "OUTLINE:";
					
					NewLine += Prefix + "\"" + Strings[i] + "\"";
					
					if (i + 1 < Strings.Length)
						NewLine += " ";
				}
				Translations[Language + 1] = NewLine;
				return string.Join("␅", Translations);
			}
			throw new Exception("Invalid Dialog Line");
		}

		public int GetStrLen(string Line, int Index){
			bool Escape = false;
			for (int i = Index; i < Line.Length; i++){
				char c = Line[i];
				if (c == '\\')
				{
					Escape = true;
					continue;
				}
				if (!Escape && c == '"')
					return i - Index;
				
				Escape = false;
			}
			return -1;
		}

        public byte[] Export(string[] Text) {
			StringBuilder SB = new StringBuilder();
			for (int i = 0, x = 0; i < Script.Length; i++) {
				if (!IsDialog(Script[i])){
					SB.AppendLine(Script[i]);
					continue;
				}
				int Len = GetDialogs(Script[i]).Length;

				var tmp = new string[Len];
				Array.Copy(Text, x, tmp, 0, Len);
				x += Len;

				string NewLine = UpdateDialog(Script[i], tmp);
				SB.AppendLine(NewLine);
			}
			
			return Eco.GetBytes(SB.ToString());
        }

    }
}
