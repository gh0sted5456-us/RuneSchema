#include "Core/ConsumedOwnerPresentation.h"
#include <cassert>
#include <stdexcept>
using namespace PS::ConsumedOwnerPresentation;
int main(){
    using Json=nlohmann::json;
    const Json authority={{"Action","ConsumedItemAuthority"},{"OwnerPresentation",{{"Connection","editor"},{"Key","open-editor"}}}};
    const Json consumer={{"Audience","Owner"},{"Connection","editor"},{"Function","OpenForController"},{"AllowedKeys",{"open-editor"}}};
    ValidateDeclaration(authority,consumer);
    auto world=consumer;world["Audience"]="World";
    auto wrongConnection=consumer;wrongConnection["Connection"]="other";
    auto wrongKey=consumer;wrongKey["AllowedKeys"]={"other-key"};
    auto noCallback=consumer;noCallback.erase("Function");
    for(const auto& invalid:{Json(),world,wrongConnection,wrongKey,noCallback}){
        bool rejected=false;try{ValidateDeclaration(authority,invalid);}catch(const std::exception&){rejected=true;}assert(rejected);
    }
    auto foreign=authority;foreign["Action"]="ExecuteGraph";
    bool rejected=false;try{ValidateDeclaration(foreign,consumer);}catch(const std::exception&){rejected=true;}assert(rejected);
    rejected=false;try{ValidateDeclaration(Json::object(),consumer);}catch(const std::exception&){rejected=true;}assert(rejected);
    const auto now=Clock::now();const auto later=now+std::chrono::seconds(6);
    Ledger ledger;
    // Either native event order requires both successful callback and removal.
    ledger.NativeCompleted("a","mod:potion",now);assert(ledger.TakeReady(now).empty());
    const auto one=ledger.Removed("a","mod:potion",now);
    auto ready=ledger.TakeReady(now);assert(ready.size()==1&&ready[0].Id==one);
    assert(ledger.TakeReady(now).empty());
    const auto ack=ledger.ClaimBridge("a","mod:potion",now);assert(ack&&ack->AlreadyExecuted&&ack->Id==one);
    ledger.BridgeCompleted("a",one,now);assert(ledger.TakeReady(now).empty());
    const auto two=ledger.Removed("a","mod:potion",now);assert(ledger.TakeReady(now).empty());
    ledger.NativeCompleted("a","mod:potion",now);
    ready=ledger.TakeReady(now);assert(ready.size()==1&&ready[0].Id==two);
    // A remote authority request has no permission until its own removal exists.
    assert(!ledger.ClaimBridge("b","mod:potion",now));
    const auto three=ledger.Removed("b","mod:potion",now);
    assert(!ledger.ClaimBridge("a","other:potion",now));
    auto claim=ledger.ClaimBridge("b","mod:potion",now);assert(claim&&claim->Id==three&&!claim->AlreadyExecuted);
    assert(ledger.TakeReady(now).empty());ledger.BridgeCompleted("b",three,now);
    ready=ledger.TakeReady(now);assert(ready.size()==1&&ready[0].Owner=="b");
    assert(!ledger.ClaimBridge("b","mod:potion",now));
    ledger.NativeCompleted("b","mod:potion",now);assert(ledger.TakeReady(now).empty());
    bool refused=false;try{ledger.BridgeCompleted("a",three,now);}catch(const std::runtime_error&){refused=true;}assert(refused);
    // Expiry and world reset cannot leave an observation available to a new pawn.
    ledger.NativeCompleted("c","mod:potion",now);ledger.Removed("d","mod:potion",now);
    assert(ledger.TakeReady(later).empty());assert(!ledger.ClaimBridge("d","mod:potion",later));
    ledger.Removed("c","mod:potion",later);assert(ledger.TakeReady(later).empty());
    ledger.Clear();ledger.Removed("c","mod:potion",later);assert(ledger.TakeReady(later).empty());
    // Independent owning components and out-of-order ready callbacks deduplicate.
    ReplayWindow replay;
    assert(replay.Accept("local-component-a",20));assert(replay.Accept("local-component-a",19));
    assert(!replay.Accept("local-component-a",20));assert(!replay.Accept("local-component-a",19));
    assert(replay.Accept("local-component-b",20));assert(!replay.Accept("",1));assert(!replay.Accept("local-component-a",0));
    assert(replay.Accept("local-component-a",10000));assert(!replay.Accept("local-component-a",20));
    replay.Clear();assert(replay.Accept("local-component-a",20));
}
