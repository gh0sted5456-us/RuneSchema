#pragma once
#include <cstdint>
namespace DragonWilds::AppearanceEvents {
using Observer = void(*)(unsigned, uintptr_t, uint32_t);
enum class Consumer { Ghost, Trace };
void Subscribe(Consumer consumer, Observer observer);
void Unsubscribe(Consumer consumer);
}
