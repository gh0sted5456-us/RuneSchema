#pragma once
#include <stdexcept>
#include <string>

namespace DragonWilds::TimeOfDay {
enum class Requirement { Any, Day, Night };
inline Requirement Parse(const std::string& value) {
    if(value.empty() || value=="Any")return Requirement::Any;
    if(value=="Day")return Requirement::Day;
    if(value=="Night")return Requirement::Night;
    throw std::runtime_error("TimeOfDay must be Any, Day, or Night");
}
inline const char* Name(Requirement value) {
    switch(value){case Requirement::Any:return "Any";case Requirement::Day:return "Day";case Requirement::Night:return "Night";}
    throw std::runtime_error("Invalid time-of-day requirement");
}
}
