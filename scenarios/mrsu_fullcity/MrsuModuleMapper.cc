#include "MrsuModuleMapper.h"
#include <omnetpp/ccomponenttype.h>

using namespace omnetpp;

namespace traci
{

Define_Module(MrsuModuleMapperFC)

void MrsuModuleMapperFC::initialize()
{
    BasicModuleMapper::initialize();
    m_mrsu_type = cModuleType::get("MrsuCarFC");
    m_regular_type = cModuleType::get("RegularCarFC");
}

cModuleType* MrsuModuleMapperFC::vehicle(NodeManager& manager, const std::string& id)
{
    bool isMrsuCandidate = (id.find("tram") != std::string::npos) ||
                           (id.find("mrsu") != std::string::npos);

    if (isMrsuCandidate) {
        return m_mrsu_type;
    }

    return m_regular_type;
}

} // namespace traci