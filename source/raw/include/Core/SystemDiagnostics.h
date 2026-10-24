#pragma once
#include <windows.h>
#include <intrin.h>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#pragma comment(lib,"user32.lib")
namespace PS::SystemDiagnostics {
inline std::string UTF8(const wchar_t* value){if(!value)return {};const int size=WideCharToMultiByte(CP_UTF8,0,value,-1,nullptr,0,nullptr,nullptr);if(size<1||size>4096)return {};std::string out(size,'\0');WideCharToMultiByte(CP_UTF8,0,value,-1,out.data(),size,nullptr,nullptr);out.pop_back();return out;}
inline std::string GiB(ULONGLONG bytes){std::ostringstream out;out<<std::fixed<<std::setprecision(2)<<double(bytes)/(1024.*1024.*1024.);return out.str()+" GiB";}
inline std::vector<std::string> Startup(const std::filesystem::path& install,const std::filesystem::path& cache){
    std::vector<std::string> rows;
    OSVERSIONINFOEXW version{};version.dwOSVersionInfoSize=sizeof(version);
    using RtlVersion=LONG(WINAPI*)(OSVERSIONINFOEXW*);const auto ntdll=GetModuleHandleW(L"ntdll.dll");const auto read=ntdll?reinterpret_cast<RtlVersion>(GetProcAddress(ntdll,"RtlGetVersion")):nullptr;
    if(read&&read(&version)==0)rows.push_back("Operating system: Windows NT "+std::to_string(version.dwMajorVersion)+"."+std::to_string(version.dwMinorVersion)+" build "+std::to_string(version.dwBuildNumber)+"; process="+std::to_string(sizeof(void*)*8)+"-bit");
    else rows.push_back("Operating system: version unavailable");
    SYSTEM_INFO system{};GetNativeSystemInfo(&system);std::string cpu="unavailable";
#if defined(_M_X64) || defined(_M_IX86)
    int regs[4]{};__cpuid(regs,int(0x80000000));if(static_cast<unsigned>(regs[0])>=0x80000004){char brand[49]{};for(unsigned i=0;i<3;++i){__cpuid(regs,int(0x80000002+i));memcpy(brand+i*16,regs,16);}cpu=brand;while(!cpu.empty()&&cpu.front()==' ')cpu.erase(cpu.begin());while(!cpu.empty()&&cpu.back()==' ')cpu.pop_back();}
#endif
    const auto architecture=system.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64?"x64":system.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_ARM64?"ARM64":system.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_INTEL?"x86":"unknown";
    rows.push_back("CPU: "+cpu+"; logical processors="+std::to_string(system.dwNumberOfProcessors)+"; native architecture="+architecture);
    MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);if(GlobalMemoryStatusEx(&memory)){
        rows.push_back("RAM: total="+GiB(memory.ullTotalPhys)+"; available="+GiB(memory.ullAvailPhys)+"; load="+std::to_string(memory.dwMemoryLoad)+"%");
        rows.push_back("Commit capacity: total="+GiB(memory.ullTotalPageFile)+"; available="+GiB(memory.ullAvailPageFile));
    }else rows.push_back("RAM: reading unavailable");
    DISPLAY_DEVICEW display{};display.cb=sizeof(display);for(DWORD i=0;i<16&&EnumDisplayDevicesW(nullptr,i,&display,0);++i){if(display.StateFlags&DISPLAY_DEVICE_PRIMARY_DEVICE){rows.push_back("Primary display adapter: "+UTF8(display.DeviceString));break;}display={};display.cb=sizeof(display);}
    std::vector<std::wstring> seen;for(const auto& [label,path]:std::vector<std::pair<std::string,std::filesystem::path>>{{"Game",install},{"Helpy cache",cache}}){const auto root=std::filesystem::absolute(path).root_path().wstring();if(root.empty()||root.starts_with(L"\\\\")){rows.push_back(label+" storage: network/unknown volume (location omitted)");continue;}
        rows.push_back(label+" location: volume "+UTF8(root.c_str())+" (personal path omitted)");if(std::find(seen.begin(),seen.end(),root)!=seen.end())continue;seen.push_back(root);ULARGE_INTEGER available{},total{},free{};
        if(GetDiskFreeSpaceExW(root.c_str(),&available,&total,&free))rows.push_back(label+" storage: total="+GiB(total.QuadPart)+"; available="+GiB(available.QuadPart)+"; free="+GiB(free.QuadPart));else rows.push_back(label+" storage: reading unavailable");
    }
    TIME_ZONE_INFORMATION zone{};const auto daylight=GetTimeZoneInformation(&zone);if(daylight!=TIME_ZONE_ID_INVALID){const auto bias=zone.Bias+(daylight==TIME_ZONE_ID_DAYLIGHT?zone.DaylightBias:zone.StandardBias);rows.push_back("Log clock: UTC (ISO 8601); local UTC offset minutes="+std::to_string(-bias));}
    rows.push_back("Privacy: no username, hostname, network address, geographic location, serial numbers or save contents collected in this header.");return rows;
}
}
