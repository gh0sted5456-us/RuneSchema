#pragma once
#include <string_view>

namespace PS::HelpyShortcut {
struct Binding {std::string_view Name;int Key;};
inline constexpr Binding Choices[]{
    {"F1",0x70},{"F2",0x71},{"F3",0x72},{"F4",0x73},{"F5",0x74},
    {"F6",0x75},{"F7",0x76},{"F8",0x77},{"F9",0x78},{"F10",0x79},{"F11",0x7A},{"F12",0x7B},
    {"F13",0x7C},{"F14",0x7D},{"F15",0x7E},{"F16",0x7F},{"F17",0x80},{"F18",0x81},
    {"F19",0x82},{"F20",0x83},{"F21",0x84},{"F22",0x85},{"F23",0x86},{"F24",0x87},
    {"Insert",0x2D},{"Home",0x24},{"End",0x23},{"PageUp",0x21},{"PageDown",0x22},{"Pause",0x13},{"ScrollLock",0x91},
    {"A",'A'},{"B",'B'},{"C",'C'},{"D",'D'},{"E",'E'},{"F",'F'},{"G",'G'},{"H",'H'},{"I",'I'},
    {"J",'J'},{"K",'K'},{"L",'L'},{"M",'M'},{"N",'N'},{"O",'O'},{"P",'P'},{"Q",'Q'},{"R",'R'},
    {"S",'S'},{"T",'T'},{"U",'U'},{"V",'V'},{"W",'W'},{"X",'X'},{"Y",'Y'},{"Z",'Z'},
    {"0",'0'},{"1",'1'},{"2",'2'},{"3",'3'},{"4",'4'},{"5",'5'},{"6",'6'},{"7",'7'},{"8",'8'},{"9",'9'}
};
inline constexpr int DefaultKey=0x71;
inline constexpr char Upper(char c){return c>='a'&&c<='z'?c-'a'+'A':c;}
inline constexpr const Binding* Find(std::string_view name){
    for(const auto& choice:Choices){
        if(choice.Name.size()!=name.size())continue;
        bool same=true;for(std::size_t i=0;i<name.size();++i)if(Upper(name[i])!=Upper(choice.Name[i])){same=false;break;}
        if(same)return &choice;
    }return nullptr;
}
inline constexpr std::string_view Name(int key){for(const auto& choice:Choices)if(choice.Key==key)return choice.Name;return "F2";}
}
