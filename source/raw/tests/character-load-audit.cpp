#include "Core/CharacterLoadAudit.h"
#include "Core/LoadProtectionNotice.h"
#include <cassert>
#include <thread>
#include <chrono>
using namespace PS::CharacterLoadAudit;
int main(){
    assert(!Token()&&!End(0)&&!PreparedForFallback());Prepared();assert(!PreparedForFallback());Pruned(5);assert(!Bypassed(Gate::Validation));
    const auto first=Begin();assert(first&&Token()==first&&!Begin());
    assert(!PreparedForFallback());Prepared();assert(PreparedForFallback());
    Pruned(2);assert(Bypassed(Gate::Validation)&&!Bypassed(Gate::Validation));
    assert(Bypassed(Gate::PlayerState));assert(!End(first+1)&&Token()==first);
    std::uint64_t child=0;std::thread worker([&]{assert(!Token()&&!PreparedForFallback());child=Begin();Pruned(9);const auto result=End(child);assert(result&&result->RegistryPruned==9&&!result->GameplayPrepared);});worker.join();
    assert(child!=first&&Token()==first);
    const auto receipt=End(first);assert(receipt&&receipt->RegistryPruned==2&&receipt->ValidationBypasses==2&&receipt->PlayerStateBypasses==1&&!receipt->Ambiguous&&!Token());
    const auto second=Begin();assert(second!=first&&!PreparedForFallback());Prepared();Pruned(1);Ambiguous();assert(!Token()&&!PreparedForFallback());Pruned(20);assert(!Bypassed(Gate::PlayerState));
    const auto nested=End(second);assert(nested&&nested->Ambiguous&&nested->RegistryPruned==1&&!nested->PlayerStateBypasses);
    assert(!End(second));const auto third=Begin();assert(End(third)->RegistryPruned==0);
    const std::string character="00112233-4455-6677-8899-aabbccddeeff",other="11112233-4455-6677-8899-aabbccddeeff";
    PS::LoadProtectionNotice::Record(character,2,1,false);
    auto notice=PS::LoadProtectionNotice::Peek(character);assert(notice&&notice->Removed==2&&notice->Skipped==1&&!PS::LoadProtectionNotice::Peek(other));
    PS::LoadProtectionNotice::Record(character,0,1,true);
    PS::LoadProtectionNotice::Acknowledge(character,notice->Version);assert(PS::LoadProtectionNotice::Peek(character));
    notice=PS::LoadProtectionNotice::Peek(character);assert(notice->Bypassed&&notice->Skipped==1);
    assert(PS::LoadProtectionNotice::Message(*notice)=="Removed 2 records.");
    PS::LoadProtectionNotice::Notice owned;owned.Removed=12;owned.Owners["Siege"]=12;
    assert(PS::LoadProtectionNotice::Message(owned)=="Removed 12 records from uninstalled mod: Siege.");
    owned.Owners["Siege"]=11;assert(PS::LoadProtectionNotice::Message(owned)=="Removed 12 records.");
    assert(PS::LoadProtectionNotice::Message({}).empty());
    const auto mods=std::filesystem::temp_directory_path()/("runeschema-owners-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(mods));
    const auto removedRows=nlohmann::json::array({{{"Mod","Siege"}},{{"Mod","Dye"}},{{"Mod","Registry-unknown (owner unavailable)"}}});
    const std::map<std::string,std::size_t> knownOwners{{"Siege",1},{"Dye",1}};
    const auto activeMods=nlohmann::json::array({{{"Id","dYe"}}});
    const auto counts=PS::LoadProtectionNotice::UninstalledOwners(removedRows,knownOwners,activeMods,mods);
    assert(counts.size()==1&&counts.at("Siege")==1);
    assert(std::filesystem::create_directory(mods/"Siege"));
    assert(PS::LoadProtectionNotice::UninstalledOwners(removedRows,knownOwners,activeMods,mods).empty());
    assert(std::filesystem::remove(mods/"Siege")&&std::filesystem::remove(mods));
    PS::CleanupNotice::WorldReceipt world;int previous=0,current=0,player=0,otherPlayer=0;
    world.Begin(L"selected");world.Record(L"other",99);world.Record(L"selected",7);world.Entering(&previous);
    world.Entered(&previous);assert(!world.Pending(&player,&previous));world.Entered(&current);
    assert(world.Pending(&player,&current)&&!world.Pending(&player,&previous));world.Acknowledge(&player);
    assert(!world.Pending(&player,&current)&&world.Pending(&otherPlayer,&current));world.Begin(L"next");assert(!world.Pending(&otherPlayer,&current));
    PS::LoadProtectionNotice::Acknowledge(character,notice->Version);assert(!PS::LoadProtectionNotice::Peek(character));
    PS::LoadProtectionNotice::Record(character,2,0,false,{{"Siege",2}});
    PS::LoadProtectionNotice::Record(character,3,0,false,{{"Siege",3}},true);
    PS::LoadProtectionNotice::Record(character,3,0,false,{{"Siege",3}},true);
    notice=PS::LoadProtectionNotice::Peek(character);assert(notice->Removed==5&&notice->Owners.at("Siege")==5);
    PS::LoadProtectionNotice::Record(character,0,0,false,{},true);
    notice=PS::LoadProtectionNotice::Peek(character);assert(notice->Removed==2&&notice->Owners.at("Siege")==2);
    PS::LoadProtectionNotice::Acknowledge(character,notice->Version);
    PS::LoadProtectionNotice::Record("not-a-guid",100,0,true);assert(!PS::LoadProtectionNotice::Peek("not-a-guid"));
    PS::LoadProtectionNotice::SessionBypasses=1;std::uint64_t seen=1;
    ++PS::LoadProtectionNotice::SessionBypasses;assert(!PS::LoadProtectionNotice::SessionBypasses.compare_exchange_strong(seen,0));
    assert(seen==2&&PS::LoadProtectionNotice::SessionBypasses.compare_exchange_strong(seen,0));
    const auto fourth=Begin();BindCharacter(character);assert(Current.Character==character);BindCharacter(other);assert(!Token());End(fourth);
}
