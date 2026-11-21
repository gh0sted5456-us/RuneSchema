#include "Core/GrandExchangeItems.h"
#undef NDEBUG
#include <cassert>
int main(){
    using J=nlohmann::json;
    J rows=J::array({{{"path","/Game/Mods/GE/Item.Item"},{"icon","/Game/Mods/GE/Icon.Icon"}},
        {{"path","/Game/Mods/GE/Item.Item"},{"icon","/Game/Mods/GE/Icon.Icon"}},
        {{"path","/Game/Mods/GE/Bad.Bad"},{"icon","https://example.com/icon"}},
        {{"path","/Game/Mods/GE/Bad.Bad"},{"icon","/Game/../Icon.Icon"}},{{"path",12},{"icon",false}}});
    const auto icons=PS::GrandExchangeItems::Icons({{"schema","grandexchange.items.index/1"},{"items",rows}});
    assert(icons.size()==1&&icons[0].second=="/Game/Mods/GE/Icon.Icon");
    bool refused=false;try{PS::GrandExchangeItems::Icons({{"schema","unknown"},{"items",rows}});}catch(...){refused=true;}assert(refused);
}
