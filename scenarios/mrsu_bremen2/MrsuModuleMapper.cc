#include "MrsuModuleMapper.h"
#include <omnetpp/ccomponenttype.h>

using namespace omnetpp;

namespace traci
{

Define_Module(MrsuModuleMapper)

void MrsuModuleMapper::initialize()
{
    BasicModuleMapper::initialize();
    m_mrsu_type = cModuleType::get("MrsuCar");
    m_regular_type = cModuleType::get("RegularCar");
}

cModuleType* MrsuModuleMapper::vehicle(NodeManager& manager, const std::string& id)
{
    bool isMrsuCandidate = (id.find("tram") != std::string::npos) ||
                           (id.find("mrsu") != std::string::npos);

    if (isMrsuCandidate) {
        return m_mrsu_type;
    }

    return m_regular_type;
}

} // namespace traci