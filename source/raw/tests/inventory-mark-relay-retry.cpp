#include "Core/InventoryMarkRelayRetry.h"
#include <cassert>
int main(){using namespace std::chrono_literals;PS::InventoryMarkRelayRetry retry;
    assert(retry.Next(false)==2s&&retry.Attempts==0);assert(retry.Next(false)==4s);assert(retry.Next(false)==8s);assert(retry.Next(false)==16s);assert(retry.Next(false)==30s);
    for(int i=0;i<100;++i)assert(retry.Next(false)==30s&&!retry.Exhausted()&&retry.Attempts==0);
    assert(retry.Next(true)==2s&&retry.Attempts==1&&retry.Deferred==0);assert(retry.Next(false)==2s&&retry.Attempts==1&&!retry.Exhausted());
    assert(retry.Next(true)==2s&&retry.Attempts==2);assert(retry.Next(true)==2s&&retry.Exhausted());
    retry={};assert(!retry.Exhausted()&&retry.Attempts==0&&retry.Deferred==0&&retry.Next(false)==2s);
}
