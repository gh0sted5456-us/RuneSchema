#pragma once
namespace PS::HelpyReferencePolicy {
// Normal refresh never downloads over a saved cache, regardless of its age,
// a changed filter, Full scan, or altered root settings. Updating is explicit.
inline bool ShouldFetch(bool online,bool cachePresent,bool cacheWritable,bool attempted,bool explicitUpdate) {
    return online&&cacheWritable&&(explicitUpdate||(!cachePresent&&!attempted));
}
}
