#include "Core/MapTravel.h"
#include <limits>
#include <stdexcept>
static void Check(bool value){if(!value)throw std::runtime_error("Map travel regression");}
int main(){
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
