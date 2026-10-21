#include "Core/ModHealthLedger.h"
#include <cassert>
int main(){
    using namespace PS;ModHealth::Ledger first;assert(first.Reconcile({"Flintlocks","Dye"}));
    assert(!first.Observe("Dye",LiveLog::Parse("Warning: /Game/Mods/AdditionalWeapons/Flintlocks/Montage"),"Game"));
    assert(first.Observe("Flintlocks",LiveLog::Parse("LogAnimation: Warning: /Game/Mods/AdditionalWeapons/Flintlocks/Montage Skeleton GUID is out-of-date"),"Game"));
    assert(first.Observe("Dye",LiveLog::Parse("[mod:Dye] Error: broken item"),"RuneSchema"));
    assert(!first.Observe("Other",LiveLog::Parse("[Other] Error: unrelated"),"Game"));
    const auto stored=first.Document();ModHealth::Ledger second;second.Load(nlohmann::json::parse(stored.dump()));
    assert(second.Read("Dye")["Errors"]==0&&second.Read("Dye")["Lifetime"]["Errors"]==1);
    assert(second.Read("Flintlocks")["Warnings"]==0&&second.Read("Flintlocks")["Lifetime"]["Warnings"]==1);
    assert(second.Observe("Dye",LiveLog::Parse("[mod:Dye] Error: broken item"),"RuneSchema"));
    assert(second.Read("Dye")["Errors"]==1&&second.Read("Dye")["Lifetime"]["Errors"]==2);
    assert(second.Reconcile({"Dye"})&&!second.Document()["Mods"].contains("Flintlocks"));
    assert(!second.Reconcile({"Dye"}));
    for(int i=0;i<100;++i)second.Observe("Dye",LiveLog::Parse("[mod:Dye] Error: repeated"),"RuneSchema");
    assert(second.Document()["Mods"]["Dye"]["LastIssues"].size()==8&&second.Read("Dye")["Details"].size()==24);
    bool refused=false;try{second.Load({{"Format",1},{"Mods",{{"Bad",{{"Errors",-1}}}}}});}catch(...){refused=true;}assert(refused);
    assert(second.Read("Dye")["Lifetime"]["Errors"]==102); // A bad ledger never overwrites the last valid state.
}
