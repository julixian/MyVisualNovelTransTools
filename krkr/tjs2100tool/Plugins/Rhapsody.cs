#IMPORT %CD%\Plugins\EushullyEditor.dll
using EushullyEditor;
using System.Collections.Generic;
using System.IO;
using System.Text;

public class RhapsodyHelper {
    BinEditor Editor;
    public RhapsodyHelper(byte[] Script) {
        Editor = new BinEditor(Script);
    }

    public string[] Import() {
        Editor.Import();
        Resources.MergeStrings(ref Editor, true);

		bool InString = false;
        List<string> Dialogues = new List<string>();
        for (uint i = 0; i < Editor.StringsInfo.Length; i++) {
            String Current = Editor.StringsInfo[i];

			if (InString)
				Dialogues[Dialogues.Count - 1] += "\n" + Current.Content;				
			else
				Dialogues.Add(Current.Content);
			
			InString = Continue(Current);
        }

        return Dialogues.ToArray();
    }

    public byte[] Export(string[] Strings) {
        for (uint i = 0, x = 0, y = 0; i < Editor.StringsInfo.Length; i++) {
			string[] Lines = Strings[x].Split('\n');
			
			while (Continue(Editor.StringsInfo[i])) {			
				Editor.StringsInfo[i].Content = "";
				if (y >= Lines.Length)
					Editor.StringsInfo[i++].Content = "";
				else
					Editor.StringsInfo[i++].Content += Lines[y++];				
			}
			
			Editor.StringsInfo[i].Content = "";
			while (y < Lines.Length) {
				EnsureLineEnd(ref Editor.StringsInfo[i].Content);
				Editor.StringsInfo[i].Content += Lines[y++];
			}
			x++;
			y = 0;			
        }
        return Editor.Export();
	}
	
	private bool Continue(String Data) {
		return Data.EndLine && !Data.EndText && Data.IsString;
	}
	
	const uint LineWidth = 70;
	private void EnsureLineEnd(ref string Line){
		if (Line.Length == 0)
			return;
		
		while (Line.Length <= LineWidth)
			Line += " ";
	}
}