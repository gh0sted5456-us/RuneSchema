#include "Generator/ItemIconCache.h"
#include <cassert>
#include <deque>
#include <fstream>

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
    end();
    // A source-scoped disk hit must install pixels without asking the engine
    // to render again. A cached file cannot stand in for source registration.
    icons::InvalidateSource();HelpySourceCache::Current={};HelpySourceCache::Current.Authoritative=true;
    HelpySourceCache::Current.Key="0123456789abcdef";HelpySourceCache::Current.SourceKeys={{"game","abcdef0123456789"}};
    const std::string cachedIcon="/Game/Art/Test/T_Cached.T_Cached";
    const auto file=icons::detail::DiskFile(cachedIcon);std::filesystem::create_directories(file.parent_path());
    const uint32_t iconWidth=64,iconHeight=64,iconBytes=iconWidth*iconHeight*4;
    std::vector<unsigned char> iconPixels(iconBytes,255);
    {std::ofstream output(file,std::ios::binary);output.write("RSI3",4);output.write(reinterpret_cast<const char*>(&iconWidth),4);
        output.write(reinterpret_cast<const char*>(&iconHeight),4);output.write(reinterpret_cast<const char*>(&iconBytes),4);
        output.write(reinterpret_cast<const char*>(iconPixels.data()),iconPixels.size());}
    const auto requestsBeforeDiskHit=submissions;
    begin();icons::Queue(cachedIcon,true);icons::Update();
    assert(submissions==requestsBeforeDiskHit&&state.Textures.contains(cachedIcon));
    assert(icons::detail::DiskFile("/NotRegistered/Art/T_Stale.T_Stale").empty());
    assert(!icons::detail::Install("/Game/Art/Test/T_Wrong.T_Wrong",file));
    state.NextSubmit={};icons::Queue("/NotRegistered/Art/T_Stale.T_Stale",true);icons::Update();
    assert(submissions==requestsBeforeDiskHit&&state.Pending.empty()&&state.Failed.empty());
    end();
    unsigned accountingResets=0;icons::RemoteResetAccounting=[&]{++accountingResets;};
    assert(!icons::RemoveDiskFolder(HostServices::CacheDirectory()));
    assert(!icons::RemoveDiskFolder(HostServices::CacheDirectory()/"unrelated"));
    assert(std::filesystem::exists(file));
    assert(icons::CleanDiskCache()&&accountingResets==1&&!std::filesystem::exists(file));
    icons::RemoteResetAccounting={};
    for(auto* texture:state.RetiredTextures)if(texture)texture->SetStatus(ImTextureStatus_Destroyed);
    icons::detail::ReapRetired(state);icons::ClearRemote();ImGui::DestroyContext();
}
