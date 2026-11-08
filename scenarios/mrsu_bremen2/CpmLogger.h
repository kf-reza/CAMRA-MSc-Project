#ifndef CPMLOGGER_H_
#define CPMLOGGER_H_

#include "artery/application/ItsG5Service.h"
#include <fstream>
#include <map>
#include <string>

namespace traci { class VehicleController; }

class CpmLogger : public artery::ItsG5Service
{
    public:
        void indicate(const vanetza::btp::DataIndication&, omnetpp::cPacket*) override;
        void trigger() override {}
        bool requiresListener() const override { return false; }
        void receiveSignal(omnetpp::cComponent* source, omnetpp::simsignal_t signalID,
                            omnetpp::cObject* obj, omnetpp::cObject* details) override;

    protected:
        void initialize() override;
        void finish() override;

    private:
        const traci::VehicleController* mVehicleController = nullptr;
        long mCpmSentCount = 0;
        long mTotalObjectsReported = 0;
        std::map<std::string, long> mPerceivedObjectCounts; // objectId -> times seen across all CPMs sent

        static std::ofstream sReportFile;
        static bool sReportFileOpen;
};

#endif /* CPMLOGGER_H_ */