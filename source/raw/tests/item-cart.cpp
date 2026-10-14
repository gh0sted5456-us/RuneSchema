#include "Core/ItemCart.h"
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
}
