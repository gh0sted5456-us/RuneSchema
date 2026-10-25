#include "Core/ItemCart.h"
#include "Core/ItemBadges.h"
#include <cassert>
#include <limits>
int main(){
    PS::ItemCart::Model cart;
    const nlohmann::json row={{"Path","/Game/Items/A.A"},{"Name","A"},{"MaxStackSize",20}};
    assert(!cart.Add({}));assert(!cart.Add({{"Path","bare-name"}}));
    assert(!cart.Add({{"Path",std::string(1024,'/')}}));
    assert(cart.Add(row));assert(cart.Add(row));assert(cart.Entries().size()==1 && cart.Entries()[0].Quantity==2);
    cart.SetQuantity("/Game/Items/A.A",-10);assert(cart.Payload()[0]["Count"]==1);
    cart.SetQuantity("/Game/Items/A.A",20000);assert(cart.Payload()[0]["Count"]==10000);
    assert(cart.Payload(1)[0]["Count"]==1);assert(cart.Payload(2)[0]["Count"]==10);assert(cart.Payload(3)[0]["Count"]==20);
    assert(cart.Entries()[0].Row["Name"]=="A"); // metadata survives catalogue paging
    assert(cart.Remove("/Game/Items/A.A"));assert(!cart.Remove("missing"));
    for(std::size_t i=0;i<cart.Limit;++i)assert(cart.Add({{"Path","/Game/Items/I"+std::to_string(i)+".I"}}));
    assert(!cart.Add({{"Path","/Game/Items/Overflow.Overflow"}}));assert(cart.Add({{"Path","/Game/Items/I0.I"}}));
    cart.Clear();assert(cart.Payload().empty());
    assert(cart.Add({{"Path","/Game/Huge.Huge"},{"MaxStackSize",std::numeric_limits<std::uint64_t>::max()}}));assert(cart.Payload(3)[0]["Count"]==10000);
    cart.Clear();assert(cart.Add(row));assert(cart.Add({{"Path","/Game/Items/B.B"}}));
    assert(!cart.BeginDelivery(0));assert(cart.BeginDelivery(10));assert(!cart.BeginDelivery(11));
    assert(!cart.Add(row));assert(!cart.Remove("/Game/Items/A.A"));cart.Clear();cart.SetQuantity("/Game/Items/A.A",8);
    assert(cart.Entries().size()==2&&cart.Entries()[0].Quantity==1);
    const nlohmann::json partial={{"GrantResults",nlohmann::json::array({
        {{"Item","/Game/Items/A.A"},{"Count",1},{"State","Confirmed"}},
        {{"Item","/Game/Items/B.B"},{"Count",1},{"State","Unconfirmed"}}})}};
    assert(cart.CompleteDelivery(11,partial)==0&&cart.Delivery()==10);
    assert(cart.CompleteDelivery(10,partial)==1&&!cart.Delivery());
    assert(cart.Entries().size()==1&&cart.Entries()[0].Path=="/Game/Items/B.B");
    assert(cart.BeginDelivery(12));assert(cart.CompleteDelivery(12,{{"Status","Accepted"}})==0&&!cart.Delivery());
    assert(cart.Entries().size()==1);assert(cart.BeginDelivery(13));
    auto wrong=partial;wrong["GrantResults"][0]["Item"]="/Game/Items/B.B";wrong["GrantResults"][0]["Count"]=2;
    assert(cart.CompleteDelivery(13,wrong)==0&&cart.Entries().size()==1);
    assert(cart.BeginDelivery(14));auto duplicate=partial;duplicate["GrantResults"][0]["Item"]="/Game/Items/B.B";
    assert(cart.CompleteDelivery(14,duplicate)==0&&cart.Entries().size()==1);
    assert(cart.BeginDelivery(15));auto malformed=partial;malformed["GrantResults"]=nlohmann::json::array({{"not","a result"},{{"Item",4}}});
    assert(cart.CompleteDelivery(15,malformed)==0&&cart.Entries().size()==1);
    assert(cart.BeginDelivery(16));cart.CancelDelivery();assert(!cart.Delivery()&&cart.Entries().size()==1);
    assert(cart.BeginDelivery(17));const nlohmann::json complete={{"GrantResults",nlohmann::json::array({{{"Item","/Game/Items/B.B"},{"Count",1},{"State","Confirmed"}}})}};
    assert(cart.CompleteDelivery(17,complete)==1&&cart.Entries().empty());assert(cart.CompleteDelivery(17,complete)==0);
    assert(cart.Add(row));auto kit=PS::ItemCart::KitDocument(cart,"starter kit");
    assert(PS::ItemCart::KitItems(kit)==cart.Payload());
    // JSONC comments are data-only: parsing does not stage or deliver anything.
    const auto jsonc=nlohmann::json::parse("/* shared kit */"+kit.dump(),nullptr,true,true);assert(PS::ItemCart::KitItems(jsonc)==cart.Payload());
    const auto rejects=[](const nlohmann::json& value){try{(void)PS::ItemCart::KitItems(value);}catch(...){return true;}return false;};
    auto invalid=kit;invalid["Action"]="GiveItems";assert(rejects(invalid));
    invalid=kit;invalid["SchemaVersion"]=2;assert(rejects(invalid));
    invalid=kit;invalid["Items"][0]["Count"]=0;assert(rejects(invalid));
    invalid=kit;invalid["Items"][0]["Count"]=10001;assert(rejects(invalid));
    invalid=kit;invalid["Items"][0]["Count"]=1.5;assert(rejects(invalid));
    invalid=kit;invalid["Items"][0]["Item"]="/Game/Bad\n.Bad";assert(rejects(invalid));
    invalid=kit;invalid["Items"].push_back(invalid["Items"][0]);assert(rejects(invalid));
    invalid=kit;invalid["Items"][0]["Script"]="run()";assert(rejects(invalid));
    invalid=kit;invalid["Name"]=std::string(129,'x');assert(rejects(invalid));
    invalid=kit;invalid["Items"]=nlohmann::json::array();assert(rejects(invalid));
    {
        PS::ItemCart::Model recent;
        assert(recent.Add(row));recent.SetQuantity("/Game/Items/A.A",3);
        assert(recent.Add({{"Path","/Game/Items/B.B"}}));
        assert(recent.BeginDelivery(100,3));
        assert(!recent.Reorder());
        assert(recent.CompleteDelivery(100,{{"GrantResults",nlohmann::json::array({
            {{"Item","/Game/Items/A.A"},{"Count",20},{"State","Confirmed"}},
            {{"Item","/Game/Items/B.B"},{"Count",1},{"State","Unconfirmed"}}})}})==1);
        assert(recent.Outcomes()[0].Item.Quantity==20);
        assert(!recent.Reorder("/Game/Items/B.B"));
        assert(recent.Reorder("/Game/Items/A.A")&&!recent.Delivery());
        assert(recent.Payload().size()==2&&recent.Payload()[1]["Count"]==20);
        recent.ApplyValidation(nlohmann::json::array({{{"Item","/Game/Items/A.A"},{"Valid",true}}}));
        assert(recent.Reorder()&&recent.Payload()[1]["Count"]==40&&!recent.Validation("/Game/Items/A.A"));
        assert(recent.Outcomes().size()==2); // Reorder cannot invalidate rendered receipt rows.
        recent.SetQuantity("/Game/Items/A.A",10000);const auto full=recent.Payload();
        assert(!recent.Reorder()&&recent.Payload()==full);
        assert(recent.Remove("/Game/Items/A.A"));
        for(size_t i=recent.Entries().size();i<recent.Limit;++i)assert(recent.Add({{"Path","/Game/Items/F"+std::to_string(i)+".F"}}));
        const auto capacity=recent.Payload();assert(!recent.Reorder()&&recent.Payload()==capacity);
        recent.Clear();assert(!recent.Reorder());
    }
    assert(!PS::ItemBadges::Masterwork({{"Name","Masterwork Axe"},{"PowerLevel",100}}));
    assert(PS::ItemBadges::Masterwork({{"Masterwork",true}}));assert(!PS::ItemBadges::Masterwork({{"Masterwork",1}}));
    assert(PS::ItemBadges::Power({{"PowerLevel",25}})==25);assert(!PS::ItemBadges::Power({{"PowerLevel",-1}}));
    assert(!PS::ItemBadges::Power({{"PowerLevel","25"}}));assert(!PS::ItemBadges::Power({{"PowerLevel",std::numeric_limits<double>::infinity()}}));
}
