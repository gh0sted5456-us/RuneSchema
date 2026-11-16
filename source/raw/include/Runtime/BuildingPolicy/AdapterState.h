#pragma once
#include <exception>

namespace PS::Buildings {
struct AdapterState {
    bool Unavailable=false;
    template<class Work,class Report> void Update(Work&& work,Report&& report){
        if(Unavailable)return;
        try{work();}catch(const std::exception& error){Unavailable=true;report(error.what());}
    }
};
}
