#IMPORT %CD%\Plugins\GBNManager.dll
#IMPORT System.Core.dll
using GBNManager;
using System;
using System.Text;

public class GBNSJIS {
    GBN Editor;
	
    public GBNSJIS(byte[] Script) {
        Editor = new GBN(Script, Encoding.GetEncoding(932));
    }

    public string[] Import() {
        return Editor.Import();
    }

    public byte[] Export(string[] Strings) {
        return Editor.Export(Strings);
	}
}