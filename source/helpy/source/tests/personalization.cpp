#include "Core/HelpyPersonalization.h"
#include "Core/ItemPresentation.h"
#include <cassert>
#include <iostream>
int main(int argc,char**argv){
    assert(argc==2);using namespace PS;namespace P=HelpyPersonalization;const auto file=std::filesystem::path(argv[1])/("labels-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())+".json");
    assert(P::Load(file));const nlohmann::json item={{"Path","/Game/Items/Stone.Stone"},{"Name","Stone"}},original=item;const auto key=P::ItemKey(item);
    assert(P::Set(key,{{"Nickname","My stones"},{"Favorite",true},{"Locked",true},{"Tags",{"junk","Building"}}}));
    assert(ItemPresentation::Name(item)=="My stones"&&ItemPresentation::OriginalName(item)=="Stone"&&item==original);
    assert(P::Favorite(item)&&P::Locked(item)&&P::Tagged(item,"JUNK")&&P::QueryUsesLabels("stones"));assert(!P::BulkEligible(item,true));
    assert(P::Set(key,{{"Locked",false},{"Favorite",false}})&&P::BulkEligible(item,true));auto equipped=item;equipped["Equipped"]=true;assert(!P::BulkEligible(equipped,false));
    assert(P::Load(file)&&ItemPresentation::Name(item)=="My stones");assert(P::Set(P::ModKey("RSDWSpears"),{{"Nickname","Spears & Scythes"}}));
    assert(P::Nickname(P::ModKey("rsdwspears"))=="Spears & Scythes"&&P::ItemKey({{"Name","Stone"}}).empty());
    auto stack=item;stack["PlayerGuid"]="11111111-22222222-33333333-44444444";stack["Instance"]={1,2,3,4};auto other=stack;other["Instance"]={5,6,7,8};
    const auto stackKey=P::StackKey(stack);assert(P::StackKeyValid(stackKey)&&P::StackKey(item).empty());
    const auto type=P::TypeKey(stack);assert(P::TypeKeyValid(type)&&P::TypeKey(item)==key);
    assert(P::Set(type,{{"Favorite",true},{"Tags",nlohmann::json::array()}})&&P::Favorite(stack)&&P::Favorite(other));
    assert(P::Set(stackKey,{{"Favorite",false},{"Tags",{"junk"}}})&&!P::Favorite(stack)&&P::Favorite(other)&&P::Tagged(stack,"junk")&&!P::Tagged(other,"junk"));
    assert(P::Set(stackKey,{{"Favorite",nullptr},{"Tags",nullptr}})&&P::Favorite(stack));
    assert(!P::StackKeyValid("stack:../foreign")&&!P::Set("stack:../foreign",{{"Favorite",true}}));
    auto foreign=stack;foreign["PlayerGuid"]="aaaaaaaa-bbbbbbbb-cccccccc-dddddddd";assert(P::StackKey(foreign)!=stackKey&&!P::Favorite(foreign));
    assert(!P::Set(key,{{"Nickname","bad\nname"}}));assert(!P::Set("item:../bad",{{"Nickname","no"}}));
    const auto contents=ConfigFiles::Read(file);ConfigFiles::Write(file,"{\"Format\":1,\"Entries\":{}}");assert(!P::Set(key,{{"Nickname","must not overwrite"}}));assert(ConfigFiles::Read(file)!="must not overwrite");
    ConfigFiles::Write(file,"invalid");assert(!P::Load(file)&&!P::Set(key,{{"Nickname","must not replace invalid data"}}));assert(ConfigFiles::Read(file)=="invalid");
    std::cout<<"Personalization: stable identities, persistence, stale-write guard, nicknames, tags, favorites, locks and bulk-removal policy passed.\n";
}
