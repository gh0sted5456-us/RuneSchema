#include "Core/ItemPaging.h"
#include "Core/ItemPopulation.h"
#include "Core/LoadingIndicator.h"
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
    using PS::ItemPopulation::Describe;
    assert(Describe("checking-cache",false,0,{},false).Busy);
    assert(Describe("discovering-items",false,0,{},false).Label=="Discovering items...");
    assert(Describe("discovering-items",false,12,{},false).Label.find("12 found")!=std::string::npos);
    for(const float angle:{0.f,1.f,3.14f,10.f}){const auto quad=PS::LoadingIndicator::Quad({200,150},13,angle);float x=0,y=0;for(const auto& p:quad){x+=p.X;y+=p.Y;}assert(std::abs(x/4-200)<.001f&&std::abs(y/4-150)<.001f);}
    assert(Describe("reading-catalogue",false,0,{},false).Label=="Loading cached items...");
    const auto icons=Describe("cached-items",true,100,{3,5,0},false);assert(icons.Busy&&icons.Fraction==.6f&&icons.Label.find("3 / 5")!=std::string::npos);
    assert(!Describe("items-ready",true,100,{4,5,1},false).Busy);
    assert(!Describe("items-ready",true,100,{5,5,0},true).Busy);
    assert(Describe("items-ready",true,100,{5,5,0},true).Label.find("checking source")!=std::string::npos);
    const auto search=Describe("filtering-cache",false,2048,{},false,4096);assert(search.Busy&&search.Determinate&&search.Fraction==.5f);
    assert(Describe("searching-live-items",false,0,{},false).Busy);
    assert(Describe("searching-live-items",true,12,{},false).Label.find("Showing matches")!=std::string::npos);
}
