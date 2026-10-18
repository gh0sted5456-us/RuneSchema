#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace PS {
template<class T>struct MetadataPage {std::vector<T> Records;std::size_t Total=0;std::uint64_t Revision=0;};
template<class T>MetadataPage<T> CopyMetadataWindow(const std::vector<T>& records,std::uint64_t revision,std::size_t offset,std::size_t limit){
    MetadataPage<T> result;result.Total=records.size();result.Revision=revision;
    const auto begin=std::min(offset,records.size());const auto count=std::min({limit,std::size_t(128),records.size()-begin});
    result.Records.assign(records.begin()+begin,records.begin()+begin+count);return result;
}
}
