#include "Core/ItemSourcePrefix.h"
#include <cassert>
#include <iostream>
using namespace PS;
int main(){
    auto p=ItemSourcePrefix::Parse("rune source:JBM sword");assert(p.Source=="JBM"&&p.Text=="rune sword");
    p=ItemSourcePrefix::Parse("SRC:\"Colors of Money\" coin");assert(p.Source=="Colors of Money"&&p.Text=="coin");
    p=ItemSourcePrefix::Parse("(sword OR axe) source:vanilla");assert(p.Source=="vanilla"&&p.Text=="(sword OR axe)");
    p=ItemSourcePrefix::Parse("opensource:x");assert(!p.Source&&p.Text=="opensource:x");
    p=ItemSourcePrefix::Parse("source:");assert(p.Source&&p.Source->empty()&&p.Text.empty());
    assert(ItemSourcePrefix::Replace("rune source:JBM sword","Colors of Money")=="rune source:\"Colors of Money\" sword");
    assert(ItemSourcePrefix::Replace("rune source:JBM sword","")=="rune sword");
    assert(ItemSourcePrefix::Replace("rune sword","JBM")=="rune sword");
    const std::vector<std::pair<std::string,std::string>> choices{{"mods/jbm","JBM"},{"mods/colorsofmoney","Colors of Money"},{"mods/colorpack","Color Pack"},{"plugins/rsnetworking","RS Networking"}};
    assert(ItemSourcePrefix::Resolve("Base",choices)=="vanilla"&&ItemSourcePrefix::Resolve("mods",choices)=="modded");
    assert(ItemSourcePrefix::Resolve("jbm",choices)=="mods/jbm"&&ItemSourcePrefix::Resolve("colorsofmoney",choices)=="mods/colorsofmoney");
    assert(ItemSourcePrefix::Resolve("Colors",choices)=="mods/colorsofmoney");
    assert(!ItemSourcePrefix::Resolve("color",choices));// ambiguous prefix
    assert(ItemSourcePrefix::Resolve("rs net",choices)=="plugins/rsnetworking");
    assert(!ItemSourcePrefix::Resolve("nothing",choices)&&!ItemSourcePrefix::Resolve("",choices));
    std::cout<<"item-source-prefix passed\n";
}
