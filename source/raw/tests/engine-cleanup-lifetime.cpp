#include "Utility/EngineCleanupLifetime.h"
#include <memory>
#include <vector>
#include <stdexcept>
#include <iostream>
int main() {
    PS::EngineCleanupLifetime lifetime;
    unsigned rootCalls=0,weakCalls=0,destroyed=0;
    struct Lease {
        PS::EngineCleanupLifetime& life;unsigned& weak;unsigned& destroyed;
        ~Lease(){life.Run([&]{++weak;});++destroyed;}
    };
    struct State {
        PS::EngineCleanupLifetime& life;unsigned& roots;
        std::shared_ptr<Lease> lease;
        ~State(){life.Run([&]{++roots;});}
    };
    auto make=[&]{return std::unique_ptr<State>(new State{lifetime,rootCalls,std::shared_ptr<Lease>(new Lease{lifetime,weakCalls,destroyed})});};
    {auto live=make();}
    if(rootCalls!=1 || weakCalls!=1 || destroyed!=1)throw std::runtime_error("Live cleanup lost");
    {
        std::vector<std::unique_ptr<State>> states;states.push_back(make());
        // Same declaration/destruction order as PlayerGhost's global owners.
        PS::StopStaticEngineCleanup barrier{lifetime};
    }
    if(rootCalls!=1 || weakCalls!=1 || destroyed!=2)throw std::runtime_error("Late engine access or retained C++ storage");
    lifetime.Stop();lifetime.Run([&]{throw std::runtime_error("Cleanup restarted");});
    std::cout<<"PASS: live cleanup runs, shutdown suppresses root/weak engine calls, C++ owners release, repeated stop is safe.\n";
}
