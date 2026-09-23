#pragma once
// Pure authoring conversion: GUI values to JSON literals. Never touches game objects.
#include <charconv>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace PS::HelpyPropertyValue {
inline std::string Kind(std::string type) {
    if(type=="bool")return "Boolean";
    if(type=="FText"||type=="FString"||type=="FName"||type=="Text"||type=="String")return "Text";
    if(type=="float"||type=="double")return "Number";
    if(type=="uint8"||type=="uint16"||type=="uint32"||type=="uint64")return "Unsigned";
    if(type=="int8"||type=="int16"||type=="int32"||type=="int64"||type=="int")return "Integer";
    return "JSON";
}
inline std::string Quote(std::string_view input) {
    std::string out="\"";constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:input) {
        switch(c) {
        case '"':out+="\\\"";break;
        case '\\':out+="\\\\";break;
        case '\n':out+="\\n";break;
        case '\r':out+="\\r";break;
        case '\t':out+="\\t";break;
        default:if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=static_cast<char>(c);
        }
    }
    return out+'"';
}
inline void Codepoint(std::string& out,uint32_t cp) {
    if(cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))throw std::runtime_error("Invalid Unicode escape.");
    if(cp<0x80)out+=static_cast<char>(cp);
    else if(cp<0x800){out+=static_cast<char>(0xc0|(cp>>6));out+=static_cast<char>(0x80|(cp&63));}
    else if(cp<0x10000){out+=static_cast<char>(0xe0|(cp>>12));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
    else {out+=static_cast<char>(0xf0|(cp>>18));out+=static_cast<char>(0x80|((cp>>12)&63));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
}
inline std::string Unquote(std::string_view text) {
    if(text.size()<2||text.front()!='"'||text.back()!='"')throw std::runtime_error("Expected a JSON string.");
    const auto end=text.size()-1;std::string out;
    const auto hex4=[&](size_t& at){
        if(at+4>end)throw std::runtime_error("Incomplete Unicode escape.");
        uint32_t n=0;
        for(int j=0;j<4;++j){const char c=text[at++];int v=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(v<0)throw std::runtime_error("Invalid Unicode escape.");n=(n<<4)|static_cast<uint32_t>(v);}
        return n;
    };
    for(size_t i=1;i<end;) {
        const auto c=static_cast<unsigned char>(text[i++]);
        if(c=='"'||c<32)throw std::runtime_error("Invalid character in JSON string.");
        if(c!='\\'){out+=static_cast<char>(c);continue;}
        if(i==end)throw std::runtime_error("Incomplete JSON escape.");
        switch(text[i++]) {
        case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
        case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
        case 'u':{auto cp=hex4(i);if(cp>=0xd800&&cp<=0xdbff){if(i+2>end||text[i]!='\\'||text[i+1]!='u')throw std::runtime_error("Missing Unicode low surrogate.");i+=2;const auto lo=hex4(i);if(lo<0xdc00||lo>0xdfff)throw std::runtime_error("Invalid Unicode low surrogate.");cp=0x10000+((cp-0xd800)<<10)+(lo-0xdc00);}Codepoint(out,cp);break;}
        default:throw std::runtime_error("Invalid JSON escape.");
        }
    }
    return out;
}
inline bool IsText(std::string_view kind) {return kind=="Text"||kind=="Reference"||kind=="Enum";}
inline std::string Encode(std::string_view value,std::string_view kind) {
    if(IsText(kind))return Quote(value);
    if(kind=="Boolean") {if(value!="true"&&value!="false")throw std::runtime_error("Choose true or false.");return std::string(value);}
    if(kind=="Integer") {
        int64_t n=0;const auto r=std::from_chars(value.data(),value.data()+value.size(),n);
        if(value.empty()||r.ec!=std::errc{}||r.ptr!=value.data()+value.size())throw std::runtime_error("Enter a whole number within the signed 64-bit range.");
        return std::to_string(n);
    }
    if(kind=="Unsigned") {
        // The existing native numeric writer uses int64; never promise a larger range.
        int64_t n=0;const auto r=std::from_chars(value.data(),value.data()+value.size(),n);
        if(value.empty()||r.ec!=std::errc{}||r.ptr!=value.data()+value.size()||n<0)throw std::runtime_error("Enter a non-negative whole number within the signed 64-bit range.");
        return std::to_string(n);
    }
    if(kind=="Number") {
        double n=0;const auto r=std::from_chars(value.data(),value.data()+value.size(),n);
        if(value.empty()||r.ec!=std::errc{}||r.ptr!=value.data()+value.size()||!std::isfinite(n))throw std::runtime_error("Enter a finite number.");
        char out[64];const auto f=std::to_chars(out,out+sizeof(out),n,std::chars_format::general);
        if(f.ec!=std::errc{})throw std::runtime_error("Number conversion failed.");
        return std::string(out,f.ptr);
    }
    if(value.empty())throw std::runtime_error("Enter a JSON value or choose Inherit source.");
    return std::string(value); // Composite JSON is parsed and checked by the game-thread authoring backend.
}
inline std::string Decode(std::string_view value,std::string_view kind) {
    return IsText(kind)&&value!="null"?Unquote(value):std::string(value);
}
}
