
#include <vector>
#include <unordered_map>
#include <functional>
#include <cassert>
#include <cstdint>
#include <iostream>
struct UObject { int32_t index; bool table=true; int32_t GetInternalIndex(){return index;} bool IsA(int){return table;} };
struct UDataTable:UObject { static int StaticClass(){return 1;} };
struct Item { UObject* object; int32_t serial; bool valid=true; UObject* GetUObject(){return object;} int32_t GetSerialNumber(){return serial;} bool IsValid(bool){return valid;} };
std::vector<Item> slots;
struct FUObjectArray { static Item* IndexToObject(int32_t i){return i>=0 && i<(int)slots.size()?&slots[i]:nullptr;} };
enum class LoopAction{Continue,Break};
size_t visited=0;
struct UObjectGlobals { template<class F> static void ForEachUObject(F f){for(auto& x:slots)if(x.object){++visited;if(f(x.object,0,0)==LoopAction::Break)break;}} };
struct Registry {std::vector<UDataTable*> restored; std::function<void()> onAdd; void Add(UDataTable* p){restored.push_back(p);if(onAdd)onAdd();}};
size_t replay(std::vector<UDataTable*> pending, Registry& m_datatableRegistry){
            // Serial zero is valid for live objects, but UE4SS weak pointers reject it.
            struct ReplayIdentity { int32_t Index = -1; int32_t Serial = 0; };
            std::unordered_map<const void*, ReplayIdentity> live;
            for (auto* table : pending) live.emplace(table, ReplayIdentity{});
            if (!live.empty()) {
                size_t remaining = live.size();
                UObjectGlobals::ForEachUObject([&](UObject* object, int32_t, int32_t) -> LoopAction {
                    const auto found = live.find(object);
                    if (found != live.end() && found->second.Index < 0 && object->IsA(UDataTable::StaticClass())) {
                        const auto index = object->GetInternalIndex();
                        auto* item = FUObjectArray::IndexToObject(index);
                        if (item && item->GetUObject() == object && item->IsValid(false)) {
                            found->second = {index, item->GetSerialNumber()};
                            if (--remaining == 0) return LoopAction::Break;
                        }
                    }
                    return LoopAction::Continue;
                });
            }
            size_t replayed = 0, zeroSerial = 0;
            for (auto* address : pending) {
                const auto identity = live.at(address);
                if (identity.Index < 0) continue;
                auto* item = FUObjectArray::IndexToObject(identity.Index);
                if (!item || item->GetUObject() != address ||
                    item->GetSerialNumber() != identity.Serial || !item->IsValid(false)) continue;
                m_datatableRegistry.Add(static_cast<UDataTable*>(item->GetUObject()));
                ++replayed;
                if (identity.Serial == 0) ++zeroSerial;
            }
return replayed;
}
int main(){
 UDataTable a{{0}},b{{1}},replacement{{1}},missing{{9}};
 slots={{&a,0},{&b,42}}; Registry r;
 assert(replay({&a,&b},r)==2); assert((r.restored==std::vector<UDataTable*>{&a,&b}));
 r={};slots[1].valid=false;assert(replay({&a,&b,&missing},r)==1);
 r={};slots={{&a,0},{&b,42}};r.onAdd=[&]{slots[1].object=&replacement;};assert(replay({&a,&b},r)==1);
 r={};slots={{&a,0},{&b,42}};r.onAdd=[&]{slots[1].serial=43;};assert(replay({&a,&b},r)==1);
 r={};slots={{&a,0},{&b,42}};r.onAdd=[&]{slots[1].valid=false;};assert(replay({&a,&b},r)==1);
 r={};slots={{&a,0},{&b,42}};b.table=false;assert(replay({&a,&b},r)==1);
 b.table=true;
 UDataTable tail{{2}};
 slots={{&a,0},{&b,42},{&tail,1}};r={};visited=0;
 assert(replay({&a,&b},r)==2 && visited==2);
 r={};visited=0;assert(replay({},r)==0 && visited==0);
 r={};visited=0;assert(replay({&a,&missing},r)==1 && visited==3);
 slots={{&a,0},{&a,0},{&tail,1},{&b,42}};r={};visited=0;
 b.index=3;assert(replay({&a,&b},r)==2 && visited==4);
 std::cout<<"Extracted replay: zero/nonzero serial, order, missing, replaced, changed serial, invalidated and non-table cases passed\n";
}
