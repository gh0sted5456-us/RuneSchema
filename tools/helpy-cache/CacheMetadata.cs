using Newtonsoft.Json.Linq;

static class CacheMetadata
{
    static readonly string[] Categories = ["Weapons", "Armor", "Consumables", "Ammo", "Runes", "Resources", "Tools", "Building", "Quest", "Misc"];
    static void Need(bool ok, string why) { if (!ok) throw new InvalidDataException(why); }
    static bool Owner(string value) => value == "core" || System.Text.RegularExpressions.Regex.IsMatch(value, "^(mods|plugins)/[a-z0-9][a-z0-9_ -]{0,95}$");
    static string Text(JToken value, string field) { var token=value[field];Need(token is null || token.Type==JTokenType.String,"Invalid cache metadata text: "+field);return (string?)token??""; }
    public static JObject Taglines(JArray records, JObject? edits, string owner)
    {
        var rules=new SortedDictionary<string,JObject>(StringComparer.Ordinal);
        void Add(string path, JToken value)
        {
            Need(path.StartsWith('/')&&path.Length<=2048&&!path.Contains("..")&&!path.Any(c=>c<32||c==127||"\\:*?".Contains(c))&&value is JObject,"Invalid tagline path");
            var source=Text(value,"Owner").ToLowerInvariant();if(source.Length==0)source=owner;
            var category=Text(value,"Category");var family=Text(value,"Family");
            Need((source.Length==0||Owner(source))&&(owner.Length==0||owner==source),"Taglines escape their mod owner");
            Need((category.Length==0||Categories.Contains(category))&&family.Length<=192&&family.All(c=>char.IsAsciiLetterLower(c)||char.IsAsciiDigit(c)||c=='_'),"Invalid tagline classification");
            rules[path.ToLowerInvariant()+"|"+source]=new JObject{["Path"]=path,["Owner"]=source,["Category"]=category,["Family"]=family};
        }
        foreach(var record in records)if((string?)record["Kind"]=="items")
        {
            var category=(string?)record["Data"]?["Category"]??"";
            Add((string)record["Path"]!,new JObject{["Owner"]=record["Owner"],["Category"]=Categories.Contains(category)?category:""});
        }
        if(edits is not null)
        {
            if(edits["Rules"] is JArray entries){Need(entries.Count<=65536,"Too many taglines");foreach(var rule in entries)Add((string)rule["Path"]!,rule);}
            else if(edits["Items"] is JObject items){Need(items.Count<=65536,"Too many taglines");foreach(var item in items.Properties())Add(item.Name,item.Value);}
            else throw new InvalidDataException("Taglines need a Rules array or Items object");
        }
        Need(rules.Count<=65536,"Too many taglines");return new JObject{["Format"]=1,["Rules"]=new JArray(rules.Values)};
    }
    public static JObject Tips(JToken document,string owner)
    {
        var entries=document as JArray??document["Tips"] as JArray;Need(entries is not null&&entries.Count<=8192,"Invalid cache tips array");var tips=new SortedDictionary<string,JObject>(StringComparer.Ordinal);
        foreach(var tip in entries!)
        {
            Need(tip.Type==JTokenType.String||tip is JObject,"Invalid cache tip");var text=tip.Type==JTokenType.String?(string)tip!:Text(tip,"Text");var source=tip.Type==JTokenType.String?owner:Text(tip,"Owner");if(source.Length==0)source=owner;
            Need(Owner(source)&&(owner.Length==0||source==owner)&&text.Length>0&&System.Text.Encoding.UTF8.GetByteCount(text)<=512&&!text.Any(c=>c<32||c==127),"Invalid tip text or owner");tips[source+"|"+text]=new JObject{["Owner"]=source,["Text"]=text};
        }return new JObject{["Format"]=1,["Tips"]=new JArray(tips.Values)};
    }
    public static void Test()
    {
        var edits=JObject.Parse("{\"Format\":99,\"Rules\":[{\"Path\":\"/Example/Item.Item\",\"Family\":\"set\",\"Category\":\"Armor\"}]}");
        Need(Taglines([],edits,"mods/example")["Rules"]![0]!.Value<string>("Owner")=="mods/example","Scoped taglines");
        Need(Tips(new JArray("A tip"),"mods/example")["Tips"]!.Count()==1,"Optional cache tips");
        var refused=false;try{Tips(new JArray(new JObject{["Owner"]="mods/other",["Text"]="Wrong mod"}),"mods/example");}catch(InvalidDataException){refused=true;}Need(refused,"Foreign tips must be refused");
    }
}
