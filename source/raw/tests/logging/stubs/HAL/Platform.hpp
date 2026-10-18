#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#define STR(text) L##text
namespace RC {
using CharType=wchar_t;using StringType=std::wstring;
namespace File {using StringViewType=std::wstring_view;}
namespace Unreal {using int32=std::int32_t;}
namespace LogLevel {inline constexpr int Normal=0,Warning=1,Error=2,Verbose=3;}
}
