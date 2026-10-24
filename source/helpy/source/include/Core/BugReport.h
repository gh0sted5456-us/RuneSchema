#pragma once
#include <array>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PS::BugReport {
inline constexpr std::array Categories{"Crash / freeze","Performance","Helpy interface","Mods / registration","Save / loading","Multiplayer / server","Cache / icons","Other"};
inline constexpr char NewIssue[]="https://github.com/gh0sted5456-us/RuneSchema/issues/new";
struct Draft {std::string Title,Details,Steps,Expected,Additional;std::size_t Category=0;};
inline bool HasText(std::string_view value){return value.find_first_not_of(" \r\n\t")!=value.npos;}
inline std::string Encode(std::string_view value){
    constexpr char hex[]="0123456789ABCDEF";std::string result;
    for(unsigned char ch:value){if((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='-'||ch=='_'||ch=='.'||ch=='~')result+=char(ch);
        else{result+='%';result+=hex[ch>>4];result+=hex[ch&15];}}
    return result;
}
inline std::string Body(const Draft& draft,const std::vector<std::pair<std::string,std::string>>& environment={}){
    if(!HasText(draft.Title)||draft.Title.size()>160||std::any_of(draft.Title.begin(),draft.Title.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Enter a short, single-line title (up to 160 bytes).");
    if(draft.Category>=Categories.size()||!HasText(draft.Details)||draft.Details.size()>2048||draft.Steps.size()>2048||draft.Expected.size()>1024||draft.Additional.size()>2048)throw std::runtime_error("Describe the problem within the form's field limits.");
    for(const auto* field:{&draft.Details,&draft.Steps,&draft.Expected,&draft.Additional})if(field->find('\0')!=field->npos)throw std::runtime_error("The report contains an embedded null character.");
    std::string body="### Category\n"+std::string(Categories[draft.Category])+"\n\n### What happened\n"+draft.Details;
    const auto section=[&](const char* title,const std::string& value){body+="\n\n### ";body+=title;body+='\n';body+=HasText(value)?value:"Not provided.";};
    section("Steps to reproduce",draft.Steps);section("Expected behavior",draft.Expected);
    if(HasText(draft.Additional))section("Additional details (user supplied)",draft.Additional);
    if(environment.size()>8)throw std::runtime_error("Unexpected bug-report metadata.");
    if(!environment.empty()){
        body+="\n\n### Runtime / versions\n";
        for(const auto& [name,value]:environment){
            if(name.size()>40||value.size()>256||name.find_first_of("\r\n\t")!=name.npos||value.find_first_of("\r\n\t")!=value.npos||name.find('\0')!=name.npos||value.find('\0')!=value.npos)throw std::runtime_error("Unexpected version metadata; uncheck runtime details or retry.");
            body+="- "+name+": "+(value.empty()?"Not reported":value)+"\n";
        }
    }
    body+="\n\nPrepared in Helpy. Logs, saves, usernames and filesystem paths are not collected automatically.";return body;
}
struct Submission {std::string Url;bool CopyBody=false;};
inline Submission Prepare(const Draft& draft,const std::string& body){
    // Large reports use an explicit clipboard handoff instead of truncating text or exceeding URI limits.
    const std::string title=std::string(NewIssue)+"?title="+Encode(draft.Title);
    const auto full=title+"&body="+Encode(body);
    return full.size()<=6000?Submission{full,false}:Submission{title,true};
}
}
