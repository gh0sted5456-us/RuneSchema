#include "Core/MapTravel.h"
#include <limits>
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("Map travel regression");}
int main(){
    using Probe=PS::MapTravel::LandingProbe;
    const PS::MapProjection::Projection map{{0,0},{1,0},{0,1},10000,10000,-1000,1000};
    Probe probe;unsigned queries=0;bool finished=false;
    const auto floor=[&](PS::MapProjection::Point point,double top,double bottom,std::optional<double> reference){
        Check(top==1000&&bottom==-1000);
        Check(queries==0?!reference:(reference&&*reference==100));
        ++queries;return PS::MapLanding::Position{point.X,point.Y,100};
    };
    for(unsigned slice=0;slice<3;++slice){
        const auto before=queries;
        try{const auto landing=probe.Resolve(map,{100,200},40,90,floor);Check(slice==2&&landing.X==100&&landing.Y==200&&landing.Z==198);finished=true;}
        catch(const Probe::Yield&){Check(slice<2);}
        Check(queries-before<=2);
    }
    Check(finished&&queries==5&&probe.Count==5);
    (void)probe.Resolve(map,{100,200},40,90,floor);Check(queries==5);
    probe={};
    try{(void)probe.Resolve(map,{0,0},40,90,[&](auto point,double,double,std::optional<double>){
        if(probe.Count==1)throw Probe::Yield{};
        return PS::MapLanding::Position{point.X,point.Y,100};
    });Check(false);}catch(const Probe::Yield&){}
    Check(probe.Count==1); // A cooperative time-budget yield retains the centre.
    unsigned resumed=0;
    for(int slice=0;slice<2;++slice)try{(void)probe.Resolve(map,{0,0},40,90,[&](auto point,double,double,auto reference){
        Check(reference&&*reference==100);++resumed;return PS::MapLanding::Position{point.X,point.Y,100};
    });}catch(const Probe::Yield&){}
    Check(probe.Count==5&&resumed==4);
    probe={};bool rejected=false;
    try{(void)probe.Resolve(map,{0,0},40,90,[](auto point,double,double,auto){return PS::MapLanding::Position{point.X,point.Y,std::numeric_limits<double>::quiet_NaN()};});}
    catch(const std::runtime_error&){rejected=true;}
    Check(rejected);
    probe={};rejected=false;
    for(int slice=0;slice<3&&!rejected;++slice)try{(void)probe.Resolve(map,{0,0},40,90,[](auto point,double,double,auto){return PS::MapLanding::Position{point.X,point.Y,point.X>0?200.:100.};});}
    catch(const Probe::Yield&){}catch(const std::runtime_error&){rejected=true;}
    Check(rejected); // Resuming a footprint must retain ledge/slope rejection.
    using Gate=PS::MapTravel::Gate;using Step=Gate::Step;
    Gate job(1000);Check(!job.Commit(1000));
    Check(job.Poll(1100,true,true)==Step::Wait);Check(job.Poll(1200,true,false)==Step::Wait);
    Check(job.Poll(1300,true,true)==Step::Wait);Check(job.Poll(1400,true,true)==Step::Probe);
    Check(job.Commit(1400));Check(!job.Commit(1401));Check(job.Poll(30000,true,false)==Step::Wait);
    Check(job.Poll(61400,true,true)==Step::Expired);
    Gate timeout(0);Check(timeout.Poll(30000,true,true)==Step::Expired);Check(!timeout.Commit(30000));
    Gate changed(500);Check(changed.Poll(600,false,true)==Step::Invalidated);Check(changed.Poll(499,true,true)==Step::Invalidated);
    Check(!changed.Commit(1000));Check(changed.Poll(1500,true,true)==Step::Invalidated);
    Gate departed(0);Check(departed.Poll(200,true,true)==Step::Wait);Check(departed.Poll(300,true,true)==Step::Probe);
    Check(departed.Poll(400,false,true)==Step::Invalidated);Check(!departed.Commit(500));
    Check(PS::MapTravel::Arrived({100,200,300},{120,190,310},90));
    Check(!PS::MapTravel::Arrived({100,200,300},{151,200,300},90));
    Check(!PS::MapTravel::Arrived({100,200,300},{100,200,391},90));
    Check(!PS::MapTravel::Arrived({100,200,300},{100,200,std::numeric_limits<double>::quiet_NaN()},90));
}
