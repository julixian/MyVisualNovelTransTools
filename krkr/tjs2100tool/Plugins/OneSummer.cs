#IMPORT System.Linq.dll
using System.Collections.Generic;
using System.Text;

namespace Filter {
    public class OneSummer {
        private Encoding Eco = Encoding.GetEncoding(932);
        
		private string Script;
		private string[] Tags;
		
		
        public OneSummer(byte[] Script) {			
            this.Script = Eco.GetString(Script).Replace("\r\n", "\n");
        }
		
        public string[] Import() {
			EnumTags();
            List<string> Dialogues = new List<string>();
            foreach (string Line in Tags) {
				bool IsText = Line.StartsWith("<11 ") || Line.StartsWith("<12 ");
				if (!IsText)
					continue;
				
				Dialogues.Add(Line.Substring(4).TrimEnd('>'));
            }
            return Dialogues.ToArray();
        }
		
		private void EnumTags(){
            List<string> Tags = new List<string>();
            bool InTag = false;
			string Buffer = string.Empty;
			foreach (char Char in Script) {
				if (Char == '<')
					InTag = true;
				if (Char == '>' && InTag){
					Buffer += Char;
					InTag = false;
					Tags.Add(Buffer);
					Buffer = string.Empty;
				}
				
				if (!InTag)
					continue;
				
				Buffer += Char;
            }		
			
			this.Tags = Tags.ToArray();
		}

        public byte[] Export(string[] Content) {
            string Result = "";
			uint ID = 0;
            foreach (string Line in Tags) {
				bool IsText = Line.StartsWith("<11 ") || Line.StartsWith("<12 ");
				if (!IsText){
					Result += Line + "\r\n";
					continue;
				}
				
				Result += Line.Substring(0, 4) + Content[ID++] + ">\r\n";
            }
            return Eco.GetBytes(Result);
        }
    }
}
