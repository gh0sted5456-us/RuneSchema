#include "Core/ItemDelivery.h"
#include <cassert>
using namespace PS::ItemDelivery;
using nlohmann::json;

int main(){
    const auto items=Read("ItemDelivery",json::parse(R"({
        "starter": ["/Game/Items/ITEM_A.ITEM_A", "/game/items/item_a.item_a", "/Game/Items/ITEM_B.ITEM_B"],
        "returning": ["/Game/Items/ITEM_A.ITEM_A"]
    })"));
    assert(items.size()==3&&items[1].Key()!=items[2].Key());
    assert(!AssetPath("/Game/Items/ITEM_A")&&!AssetPath("/Game/../Invalid.Invalid"));
    assert(!AssetPath("/Game//Items.Invalid")&&!AssetPath("/Game/Items.Asset/Sub"));
    const auto key=items[0].Key();
    Receipts journal(Receipts::Empty());
    assert(journal.Hold(key,false));
    Receipts afterCrash(journal.Document());
    assert(!afterCrash.CanAttempt(key,false)&&!afterCrash.CanAttempt(key,true));
    assert(afterCrash.CanAttempt(items[1].Key(),false)); // New item added to an existing delivery.
    SaveReceipt save;
    save.Dispatch({5,"slot-a"});
    save.Complete({4,"slot-a"},1);
    save.Complete({5,"slot-b"},1);
    assert(!save.Confirmed()&&!save.Result);
    save.Complete({5,"slot-a"},0);
    assert(save.Result==false&&!save.Confirmed());
    SaveReceipt accepted;
    accepted.Dispatch({6,"slot-a"});
    accepted.Complete({6,"slot-a"},1);
    assert(accepted.Confirmed()&&afterCrash.Commit(key));
    assert(!afterCrash.CanAttempt(key,false)&&afterCrash.CanAttempt(key,true));
    assert(afterCrash.Hold(key,true));
    assert(!Receipts(afterCrash.Document()).CanAttempt(key,true));
    SaveReceipt ambiguous;
    ambiguous.Dispatch({7,"slot-a"});
    ambiguous.Dispatch({8,"slot-a"});
    ambiguous.Complete({7,"slot-a"},1);
    assert(!ambiguous.Confirmed());
    SaveReceipt opaque;
    opaque.OpaqueSlot="slot-a";
    opaque.DispatchOpaque(true);
    opaque.Complete({10,"slot-b"},1);
    assert(!opaque.Confirmed());
    opaque.Complete({10,"slot-a"},1);
    assert(opaque.Confirmed());
    SaveReceipt overlap;
    overlap.OpaqueSlot="slot-a";
    overlap.DispatchOpaque(false);
    overlap.Complete({11,"slot-a"},1);
    assert(!overlap.Confirmed());
}
