#include "Core/CharacterAppearance.h"
#include <cassert>
#include <stdexcept>
#undef assert
#define assert(value) ((value)?void():throw std::runtime_error(#value))
template<class F>void Reject(F call){bool failed=false;try{call();}catch(...){failed=true;}assert(failed);}
int main(){
    namespace A=PS::CharacterAppearance;using J=A::Json;
    const auto route=A::Declaration("BodyMod","appearance",{{"MaxBytes",4096},{"Class","/BodyMod/BPC_Consumer.BPC_Consumer_C"}});
    assert(route.Key=="BodyMod:appearance"&&route.Limit==4096);
    assert(A::Declaration("BodyMod","native",J::object()).Class.empty());
    Reject([&]{A::Declaration("Bad:Owner","appearance",J::object());});
    Reject([&]{A::Declaration("BodyMod","appearance",{{"Class","/Script/Engine.ActorComponent"}});});
    Reject([&]{A::Declaration("BodyMod","appearance",{{"Class","/BodyMod/../Bad.Bad_C"}});});
    Reject([&]{A::Declaration("BodyMod","appearance",{{"Function","OnAppearance"}});});
    Reject([&]{A::Declaration("BodyMod","appearance",{{"MaxBytes",-1}});});
    Reject([&]{A::Declaration("BodyMod","appearance",{{"MaxBytes",4097}});});
    Reject([&]{A::Payload(route,"[]");});Reject([&]{A::Payload(route,std::string(4097,' '));});
    Reject([&]{A::Payload(route,"{\"bad\":NaN}");});
    Reject([&]{A::Payload(route,std::string(34,'[')+"0"+std::string(34,']'));});
    A::Ledger ledger;const std::string guid="0123456789abcdef0123456789abcdef";
    const auto first=ledger.Put(route,"real-pawn-serial",guid,"{ \"bodyType\": \"male\", \"muscle\":50 }");
    assert(first.CharacterGuid=="0123456789ABCDEF0123456789ABCDEF"&&first.Revision==1&&ledger.Size()==1);
    assert(ledger.Put(route,"real-pawn-serial",guid,first.Payload).Revision==1);
    const auto next=ledger.Put(route,"real-pawn-serial",guid,"{\"muscle\":75}");assert(next.Revision==2&&ledger.Size()==1);
    A::Recipient recipient;
    assert(!recipient.Wants(next,"other-player",false));
    assert(recipient.Wants(next,"real-pawn-serial",false)&&recipient.Wants(next,"other-player",true));
    recipient.Subject(next.Subject,true);assert(recipient.Wants(next,"other-player",false));
    recipient.Seen[{next.Subject,next.Key}]=next.Revision;assert(!recipient.Wants(next,"other-player",false));
    recipient.Subject(next.Subject,false);assert(!recipient.Wants(next,"other-player",false)&&recipient.Seen.empty());
    recipient.Subject(next.Subject,true);assert(recipient.Wants(next,"other-player",false));
    recipient.CheckPublication(route.Key,1);recipient.Published[route.Key]=1;
    Reject([&]{recipient.CheckPublication(route.Key,1);});Reject([&]{recipient.CheckPublication(route.Key,0);});
    recipient.CheckPublication(route.Key,2);recipient.CheckPublication("other:appearance",1);
    ledger.Put(route,"other-pawn",guid,"{}");assert(ledger.Size()==2);
    const auto removed=ledger.Put(route,"real-pawn-serial",guid,"",true);assert(removed.Removed&&removed.Payload=="{}");
    assert(ledger.Snapshot().at({"real-pawn-serial",route.Key}).Removed);
    const auto before=ledger.StoredBytes();Reject([&]{ledger.Put(route,"real-pawn-serial",std::string(32,'0'),"{}");});assert(ledger.StoredBytes()==before);
    ledger.Remove("real-pawn-serial");assert(ledger.Size()==1&&ledger.StoredBytes()==2);
    for(size_t i=1;i<A::MaximumRecords;++i)ledger.Put(route,"pawn-"+std::to_string(i),guid,"{}");
    Reject([&]{ledger.Put(route,"one-too-many",guid,"{}");});assert(ledger.Size()==A::MaximumRecords);
    ledger.Clear();assert(ledger.Size()==0&&ledger.StoredBytes()==0&&ledger.Put(route,"new-world",guid,"{}").Revision==1);
}
