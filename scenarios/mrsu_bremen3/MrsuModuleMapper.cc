#include "MrsuModuleMapper.h"
#include <omnetpp/ccomponenttype.h>

using namespace omnetpp;

namespace traci
{

Define_Module(MrsuModuleMapper3)

void MrsuModuleMapper3::initialize()
{
    BasicModuleMapper::initialize();
    m_mrsu_type = cModuleType::get("MrsuCar3");
    m_regular_type = cModuleType::get("RegularCar3");
}

cModuleType* MrsuModuleMapper3::vehicle(NodeManager& manager, const std::string& id)
{
    bool isMrsuCandidate = (id.find("tram") != std::string::npos) ||
                           (id.find("mrsu") != std::string::npos);

    if (isMrsuCandidate) {
        return m_mrsu_type;
    }

    return m_regular_type;
}

} // namespace traci