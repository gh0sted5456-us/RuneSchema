#pragma once
#include <array>
#include <algorithm>
#include <cstddef>
namespace PS::HelpyTaskbar {
enum class Window { ItemBay,IconCache,Map,Recipes,Runtime,Party,Inventory };
class State {
    std::array<bool,7> minimized{};
public:
    void Minimize(Window window){minimized.at(static_cast<size_t>(window))=true;}
    void Restore(Window window){minimized.at(static_cast<size_t>(window))=false;}
    void Forget(Window window){Restore(window);}
    bool Contains(Window window)const{return minimized.at(static_cast<size_t>(window));}
    size_t Count()const{return std::count(minimized.begin(),minimized.end(),true);}
};
inline State Current;
}
