#IMPORT System.Linq.dll
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace STR {
    public class Processor {
        BinaryReader Script;
		
		
        public Processor(byte[] Script) {
			this.Script = new BinaryReader((Stream)new MemoryStream(Script), Encoding.UTF8);
        }

        public string[] Import() {
            string[] Arr = new string[Script.ReadUInt32()];
			for (uint i = 0; i < Arr.LongLength; i++){
				Arr[i] = Script.ReadString();
			}
			
			return Arr;
        }		

        public byte[] Export(string[] Text) {
			MemoryStream TMP = new MemoryStream();
			BinaryWriter Writer = new BinaryWriter(TMP, Encoding.UTF8);
			Writer.Write((uint)Text.LongLength);
			
			for (uint i = 0; i < Text.LongLength; i++)
				Writer.Write(Text[i]);
			
			Writer.Flush();
			
			byte[] CNT = TMP.ToArray();
			TMP.Close();
			
			return CNT;
        }
    }
}
