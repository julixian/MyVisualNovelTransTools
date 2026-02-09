#IMPORT System.Core.dll
#IMPORT System.Linq.dll
#IMPORT System.Web.dll
#IMPORT System.Runtime.dll
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Web;
using System.Net;

namespace HTML
{
    public class Plain
    {
        string Script;
        List<TextInfo> Infos;
        Encoding Eco = Encoding.UTF8;
        bool BOOM = false;
        public Plain(byte[] Script)
        {
            if (Script[0] == 0xFF && Script[1] == 0xFE)
            {
                BOOM = true;
                byte[] narr = new byte[Script.Length - 2];
                for (int i = 2; i < Script.Length; i++)
                    narr[i - 2] = Script[i];
                this.Script = Eco.GetString(narr).Replace("\r\n", "\n");
                return;
            }
            this.Script = Eco.GetString(Script).Replace("\r\n", "\n");
        }
		
		private readonly string[] SkipTags = new string[] { "style" };
        private readonly string[] AllowTags = new string[] { "em", "b", "i", "br" };
        public string[] Import()
        {
            Infos = new List<TextInfo>();
            int Status = 0;
            bool DenyTag = false;
			bool InSkip = false;
            TextInfo LastTagInfo = new TextInfo();
            TextInfo TagInfo = new TextInfo();
            TextInfo Current = new TextInfo();
            for (int i = 0; i < Script.Length; i++)
            {
                char c = Script[i];
                switch (Status)
                {
                    default:
                        if (c == '<')
                        {
                            Status = 1;
                            TagInfo.Begin = i;
                            break;
                        }
                        break;
                    case 1:
                        if (c == '>')
                        {
                            TagInfo.End = i;
                            string Tag = GetText(TagInfo).ToLower().Trim(' ', '\n', '\r', '<', '>', '\\', '/');							
							bool SkipTag = (from x in SkipTags where Tag.StartsWith(x) select x).Any();
							if (SkipTag) {
								InSkip = !InSkip;
								if (!InSkip){
									Current = new TextInfo();
									Current.Begin = i + 1;
									if (Infos.Count > 0)
										Infos.Remove(Infos.Last());
									Status = 2;
									break;
								}
							}
							bool AllowedTag = (from x in AllowTags where Tag == x select x).Any();
							bool IncludeTag = false;
							if (DenyTag && AllowedTag){
								int LastEnd = LastTagInfo.End + 1;
                                while (char.IsWhiteSpace(Script[LastEnd]))
                                    LastEnd++;
								if (LastEnd == TagInfo.Begin)
									IncludeTag = true;
							}
							
							if (!DenyTag && AllowedTag)
                            {
                                Current = Infos.Last();
                                Infos.Remove(Current);
                                Status = 2;
                                break;
                            }
                            else DenyTag = true;

                            Status = 2;
                            Current = new TextInfo();
                            Current.Begin = i + 1;
							
							if (IncludeTag)
								Current.Begin = TagInfo.Begin;
							
                            continue;
                        }
						if (InSkip)
							continue;
                        if (char.IsWhiteSpace(c) && i == Current.Begin)
                            Current.Begin = i;
                        break;
                    case 2:
                        if (c == '<')
                        {
							var Tag = GetTagAt(Script, i).Split(' ')[0].ToLower().Trim(' ', '\n', '\r', '<', '>', '\\', '/');
							bool AllowedTag = (from x in AllowTags where Tag == x select x).Any();
							if (AllowedTag)
								break;
							
                            Current.End = i;
                            if (Current.Length > 2)
                            {
                                while (char.IsWhiteSpace(Script[Current.End - 1]))
                                    Current.End--;
                                if (Current.Length > 2) {
									DenyTag = false;
                                    Infos.Add(Current);
                                }
                            }
							LastTagInfo = TagInfo;
                            TagInfo.Begin = i;
                            Status = 1;
                            break;
                        }
                        break;
                }
            }
            return (from x in Infos select GetText(x)).ToArray();
        }
		
		private string GetTagAt(string Str, int Index){
			if (Str[Index] == '<')
				Index++;
			int Begin = Index;
			while (Str[Index] != '>')
				Index++;
			
			int Len = Index - Begin;
			return Str.Substring(Begin, Len);
		}

        public byte[] Export(string[] Text)
        {
            StringBuilder SB = new StringBuilder(Script);
            for (int i = Infos.Count - 1; i >= 0; i--)
                SetText(Infos[i], SB, Text[i]);

            return Eco.GetBytes(SB.ToString());
        }

        struct TextInfo
        {
            public int Begin;
            public int End;

            public int Length
            {
                get
                {
                    return End - Begin;
                }
                set
                {
                    End = Begin + value;
                }
            }
        }

        private string GetText(TextInfo Info)
        {
            return WebUtility.HtmlDecode(Script.Substring(Info.Begin, Info.End - Info.Begin));
        }

        private void SetText(TextInfo Info, StringBuilder Builder, string Content)
        {
            Builder.Remove(Info.Begin, Info.Length);
            Builder.Insert(Info.Begin, WebUtility.HtmlEncode(Content).Replace("&lt;br /&gt;", "<br />").Replace("&lt;br/&gt;", "<br/>").Replace("&lt;br&gt;", "<br>"));
        }
    }
}
