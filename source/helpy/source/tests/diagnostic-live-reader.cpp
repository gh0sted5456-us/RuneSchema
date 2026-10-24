#include "Generator/LiveLogPanel.h"
#include "Core/DiagnosticCenterModel.h"
#include <cassert>
#include <iostream>
namespace PS::HostServices {std::filesystem::path ModDirectory(){return std::filesystem::current_path();}}
int main(int argc,char** argv){
    using namespace PS;
    const bool fixture=argc==1;
    const auto file=fixture?std::filesystem::temp_directory_path()/"diagnostic-live-reader.log":std::filesystem::path(argv[1]);
    if(fixture){std::ofstream out(file);out<<"[RuneSchema] [info] Ready\n[RuneSchema] [diagnostic] Registry checked\n";}
    DiagnosticCenterModel::Store store;
    size_t backfill=0,live=0;
    LiveLogPanel::Streams[0].Override=file;
    LiveLogPanel::OnBackfillEntry=[&](const LiveLog::Entry& entry){++backfill;store.Accept(entry);};
    LiveLogPanel::OnEntry=[&](const LiveLog::Entry& entry){++live;store.Accept(entry);};
    const auto read=[](){auto& state=LiveLogPanel::Streams[0];state.NextRead={};LiveLogPanel::Poll();assert(state.Worker.valid());state.Worker.wait();LiveLogPanel::Poll();assert(state.ReadError.empty());};
    read();assert(backfill>0&&!store.Events().empty()&&LiveLogPanel::RuneSchemaLiveReady);
    if(fixture){
        assert(backfill==2&&store.UnreadIssues()==0);
        {std::ofstream out(file,std::ios::app);out<<"[RuneSchema] [info] Still active\n";}
        read();assert(live==1&&store.Events().size()==3&&store.UnreadIssues()==0);
        LiveLogPanel::Streams[0].Paused=true;
        {std::ofstream out(file,std::ios::app);out<<"[RuneSchema] [warning] Example issue\n";}
        read();assert(live==2&&store.Events().size()==4&&store.UnreadIssues()==1);
    }
    std::cout<<"Read "<<backfill<<" initial and "<<live<<" new lines; "<<store.Events().size()<<" diagnostic history events.\n";
    LiveLogPanel::Shutdown();if(fixture)std::filesystem::remove(file);
}
