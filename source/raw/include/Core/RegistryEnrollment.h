#pragma once
#include <mutex>
#include <set>
#include <string>
namespace PS::RegistryEnrollment {
inline std::mutex Mutex;
inline std::set<std::string> Failures;
inline void Reset(){std::scoped_lock lock(Mutex);Failures.clear();}
inline void Reject(const std::string& source,const std::string& reason){std::scoped_lock lock(Mutex);Failures.insert(source+": "+reason);}
inline bool Healthy(){std::scoped_lock lock(Mutex);return Failures.empty();}
}
