#include "Runtime/F2ReferenceIndex.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include "Generator/F2ReferenceTree.h"

#ifdef _WIN32
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib,"winhttp.lib")
#endif
namespace PS::F2ReferenceIndex {
namespace {
using json=nlohmann::json;

struct Fetcher {
    std::shared_ptr<std::atomic<bool>> cancel;
    unsigned requests=0;
    std::chrono::steady_clock::time_point deadline=std::chrono::steady_clock::now()+std::chrono::seconds(90);
    void Check()const {
        if(cancel->load()||std::chrono::steady_clock::now()>deadline)
            throw std::runtime_error("Reference indexing cancelled or timed out; existing cache retained");
    }
    json Get(const std::string& route) {
        Check();if(++requests>80)throw std::runtime_error("Reference request budget reached; existing cache retained");
#ifdef _WIN32
        struct Handle {
            HINTERNET value=nullptr;
            ~Handle(){if(value)WinHttpCloseHandle(value);}
        };
        Handle session{WinHttpOpen(L"RuneSchema-F2-ReferenceIndex/1",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
        if(!session.value)throw std::runtime_error("WinHTTP session unavailable; using local index");
        if(!WinHttpSetTimeouts(session.value,5000,5000,5000,5000))
            throw std::runtime_error("Cannot enforce reference request timeout");
        Handle connection{WinHttpConnect(session.value,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0)};
        if(!connection.value)throw std::runtime_error("Cannot connect to public reference index");
        const std::wstring path(route.begin(),route.end());
        Handle request{WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
        if(!request.value)throw std::runtime_error("Cannot open reference request");
        // Fixed public host/repository, no credentials, no automatic cross-host redirects.
        DWORD disable=WINHTTP_DISABLE_REDIRECTS;
        if(!WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disable,sizeof(disable)))
            throw std::runtime_error("Cannot enforce reference redirect policy");
        DWORD autologon=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
        if(!WinHttpSetOption(request.value,WINHTTP_OPTION_AUTOLOGON_POLICY,&autologon,sizeof(autologon)))
            throw std::runtime_error("Cannot disable automatic reference authentication");
        constexpr auto headers=L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
        if(!WinHttpSendRequest(request.value,headers,static_cast<DWORD>(-1L),WINHTTP_NO_REQUEST_DATA,0,0,0)
            ||!WinHttpReceiveResponse(request.value,nullptr))throw std::runtime_error("Reference download unavailable; using local index");
        DWORD status=0,size=sizeof(status);
        if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)
            throw std::runtime_error("Reference HTTP "+std::to_string(status)+"; using local index (no automatic retry)");
        std::string text;char block[16384];DWORD read=0;
        for(;;) {
            Check();
            if(!WinHttpReadData(request.value,block,sizeof(block),&read))throw std::runtime_error("Reference download interrupted");
            if(!read)break;
            if(text.size()+read>24*1024*1024)throw std::runtime_error("Reference response exceeds 24 MiB; cache retained");
            text.append(block,read);
        }
        return json::parse(text,[](int depth,json::parse_event_t,json&){
            if(depth>32)throw std::runtime_error("Reference JSON nesting exceeds limit");
            return true;
        });
#else
        (void)route;throw std::runtime_error("Online reference indexing is Windows-only; local index still works");
#endif
    }
    json Tree(const std::string& ref,bool recursive=false) {
        if(!F2Catalog::SimpleToken(ref))throw std::runtime_error("Invalid reference tree identifier");
        auto result=Get("/repos/RSDWArchive/RSDWArchive/git/trees/"+ref+(recursive?"?recursive=1":""));
        if(!result.is_object()||!result.contains("tree")||!result["tree"].is_array()
            ||result["tree"].size()>100000)throw std::runtime_error("Unexpected public reference tree format");
        return result;
    }

};
Result Fetch(F2Catalog::Sources sources,const std::shared_ptr<std::atomic<bool>>& cancelled) {
    Result out;Fetcher http{cancelled};
    try {
        const auto fetch=[&](const std::string& ref,bool recursive) {
            const auto data=http.Tree(ref,recursive);
            F2ReferenceTree::Tree tree;
            tree.sha=data.at("sha").get<std::string>();tree.truncated=data.value("truncated",false);
            for(const auto& node:data.at("tree")) {
                http.Check();
                if(!node.is_object())throw std::runtime_error("Malformed reference tree node");
                const auto type=node.value("type",std::string{});
                if(type!="tree"&&type!="blob")continue;
                const auto path=node.at("path").get<std::string>();
                if(path.empty()||path.size()>2048||path.find("..")!=path.npos||path.front()=='/'||path.find('\\')!=path.npos)
                    throw std::runtime_error("Invalid path in reference tree");
                tree.nodes.push_back({path,node.at("sha").get<std::string>(),type=="tree"});
            }
            return tree;
        };
        out.entries=F2ReferenceTree::Collect(fetch,sources,out.revision);http.Check();
        if(out.entries.empty())throw std::runtime_error("RSDW returned no usable candidate paths; existing cache retained");
    }catch(const std::exception& e){out.error=e.what();out.entries.clear();}
    catch(...){out.error="Reference indexing failed; existing cache retained";out.entries.clear();}
    out.requests=http.requests;return out;
}
}
bool Job::Begin(F2Catalog::Sources sources) {
    if(future.valid())return false;
    cancel=std::make_shared<std::atomic<bool>>(false);
    future=std::async(std::launch::async,[sources=std::move(sources),flag=cancel]{return Fetch(sources,flag);});
    return true;
}
std::optional<Result> Job::Poll() {
    if(!future.valid()||future.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return {};
    return future.get();
}
void Job::Cancel() noexcept {if(cancel)cancel->store(true);}
void Job::Shutdown() noexcept {Cancel();try{if(future.valid())future.get();}catch(...) {}}
} // namespace PS::F2ReferenceIndex
