#pragma once
struct SafetyHookInline {
    bool installed=false;
    explicit operator bool()const{return installed;}
    template<class...T>void call(T...){ }
};
