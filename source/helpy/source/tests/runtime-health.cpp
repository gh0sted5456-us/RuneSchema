#include "Core/WindowsResourceSample.h"
#include "Core/DiagnosticToastFeed.h"
#include <cassert>
#include <iostream>
#include <thread>

using namespace PS;using namespace RuntimeHealth;
static bool Has(const std::vector<Event>& events,Diagnostics::Id id){return std::any_of(events.begin(),events.end(),[&](const auto& event){return event.Code==id;});}
int main(int argc,char** argv){
    if(argc==2&&std::string_view(argv[1])=="--gpu"){
        WindowsResourceSample::Sampler sampler;(void)sampler.Read(0,true);std::this_thread::sleep_for(std::chrono::seconds(1));const auto sample=sampler.Read(0,true);
        std::cout<<"CPU="<<sample.Helpy.Cpu.value_or(-1)<<" RAM_MiB="<<sample.Helpy.WorkingSet.value_or(0)/1048576<<" HelpyGPU="<<sample.Helpy.Gpu.value_or(-1)<<" SystemGPU="<<sample.Gpu.value_or(-1)<<" Status="<<sample.GpuStatus<<'\n';return 0;
    }
    {PlayTime timer;unsigned notices=0;std::optional<unsigned> reminder;
        for(unsigned second=0;second<=14400;second+=5)if(auto hour=timer.Tick(42,true,true,second)){++notices;reminder=hour;assert((notices==1&&second==10800&&*hour==3)||(notices==2&&second==14400&&*hour==4));}
        assert(notices==2&&reminder==4&&timer.Seconds()==14400);
        const auto before=timer.Seconds();timer.Tick(42,false,true,14405);timer.Tick(42,false,true,18005);timer.Tick(42,true,true,18010);assert(timer.Seconds()==before);
        timer.Tick(42,true,true,20000);assert(timer.Seconds()==before); // Suspension is not gameplay.
        timer.Tick(7,true,true,20005);assert(timer.Seconds()==0);
        timer.Tick(0,false,true,20010);assert(timer.Seconds()==0);
        PlayTime disabled;for(unsigned second=0;second<=10800;second+=5)assert(!disabled.Tick(42,true,false,second));assert(!disabled.Tick(42,true,true,10805));
        assert(Reminder(3)=="You have been playing for three hours, consider taking a 10 minute break.");
        DiagnosticToastFeed::Feed feed;feed.Accept(LiveLog::Parse("[RuneSchema] [info] [RS-PLAY-001] "+Reminder(3)),{});assert(feed.Visible().size()==1&&feed.Visible()[0].Severity==LiveLog::Level::Info);
    }
    {Resources policy;Settings settings;settings.StartupAlerts=true;Sample sample;sample.Helpy.Pid=1;sample.Game.Pid=2;sample.Helpy.Cpu=95;
        assert(policy.Review(sample,settings).empty());sample.At=5;assert(policy.Review(sample,settings).empty());sample.At=10;assert(Has(policy.Review(sample,settings),Diagnostics::Id::ProcessCpuHigh));
        sample.At=15;assert(policy.Review(sample,settings).empty());sample.Helpy.Cpu.reset();sample.At=20;assert(policy.Review(sample,settings).empty());
        sample.Game.Gpu=99;sample.At=25;policy.Review(sample,settings);sample.At=30;policy.Review(sample,settings);sample.At=35;assert(Has(policy.Review(sample,settings),Diagnostics::Id::ProcessGpuHigh));
        settings.Gpu=false;sample.At=40;assert(!Has(policy.Review(sample,settings),Diagnostics::Id::ProcessGpuHigh));settings.Resources=false;assert(policy.Review(sample,settings).empty());
        Resources growth;Settings growthSettings;growthSettings.StartupAlerts=true;sample={};sample.Helpy.Pid=1;sample.Helpy.Created=123;sample.Helpy.PrivateBytes=1024;
        for(unsigned second=0;second<=60;second+=5){sample.At=second;if(second==60)sample.Helpy.PrivateBytes=300ULL*1048576;const auto events=growth.Review(sample,growthSettings);assert(Has(events,Diagnostics::Id::ProcessMemoryGrowth)==(second==60));}
        sample.At=65;sample.Helpy.Created=124;sample.Helpy.PrivateBytes=900ULL*1048576;assert(!Has(growth.Review(sample,growthSettings),Diagnostics::Id::ProcessMemoryGrowth));
    }
    {Resources policy;Settings settings;Sample sample;sample.Helpy.Pid=1;sample.Game.Pid=2;sample.Game.Created=100;sample.Helpy.Cpu=95;sample.Game.Gpu=99;sample.Memory=95;
        assert(!settings.StartupAlerts);
        for(unsigned second=0;second<120;second+=5){sample.At=second;sample.Game.PrivateBytes=(1ULL+second)*1048576;assert(policy.Review(sample,settings).empty());}
        sample.At=120;assert(policy.Review(sample,settings).empty());sample.At=125;assert(policy.Review(sample,settings).empty());sample.At=130;
        const auto events=policy.Review(sample,settings);assert(Has(events,Diagnostics::Id::ProcessCpuHigh)&&Has(events,Diagnostics::Id::ProcessGpuHigh)&&Has(events,Diagnostics::Id::SystemMemoryPressure));
        assert(!Has(events,Diagnostics::Id::ProcessMemoryGrowth));
        sample.At=300;assert(policy.Review(sample,settings).empty());sample.At=305;assert(policy.Review(sample,settings).empty());sample.At=310;
        assert(Has(policy.Review(sample,settings),Diagnostics::Id::SystemMemoryPressure)); // A sampling pause must not restart the startup quiet period.
        sample.Game.Created=101;sample.At=315;assert(policy.Review(sample,settings).empty());sample.At=320;assert(policy.Review(sample,settings).empty());
        settings.StartupAlerts=true;sample.At=325;assert(policy.Review(sample,settings).empty());sample.At=330;assert(policy.Review(sample,settings).empty());sample.At=335;
        assert(Has(policy.Review(sample,settings),Diagnostics::Id::SystemMemoryPressure));
        settings.StartupAlerts=false;sample.At=340;assert(policy.Review(sample,settings).empty());
        settings.Resources=false;sample.At=345;assert(policy.Review(sample,settings).empty());settings.Resources=true;sample.At=350;assert(policy.Review(sample,settings).empty());
    }
    {LuaActivity lua;lua.Add("External",false,"External label");lua.Add("Companion",true);Settings settings;settings.LuaMessages=10;lua.Review(0,settings);
        for(unsigned i=0;i<12;++i)assert(!lua.Observe("[Lua] [External label] sample",1));lua.Slow(4);
        auto events=lua.Review(5,settings);assert(Has(events,Diagnostics::Id::LuaLogActivity)&&Has(events,Diagnostics::Id::LuaActivityOverlap));assert(lua.Status("External")=="High log activity observed");
        for(unsigned i=0;i<12;++i)lua.Observe("[Lua] [External] sample",6);assert(lua.Review(10,settings).empty());
        assert(!lua.Observe("[Lua] [Companion] Lua error: prior startup",11,false));assert(lua.Status("Companion")=="Error reported in log");
        const auto failure=lua.Observe("[RuneSchema][UE4SS][LUA-FAILED] 'Companion': failure",12);assert(failure&&failure->Code==Diagnostics::Id::LuaCompanionError);
        assert(!lua.Observe("[Lua] [Unknown] Lua error",13));assert(!lua.Observe(std::string(16385,'x'),14));
        LuaActivity ambiguous;ambiguous.Add("First",false,"Shared label");ambiguous.Add("Second",false,"Shared label");assert(!ambiguous.Observe("[Shared label] Lua error",0));assert(ambiguous.Status("First")=="Not observed");
        settings.UE4SS=false;assert(lua.Review(20,settings).empty());
    }
    assert(!WindowsResourceSample::Cpu(20,10,1,8));assert(!WindowsResourceSample::Cpu(0,10,0,8));assert(std::abs(*WindowsResourceSample::Cpu(0,40000000,1,8)-50)<.00001);
    assert(WindowsResourceSample::GpuPid(L"pid_42_luid_0x00000000_0x01_eng_0_engtype_3D")==42);assert(WindowsResourceSample::GpuPid(L"pid_420_luid_0")==420);
    assert(!WindowsResourceSample::GpuPid(L"pid_4294967296_luid_0")&&!WindowsResourceSample::GpuPid(L"pid__luid_0")&&!WindowsResourceSample::GpuPid(L"other"));
    DWORD before=0,after=0;assert(GetProcessHandleCount(GetCurrentProcess(),&before));
    for(unsigned i=0;i<20;++i){WindowsResourceSample::Sampler sampler;const auto sample=sampler.Read(0,false);assert(sample.Helpy.Pid==GetCurrentProcessId()&&sample.Helpy.WorkingSet&&sample.Helpy.PrivateBytes&&!sample.Helpy.Cpu&&!sample.Helpy.Gpu);}
    assert(GetProcessHandleCount(GetCurrentProcess(),&after)&&after<=before+2);
}
