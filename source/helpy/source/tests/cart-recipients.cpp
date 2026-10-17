#include "Core/CartDeliveryBatch.h"
#include <cassert>
using namespace PS::CartDeliveryBatch;
template<class F> void Reject(F fn){bool failed=false;try{fn();}catch(...){failed=true;}assert(failed);}
int main(){
    assert(!MissingReceiptExpired(31,true)&&!MissingReceiptExpired(300,true)&&!MissingReceiptExpired(30,false)&&MissingReceiptExpired(31,false));
    const Json items=Json::array({{{"Item","/Game/Items/A.A"},{"Count",3}},{{"Item","/Game/Items/B.B"},{"Count",1}}});
    const std::vector<Recipient> players={{"/Game/Players/A.A","Alice"},{"/Game/Players/B.B","Bob"}};
    const auto receipt=[&](bool ok){Json rows=items;for(auto& row:rows)row["State"]=ok?"Confirmed":"Unconfirmed";return Json{{"GrantResults",rows}};};
    Plan batch;batch.Begin(items,players);assert(!batch.Finished()&&!batch.Pending()&&batch.Total()==2);
    auto first=batch.Request(1);assert(first["Player"]==players[0].Path&&first["Items"]==items);Reject([&]{batch.Request(2);});
    assert(!batch.Accept(9,receipt(true))&&batch.Pending());assert(batch.Accept(1,receipt(true))&&batch.Completed()==1&&!batch.Finished());
    assert(!batch.Accept(1,receipt(true)));assert(batch.Request(2)["Player"]==players[1].Path);assert(batch.Accept(2,receipt(true))&&batch.Finished()&&!batch.Stopped());
    PS::ItemCart::Model cart;assert(cart.Add({{"Path","/Game/Items/A.A"}}));cart.SetQuantity("/Game/Items/A.A",3);assert(cart.Add({{"Path","/Game/Items/B.B"}}));
    assert(cart.BeginDelivery(10));assert(cart.CompleteDelivery(10,batch.Aggregate())==2&&cart.Entries().empty());
    batch.Begin(items,players);batch.Request(3);batch.Accept(3,receipt(true));batch.Request(4);auto partial=receipt(true);partial["GrantResults"][1]["State"]="Unconfirmed";
    batch.Accept(4,partial);assert(batch.Finished()&&batch.Stopped());assert(batch.Aggregate()["GrantResults"][0]["State"]=="Confirmed"&&batch.Aggregate()["GrantResults"][1]["State"]=="Unconfirmed");
    batch.Begin(items,players);batch.Request(5);batch.Accept(5,receipt(false));assert(batch.Stopped()&&batch.Completed()==1);Reject([&]{batch.Request(6);});
    assert(batch.Aggregate()["GrantResults"][0]["State"]=="Unconfirmed");
    batch.Begin(items,players);batch.Request(7);batch.Stop();assert(batch.Stopped()&&!batch.Pending());assert(!batch.Accept(7,receipt(true)));
    auto duplicate=receipt(true);duplicate["GrantResults"].push_back(duplicate["GrantResults"][0]);assert(!Plan::Confirmed(duplicate,items[0]));
    assert(!Plan::Confirmed({{"Status","Accepted by server"}},items[0]));
    Reject([&]{batch.Begin(items,{});});Reject([&]{batch.Begin(items,{{"guid1","A"},{"guid2","B"}});});Reject([&]{batch.Begin(items,{players[0],players[0]});});
    batch.Begin(items,{{"guid1","Self-only remote"}});assert(batch.Request(8)["Player"]=="guid1");batch.Stop();
    auto excess=players;for(size_t i=0;i<Plan::MaxPlayers;++i)excess.push_back({"/Game/Players/Extra"+std::to_string(i),"Extra"});Reject([&]{batch.Begin(items,excess);});
}
