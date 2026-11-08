#ifndef TESTCPMSERVICEBREMEN_H_
#define TESTCPMSERVICEBREMEN_H_

#include "artery/application/ItsG5Service.h"
#include "artery/networking/PositionProvider.h"
#include "artery/envmod/LocalEnvironmentModel.h"
#include <omnetpp/simtime.h>

class TestCpmServiceLuST : public artery::ItsG5Service
{
    public:
        ~TestCpmServiceLuST();

    protected:
        int numInitStages() const override;
        void initialize(int stage) override;
        void handleMessage(omnetpp::cMessage* msg) override;
        void trigger() override;
        void indicate(const vanetza::btp::DataIndication&, omnetpp::cPacket*) override;

    private:
        void generatePacket();

        int mHostId = 0;
        const artery::PositionProvider* mPositionProvider = nullptr;
        const artery::LocalEnvironmentModel* mEnvironmentModel = nullptr;
        omnetpp::cMessage* mTrigger = nullptr;
        omnetpp::SimTime mCpmInterval;

        long mCpmSentLifetime = 0;
        long mCpmReceivedLifetime = 0;
};

#endif