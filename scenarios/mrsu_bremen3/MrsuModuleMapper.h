#ifndef MRSUMODULEMAPPER_H_
#define MRSUMODULEMAPPER_H_

#include "traci/BasicModuleMapper.h"

namespace traci
{

class MrsuModuleMapper3 : public BasicModuleMapper
{
    public:
        void initialize() override;
        omnetpp::cModuleType* vehicle(NodeManager& manager, const std::string& id) override;

    private:
        omnetpp::cModuleType* m_mrsu_type = nullptr;
		omnetpp::cModuleType* m_regular_type = nullptr;
};

} // namespace traci

#endif /* MRSUMODULEMAPPER_H_ */