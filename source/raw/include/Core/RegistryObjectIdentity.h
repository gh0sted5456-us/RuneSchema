#pragma once
namespace PS::RegistryObjectIdentity {
inline bool Matches(bool sameObject,bool samePath,bool valid,bool destroying,
    bool requireRoot,bool rooted,int savedSerial,int currentSerial) {
    return sameObject&&samePath&&valid&&!destroying&&(!requireRoot||rooted)
        &&savedSerial>0&&savedSerial==currentSerial;
}
}
