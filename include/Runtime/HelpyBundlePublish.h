#pragma once
#include <exception>
#include <string>
#include <vector>
namespace PS::HelpyBundlePublish {
// Document exposes Commit(), Committed(), PreservePending(). Callers order raw
// stats before the asset, then optional companions. Already-published documents
// are never deleted; uncommitted staging files survive any failure for repair.
template<class Document,class OnCommit>
std::string Publish(const std::vector<Document*>& ordered,OnCommit&& onCommit) {
    std::string error;
    try {for(auto* document:ordered){document->Commit();onCommit(*document);}}
    catch(const std::exception& e){error=e.what();}
    catch(...){error="Unexpected bundle publication failure";}
    if(!error.empty())for(auto* document:ordered)if(!document->Committed())document->PreservePending();
    return error;
}
}
