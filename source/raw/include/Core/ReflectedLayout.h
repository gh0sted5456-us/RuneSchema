#pragma once
#include <span>
#include <stdexcept>

namespace PS::ReflectedLayout {
// Property coordinates come from live reflection. Padding/reordering is not
// an ABI change when callers marshal/read each field through its property.
template<class Fields> void Validate(int frameSize,const Fields& fields) {
    if(frameSize<0||frameSize>65535||fields.size()>256)
        throw std::runtime_error("Reflected parameter frame exceeds supported bounds");
    for(size_t i=0;i<fields.size();++i) {
        const auto& field=fields[i];
        if(field.Offset<0||field.Size<=0||field.Offset>frameSize||field.Size>frameSize-field.Offset)
            throw std::runtime_error("Reflected property is outside its owner frame");
        for(size_t j=0;j<i;++j) {
            const auto& other=fields[j];
            if(field.Name==other.Name)throw std::runtime_error("Reflected property name is duplicated");
            if(field.Offset<other.Offset+other.Size&&other.Offset<field.Offset+field.Size)
                throw std::runtime_error("Reflected parameter fields overlap");
        }
    }
}
}
