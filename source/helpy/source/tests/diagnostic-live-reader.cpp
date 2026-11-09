#include "Generator/LiveLogPanel.h"
#include "Core/DiagnosticCenterModel.h"
#include "Core/DiagnosticSession.h"
#include "Generator/StructuredLogFeed.h"
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
    std::string session;
    LiveLogPanel::Streams[0].Override=file;
    LiveLogPanel::OnBackfillEntry=[&](const LiveLog::Entry& entry){++backfill;store.Accept(entry);};
    LiveLogPanel::OnEntry=[&](const LiveLog::Entry& entry){
        if(session.empty()||DiagnosticSession::Includes(session,entry.Text)){++live;store.Accept(entry);}
    };
    const auto read=[](){auto& state=LiveLogPanel::Streams[0];state.NextRead={};LiveLogPanel::Poll();assert(state.Worker.valid());state.Worker.wait();LiveLogPanel::Poll();assert(state.ReadError.empty());};
    read();assert(backfill>0&&!store.Events().empty()&&LiveLogPanel::RuneSchemaLiveReady);
    if(fixture){
        assert(backfill==2&&store.UnreadIssues()==0);
        {std::ofstream out(file,std::ios::app);out<<"[RuneSchema] [info] Still active\n";}
        read();assert(live==1&&store.Events().size()==3&&store.UnreadIssues()==0);
        LiveLogPanel::Streams[0].Paused=true;
        {std::ofstream out(file,std::ios::app);out<<"[RuneSchema] [warning] Example issue\n";}
        read();assert(live==2&&store.Events().size()==4&&store.UnreadIssues()==1);

        // Leave a previous-run batch in flight while the new launch reuses the file.
        auto& state=LiveLogPanel::Streams[0];
        {std::ofstream out(file,std::ios::app);out<<"[2026-11-08T11:22:33.122Z thread=10] [RuneSchema] [error] Old launch\n";}
        state.NextRead={};LiveLogPanel::Poll();state.Worker.wait();
        session="2026-11-08T11:22:33.123";
        assert(DiagnosticSession::ProcessStart(GetCurrentProcessId()).size()==23);
        assert(!DiagnosticSession::Includes(session,"2026-11-08T11:22:33.122Z"));
        assert(DiagnosticSession::Includes(session,"2026-11-08T11:22:33.123Z"));
        store.Clear();LiveLogPanel::RestartRuneSchemaRead();
        LiveLogPanel::Poll();assert(store.Events().empty()&&state.PausedLines.empty());
        {std::ofstream out(file);
         out<<"[2026-11-08T11:22:33.123Z thread=20] [RuneSchema] [warning] Current launch\n"
              "[2026-11-08T11:22:33.121Z] [RuneSchema] [error] Delayed previous-run diagnostic\n";}
        read();assert(live==3&&store.Events().size()==1&&store.UnreadIssues()==1);
        assert(store.Events().back().Text.find("Current launch")!=std::string::npos);

        std::size_t structured=0;
        StructuredLogFeed::OnRecord=[&](const StructuredDiagnostic::Record& record){
            if(DiagnosticSession::Includes(session,record.Timestamp))++structured;
        };
        const auto records=[](){
            StructuredDiagnosticReader::Batch batch;
            batch.Records.resize(2);
            batch.Records[0].Timestamp="2026-11-08T11:22:33.122Z";
            batch.Records[1].Timestamp="2026-11-08T11:22:33.124Z";
            return batch;
        };
        StructuredLogFeed::Worker=std::async(std::launch::async,records);
        StructuredLogFeed::Restart();StructuredLogFeed::DrainPending();
        assert(structured==0);
        StructuredLogFeed::ConsumeBatch(records());assert(structured==1);
        StructuredLogFeed::Shutdown();
    }
    std::cout<<"Read "<<backfill<<" initial and "<<live<<" new lines; "<<store.Events().size()<<" diagnostic history events.\n";
    LiveLogPanel::Shutdown();if(fixture)std::filesystem::remove(file);
}
