#include "Core/ItemSearchExpression.h"
#include "Core/ItemCatalogFilter.h"
#include "Core/CacheBundleItems.h"
#include <cassert>
#include <random>
// Boolean item search: syntax, the plain-query compatibility contract, index soundness,
// and malformed input. The same header is compiled into the RuneSchema core.
int main(){
    using namespace PS;using Json=nlohmann::json;using ItemSearchExpression::Expression;
    const auto lower=[](std::string word){return HelpyCacheSources::Lower(std::move(word));};
    const auto parse=[&](std::string_view text){return Expression::Parse(text,lower);};
    const auto matches=[&](std::string_view query,std::string_view text){return parse(query).Matches(text);};
    // Plain AND search is unchanged: same terms, same order, and still the fast path.
    {const auto plain=parse("Iron  SWORD two-handed");
        assert(plain.Plain()&&(plain.Required()==std::vector<std::string>{"iron","sword","two-handed"}));
        assert(plain.Matches("big iron sword, two-handed")&&!plain.Matches("iron axe"));
        assert(parse("").Empty()&&parse("   ").Empty()&&parse("").Matches("anything"));}
    // OR, |, NOT, -, !, AND, &&, grouping and precedence (NOT > AND > OR).
    assert(matches("sword OR axe","iron axe")&&matches("sword | axe","iron sword")&&!matches("sword OR axe","bow"));
    assert(matches("sword||axe","axe")&&matches("sword AND iron","iron sword")&&matches("sword && iron","iron sword"));
    assert(!matches("sword -rusty","rusty sword")&&matches("sword -rusty","iron sword"));
    assert(!matches("sword NOT rusty","rusty sword")&&!matches("sword !rusty","rusty sword"));
    assert(matches("-rusty","iron sword")&&!matches("-rusty","rusty sword"));
    assert(matches("iron sword OR bow","bow")&&!matches("iron sword OR bow","iron axe")); // (iron AND sword) OR bow
    assert(matches("iron (sword OR axe)","iron axe")&&!matches("iron (sword OR axe)","bronze axe"));
    assert(matches("(sword OR axe) -rusty","sharp axe")&&!matches("(sword OR axe) -rusty","rusty axe"));
    assert(!matches("-(sword OR axe)","iron axe")&&matches("-(sword OR axe)","iron bow"));
    assert(matches("NOT NOT sword","sword")&&!matches("NOT NOT sword","axe"));
    // Lower-case "or"/"not" stay words; a hyphen inside a word or a lone "-" is text.
    assert(parse("armor or").Plain()&&(parse("armor or").Required()==std::vector<std::string>{"armor","or"}));
    assert(parse("not").Plain()&&matches("not","knot")&&matches("two-handed","two-handed axe")&&matches("a - b","a - b"));
    // Phrases.
    assert(matches("\"iron sword\"","iron sword of rage")&&!matches("\"iron sword\"","iron great sword"));
    assert(matches("-\"iron sword\" sword","steel sword")&&!matches("-\"iron sword\" sword","iron sword"));
    assert(matches("\"iron sword","iron sword")); // Unterminated quote runs to the end.
    // Required terms are a sound prefilter: every match contains every required term.
    {const auto mixed=parse("iron (sword OR axe) -rusty \"old guard\"");
        assert(!mixed.Plain()&&(mixed.Required()==std::vector<std::string>{"iron","old guard"}));
        assert((mixed.Terms()==std::vector<std::string>{"iron","sword","axe","rusty","old guard"}));
        assert(parse("sword OR axe").Required().empty()&&parse("-rusty").Required().empty());}
    // Malformed input never throws and degrades sensibly while the player types.
    assert(matches("(sword OR axe","axe")&&matches("sword) OR axe","axe")&&matches("sword OR","sword")&&matches("OR sword","sword"));
    assert(matches("sword -","sword -")&&parse("()").Empty()&&parse("NOT").Empty()&&parse("-\"\"").Empty()&&parse("| | AND").Empty());
    {std::string deep;for(int i=0;i<200;++i)deep+="(";deep+="sword";assert(matches(deep,"sword"));}
    {std::mt19937 random{7};const std::string alphabet="ab ()\"|-!&ORANDNOT";
        for(int round=0;round<20000;++round){std::string text;const auto size=random()%40;for(size_t i=0;i<size;++i)text+=alphabet[random()%alphabet.size()];
            const auto expression=parse(text);(void)expression.Matches("ab ba a b");
            for(const auto& term:expression.Required())assert(!term.empty());}}
    // A fold that empties a term drops it and any operator applied only to it.
    {const auto alnum=[](std::string word){std::string out;for(unsigned char c:word)if(std::isalnum(c))out+=char(std::tolower(c));return out;};
        const auto folded=Expression::Parse("Iron-Sword -... \"Old Guard\"",alnum);
        assert(folded.Plain()&&(folded.Required()==std::vector<std::string>{"ironsword","oldguard"})&&folded.Matches("theironswordoftheoldguard"));}
    // The catalogue filter: boolean rows, category text, and the plain fast path.
    const Json sword={{"Path","/Game/Items/Weapons/ITEM_IronSword.ITEM_IronSword"},{"Name","Iron Sword"},{"Category","Weapons"}};
    const Json rusty={{"Path","/Game/Items/Weapons/ITEM_RustySword.ITEM_RustySword"},{"Name","Rusty Sword"},{"Category","Weapons"}};
    const Json axe={{"Path","/Game/Items/Tools/ITEM_IronAxe.ITEM_IronAxe"},{"Name","Iron Axe"},{"Category","Tools"}};
    const Json bread={{"Path","/Game/Items/Food/ITEM_Bread.ITEM_Bread"},{"Name","Bread"},{"Category","Food"}};
    const auto count=[&](const std::string& query){const ItemCatalogFilter::Query filter(query);int found=0;for(const auto* row:{&sword,&rusty,&axe,&bread})found+=filter(*row);return found;};
    assert(count("sword")==2&&count("sword -rusty")==1&&count("sword OR axe")==3&&count("-iron")==2&&count("iron -weapons")==1);
    assert(count("__helpycat:Weapons -rusty")==1&&count("__helpycat:Tools sword OR axe")==1&&count("(bread OR axe) -tools")==1);
    {const ItemCatalogFilter::Query plain("Iron Sword");assert(!plain.Boolean()&&plain.HasText()&&(plain.Words==std::vector<std::string>{"iron","sword"}));
        const ItemCatalogFilter::Query negative("-rusty");assert(negative.Boolean()&&negative.HasText()&&negative.Words.empty());
        assert(!ItemCatalogFilter::Query("__helpycat:Weapons").HasText());}
    // The bundle index narrows by required terms and its trigram mask, then applies the full expression.
    {Json records=Json::array();for(const auto* row:{&sword,&rusty,&axe,&bread})records.push_back({{"Kind","items"},{"Path",row->at("Path")},{"Data",*row}});
        const CacheBundleItems::Index index(records);const auto rows=[&](const std::string& query){return index.Page(query,"",40)["Rows"].size();};
        assert(rows("sword")==2&&rows("sword -rusty")==1&&rows("sword OR bread")==3&&rows("-sword")==2&&rows("iron (sword OR axe) -tools")==1);
        assert(rows("\"iron sword\"")==1&&rows("\"sword axe\"")==0&&rows("__helpycat:Weapons -rusty")==1);}
}
