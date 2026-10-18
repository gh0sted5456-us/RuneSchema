#pragma once

namespace DragonWilds::JournalPlayerSelection {
// Finish the scan before rejecting world ambiguity: the intended player's
// component may appear after multiple unrelated players' components.
template<class Object> class Selection {
    Object* m_uniqueWorld=nullptr;
    Object* m_owned=nullptr;
    bool m_worldAmbiguous=false;
    bool m_ownerAmbiguous=false;
public:
    void Observe(Object* object,bool owned) {
        if(!object)return;
        if(m_uniqueWorld && m_uniqueWorld!=object)m_worldAmbiguous=true;
        else if(!m_uniqueWorld)m_uniqueWorld=object;
        if(!owned)return;
        if(m_owned && m_owned!=object)m_ownerAmbiguous=true;
        else if(!m_owned)m_owned=object;
    }
    Object* Result(bool allowUniqueWorld) const {
        if(m_ownerAmbiguous)return nullptr;
        if(m_owned)return m_owned;
        return allowUniqueWorld && !m_worldAmbiguous?m_uniqueWorld:nullptr;
    }
};
}
