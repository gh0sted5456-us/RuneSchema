#pragma once
#include "FakeHost.h"
namespace PS {
struct WeakObjectHandle {
    RC::Unreal::UObject* object=nullptr;
    int serial=0;
    void Assign(RC::Unreal::UObject* p){object=p;serial=p&&!p->failWeak?p->serial:0;}
    RC::Unreal::UObject* Get()const noexcept{return object&&serial&&object->serial==serial?object:nullptr;}
    void Reset()noexcept{object=nullptr;serial=0;}
};
}
