#include "MrsuModuleMapper.h"
#include <omnetpp/ccomponenttype.h>

using namespace omnetpp;

namespace traci
{

Define_Module(MrsuModuleMapperLuST)

void MrsuModuleMapperLuST::initialize()
{
    BasicModuleMapper::initialize();
    m_mrsu_type = cModuleType::get("MrsuCarLuST");
    m_regular_type = cModuleType::get("RegularCarLuST");
}

cModuleType* MrsuModuleMapperLuST::vehicle(NodeManager& manager, const std::string& id)
{
    bool isMrsuCandidate = (id.find("tram") != std::string::npos) ||
                           (id.find("mrsu") != std::string::npos);

    if (isMrsuCandidate) {
        return m_mrsu_type;
    }

    return m_regular_type;
}

} // namespace traci