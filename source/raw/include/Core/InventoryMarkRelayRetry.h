#pragma once
#include <algorithm>
#include <chrono>
namespace PS {
struct InventoryMarkRelayRetry {
    unsigned Attempts=0,Deferred=0;
    bool Exhausted()const{return Attempts>=3;}
    std::chrono::seconds Next(bool forwarded){
        if(forwarded){++Attempts;Deferred=0;return std::chrono::seconds(2);}
        Deferred=std::min(Deferred+1,5u);
        return std::chrono::seconds(std::min(30u,1u<<Deferred));
    }
};
}
