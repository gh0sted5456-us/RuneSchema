#include "WikiLinks.h"
#include "Core/HelpyWebOrigin.h"
#include <cassert>
#include <iostream>
int main(){
    using namespace PS;
    assert(WikiLinks::Lookup("")==WikiLinks::Home&&WikiLinks::Lookup("  \t ")==WikiLinks::Home);
    assert(WikiLinks::Lookup("Adventurer's tunic")=="https://dragonwilds.runescape.wiki/w/Special:Search?search=Adventurer%27s%20tunic&go=Go");
    assert(WikiLinks::Lookup(" Rune bar ")=="https://dragonwilds.runescape.wiki/w/Special:Search?search=Rune%20bar&go=Go");
    assert(WikiLinks::Lookup("a&b=c#d?e/f")=="https://dragonwilds.runescape.wiki/w/Special:Search?search=a%26b%3Dc%23d%3Fe%2Ff&go=Go");
    assert(WikiLinks::Lookup("\xC3\xA9")=="https://dragonwilds.runescape.wiki/w/Special:Search?search=%C3%A9&go=Go");
    assert(WikiLinks::Lookup(std::string(500,'x')).size()==WikiLinks::Lookup(std::string(400,'x')).size());
    // Every address the plugin builds stays inside its origin.
    for(auto q:{"", "x", "https://evil.example", "@evil", "../.."})assert(HelpyWebOrigin::Inside(WikiLinks::Lookup(q),WikiLinks::Origin));
    assert(HelpyWebOrigin::Valid(WikiLinks::Origin));
    for(auto bad:{"http://dragonwilds.runescape.wiki","https://","https://localhost","https://a.b/","https://a.b:443","https://user@a.b","https://A.b","https://.a.b","https://a..b","https://a.b."})assert(!HelpyWebOrigin::Valid(bad));
    const std::string_view o="https://dragonwilds.runescape.wiki";
    assert(HelpyWebOrigin::Inside(std::string_view("https://dragonwilds.runescape.wiki"),o));
    assert(HelpyWebOrigin::Inside(std::string_view("https://dragonwilds.runescape.wiki/w/Rune_bar"),o));
    assert(HelpyWebOrigin::Inside(std::string_view("https://dragonwilds.runescape.wiki?x"),o));
    for(auto bad:{"https://dragonwilds.runescape.wiki.evil.com/","https://dragonwilds.runescape.wiki@evil.com/","https://dragonwilds.runescape.wikix","http://dragonwilds.runescape.wiki/","https://dragonwilds.runescape.wik"})
        assert(!HelpyWebOrigin::Inside(std::string_view(bad),o));
    assert(HelpyWebOrigin::Inside(std::wstring_view(L"https://dragonwilds.runescape.wiki/w/X"),std::wstring_view(L"https://dragonwilds.runescape.wiki")));
    assert(HelpyWebOrigin::External(L"https://x.y")&&HelpyWebOrigin::External(L"http://x.y")&&!HelpyWebOrigin::External(L"file:///C:/")&&!HelpyWebOrigin::External(L"javascript:alert(1)"));
    std::cout<<"PASS wiki links and web origins\n";
}
