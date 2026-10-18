#pragma once
#include <HAL/Platform.hpp>
#include <format>
#include <vector>
namespace RC::Output {
inline std::vector<std::wstring> Rows;
template<int Level,class... Args> void send(std::wstring_view content,Args&&... args){
    Rows.push_back(std::vformat(content,std::make_wformat_args(args...)));
    const auto consume=[]<class T>(T&& value){if constexpr(std::is_same_v<std::remove_cvref_t<T>,std::wstring>&&!std::is_lvalue_reference_v<T>)value.clear();};
    (consume(std::forward<Args>(args)),...);
}
}
