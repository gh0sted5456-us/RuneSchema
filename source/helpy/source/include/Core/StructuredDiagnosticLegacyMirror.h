#pragma once
#include "Core/StructuredDiagnosticContract.h"
#include <algorithm>
#include <string>
#include <string_view>

namespace PS::StructuredDiagnosticLegacyMirror {
inline constexpr std::string_view Marker="[diag-event=";
inline bool SafeReference(std::string_view value) noexcept {
    if(value.empty()||value.size()>260)return false;
    return std::all_of(value.begin(),value.end(),[](unsigned char c){
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||
               (c>='0'&&c<='9')||c=='-'||c=='_'||c==':'||c=='.';
    });
}
inline std::string Reference(const StructuredDiagnostic::Record& r){
    return r.SessionId+":"+r.EventId;
}
inline std::string OneLine(std::string_view source,std::size_t maximum){
    std::string out;
    out.reserve(std::min(source.size(),maximum));
    for(std::size_t i=0;i<source.size()&&out.size()<maximum;){
        const auto c=static_cast<unsigned char>(source[i]);
        if(c=='\r'||c=='\n'||c=='\t'||c<32||c==127){out.push_back(' ');++i;continue;}
        if(c<128){out.push_back(static_cast<char>(c));++i;continue;}
        const std::size_t width=(c>=0xC2&&c<=0xDF)?2:
                                (c>=0xE0&&c<=0xEF)?3:
                                (c>=0xF0&&c<=0xF4)?4:0;
        if(!width||i+width>source.size()){out.push_back('?');++i;continue;}
        bool valid=true;
        for(std::size_t n=1;n<width;++n)
            valid&=(static_cast<unsigned char>(source[i+n])&0xC0u)==0x80u;
        if(!valid){out.push_back('?');++i;continue;}
        if(out.size()+width>maximum)break;
        out.append(source.substr(i,width));i+=width;
    }
    return out;
}
inline std::string Format(const StructuredDiagnostic::Record& r){
    if(r.Severity!="Error"&&r.Severity!="Warning")return {};
    if(!StructuredDiagnostic::IsCode(r.Code)||r.Title.empty()||r.Message.empty())return {};
    const auto ref=Reference(r);
    if(!SafeReference(ref))return {};
    const std::string lead=r.Severity=="Error"?"[RuneSchema] [error] [":"[RuneSchema] [warning] [";
    std::string output=lead+r.Code+"] ";
    output+=OneLine(r.Title,160);
    output+=": ";
    output+=OneLine(r.Message,384);
    output+=" [diag-event="+ref+"]\n";
    return output;
}
inline std::string EventReference(std::string_view row){
    const auto start=row.find(Marker);
    if(start==row.npos)return {};
    const auto from=start+Marker.size();
    const auto end=row.find(']',from);
    if(end==row.npos||end-from>260)return {};
    const auto ref=row.substr(from,end-from);
    return SafeReference(ref)?std::string(ref):std::string{};
}
}
