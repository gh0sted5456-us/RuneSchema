#include "Core/ItemPaging.h"
#include <cassert>
int main(){using namespace PS::ItemPaging;
    assert(DefaultSize==20&&Size(0)==1&&Size(500)==100);
    auto page=Slice(100,0,20,false);assert(page.First==0&&page.Count==20&&!page.Previous&&page.Next);
    page=Slice(43,2,20,false);assert(page.First==40&&page.Count==3&&page.Previous&&!page.Next);
    page=Slice(28,1,20,true);assert(page.First==20&&page.Count==8&&page.Previous&&page.Next);
    page=Slice(0,0,20,true);assert(page.Count==0&&!page.Previous&&page.Next);
    page=Slice(0,0,20,false);assert(!page.Next);
    assert(Resize(2,20,40)==1&&Resize(2,20,10)==4&&Resize(2,20,100)==0);
    assert(CartHeight(0,false)<CartHeight(1,false));
    assert(CartHeight(0,false)<=100&&CartHeight(1,true)>CartHeight(1,false));
}
