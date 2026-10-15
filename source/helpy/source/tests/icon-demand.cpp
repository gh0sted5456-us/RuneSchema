#include "Generator/ItemIconCache.h"
#include <cassert>
#include <deque>

int main() {
    using namespace PS;
    namespace icons=ItemIconCache;
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;
    io.DisplaySize={640,480};io.DeltaTime=1.0f/60.0f;
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    SpawnToolRequests::Generation=41;
    auto& state=icons::detail::State();state.Generation=41;
    unsigned submissions=0;std::deque<nlohmann::json> replies;
    icons::ConfigureRemote([&](const std::string&){++submissions;return true;},[&](nlohmann::json& result){if(replies.empty())return false;result=std::move(replies.front());replies.pop_front();return true;});
    const auto begin=[] {ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({640,480});ImGui::Begin("test");};
    const auto end=[] {ImGui::End();ImGui::EndFrame();};
    begin();
    assert(!icons::Draw("clipped",{900,900},{950,950},true));
    assert(state.Queued.empty());
    icons::Queue("waiting",true);icons::Update();assert(submissions==0); // source identity unavailable
    end();
    HelpySourceCache::Current.Key="icon-demand-fixture";
    begin();icons::Update();assert(submissions==0&&state.Queued.empty()); // no longer visible
    for(int i=0;i<80;++i){const auto icon="visible-"+std::to_string(i);state.DiskChecked.insert(icon);icons::Queue(icon,true);}
    assert(state.Queued.size()==32);icons::Update();assert(submissions==1&&state.Pending.size()==1);
    state.NextSubmit={};icons::Update();assert(submissions==1); // single in-flight capture
    const auto active=*state.Pending.begin();end();
    begin();
    replies.push_back({{"Icon",active},{"Status","Error"},{"Retryable",true},{"Error","blank item icon"}});
    icons::Update();state.NextSubmit={};icons::Update();
    assert(submissions==1&&state.Pending.empty()&&state.Queued.empty()); // retries off-screen are discarded
    // A visible icon may retry, but never loop indefinitely while idle.
    state.DiskChecked.insert(active);icons::Queue(active,true);state.NextSubmit={};icons::Update();assert(submissions==2);
    replies.push_back({{"Icon",active},{"Status","Error"},{"Retryable",true},{"Error","blank item icon"}});
    icons::Update();state.NextSubmit={};icons::Update();assert(submissions==3);
    replies.push_back({{"Icon",active},{"Status","Error"},{"Retryable",true},{"Error","blank item icon"}});
    icons::Update();assert(state.Failed.contains(active)&&state.RetryAt.empty());
    state.NextSubmit={};icons::Queue(active,true);icons::Update();assert(submissions==3);
    icons::RetryFailed();state.DiskChecked.insert(active);icons::Queue(active,true);icons::Update();assert(submissions==4);
    end();icons::ClearRemote();ImGui::DestroyContext();
}
