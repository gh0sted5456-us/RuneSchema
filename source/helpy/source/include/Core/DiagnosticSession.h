#pragma once
#include "Core/StructuredDiagnosticContract.h"
#include <Windows.h>
#include <cstdio>
#include <string>
#include <string_view>

namespace PS::DiagnosticSession {
inline std::string ProcessStart(DWORD pid){
    const auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process)return {};
    FILETIME start{},exit{},kernel{},user{};
    const bool read=GetProcessTimes(process,&start,&exit,&kernel,&user)!=0;
    CloseHandle(process);
    SYSTEMTIME utc{};
    if(!read||!FileTimeToSystemTime(&start,&utc))return {};
    char time[32]{};
    std::snprintf(time,sizeof(time),"%04u-%02u-%02uT%02u:%02u:%02u.%03u",
        unsigned(utc.wYear),unsigned(utc.wMonth),unsigned(utc.wDay),unsigned(utc.wHour),
        unsigned(utc.wMinute),unsigned(utc.wSecond),unsigned(utc.wMilliseconds));
    return time;
}

inline bool Includes(std::string_view start,std::string_view record){
    if(start.empty())return false;
    if(record.starts_with('['))record.remove_prefix(1);
    const auto end=record.find_first_of(" ]");
    const auto stamp=record.substr(0,end);
    return StructuredDiagnostic::IsUtc(stamp)&&stamp.substr(0,stamp.size()-1)>=start;
}
}
