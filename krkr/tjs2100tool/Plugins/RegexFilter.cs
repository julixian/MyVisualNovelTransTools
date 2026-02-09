#IMPORT System.Collections.Generic.dll
#IMPORT System.Linq.dll
#IMPORT System.Text.dll
#IMPORT %CD%\Plugins\System.Web.dll
#IMPORT System.Core.dll

using System.Text;
using System.Text.RegularExpressions;
using System.Collections.Generic;
using System.Linq;
using System.Web;

public class RegexFilter
{
    Encoding Encoding = Encoding.UTF8;
    string Script = string.Empty;

    string[] Regexs = new string[] {

        //EntisGLS
        "(?:text_EN=\"([^\"\\\\]*(?:\\\\.[^\"\\\\]*)*)\"|name_EN=\"([^\"\\\\]*(?:\\\\.[^\"\\\\]*)*)\")" 
    };
    public RegexFilter(byte[] Script) {
        this.Script = Encoding.GetString(Script);
    }

    public string[] Import()
    {
        List<string> Lines = new List<string>();
        foreach (var Exp in Regexs)
        {
            Regex Regex = new Regex(Exp, RegexOptions.Multiline);
            var Results = Regex.Matches(Script);
            foreach (var Result in Results.Cast<Match>())
            {
                foreach (var Group in Result.Groups.Cast<Group>().Skip(1))
                {
                    var Line = HttpUtility.HtmlDecode(Group.Value);
                    if (!string.IsNullOrWhiteSpace(Line)){
						Lines.Add(Line);
						Entries.Add(new Entry() { Offset = Group.Index, Length = Group.Length });
					}
                }
            }
        }
        return Lines.ToArray();
    }

    List<Entry> Entries = new List<Entry>();

    struct Entry
    {
        public int Offset;
        public int Length;
        public string Content;
    }

    public byte[] Export(string[] Lines)
    {
        StringBuilder Builder = new StringBuilder(Script);

        var SortedEntries = Entries.Select((e, n) => { 
            e.Content = Lines[n].Replace("\\\"", "\x0").Replace("\"", "\\\"").Replace("\x0", "\\\""); 
            return e;
        }).OrderByDescending(x => x.Offset);

        foreach (var Entry in SortedEntries)
        {
            Builder.Remove(Entry.Offset, Entry.Length);
            Builder.Insert(Entry.Offset, HttpUtility.HtmlEncode(Entry.Content));
        }

        return Encoding.GetBytes(Builder.ToString());
    }
}