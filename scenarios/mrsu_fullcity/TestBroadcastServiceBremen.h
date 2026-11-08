#ifndef TESTBROADCASTSERVICE_H_
#define TESTBROADCASTSERVICE_H_

#include "artery/application/ItsG5Service.h"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <fstream>

// forward declaration
namespace traci { class VehicleController; }

class TestBroadcastServiceFC : public artery::ItsG5Service
{
    public:
        void indicate(const vanetza::btp::DataIndication&, omnetpp::cPacket*) override;
        void trigger() override;
        void handleMessage(omnetpp::cMessage* msg) override;
		void receiveSignal(omnetpp::cComponent* source, omnetpp::simsignal_t signalID, double value, omnetpp::cObject* details) override;
        void receiveSignal(omnetpp::cComponent* source, omnetpp::simsignal_t signalID, omnetpp::cObject* obj, omnetpp::cObject* details) override;

    protected:
        void initialize() override;
        void finish() override;

    private:
        void reportWindow();

        const traci::VehicleController* mVehicleController = nullptr;
		double mTransmitPower = 0;
		
        bool isMRSU = false;

        long packetsSent = 0;
        long packetsReceived = 0;

        omnetpp::simtime_t totalDelay = 0;
        double averageDelay = 0;

        // MRSU-only: per-sender, per-window aggregation
        struct SenderRecord {
            long windowStartSentCount = -1;
            long lastKnownSentCount = 0;
            long receivedCountThisWindow = 0;
            double lastKnownPosX = 0;
            double lastKnownPosY = 0;
            double lastKnownSpeed = 0;
            double lastHeardTime = 0;
            long lastKnownAwareness = 0;
        };
      
        std::map<std::string, SenderRecord> senderStats;

        omnetpp::cMessage* mWindowTimer = nullptr;
        static constexpr double WINDOW_INTERVAL = 10.0; // seconds

        omnetpp::cMessage* mRelayTimer = nullptr;
        static constexpr double RELAY_INTERVAL = 1.0; // seconds
        static constexpr int MAX_RELAY_VEHICLES = 30; // safety cap to stay within GeoNetworking max packet size
        void broadcastRelay();
		static constexpr double MRSU_TRANSMIT_POWER = 2.0; // watts (2000 mW), boosted vs regular vehicles
        int windowNumber = 0;

        // accumulated across all windows, for final scalar recording
        long cumulativeExpected = 0;
        long cumulativeReceived = 0;
        double sumOfWindowPDRs = 0;

        // per-window accumulators for delay and velocity (reset every window)
        double windowDelaySum = 0;
        double windowSpeedSum = 0;
        long windowMessageCount = 0;
		double windowCbrSum = 0;
        long windowCbrSampleCount = 0;

        double windowAwarenessSum = 0;
        long windowAwarenessSampleCount = 0;

		// lifetime counters for the communication summary report
        double mStartTime = 0;
        long mCpmSentLifetime = 0;
        long mCpmReceivedLifetime = 0;
        long mRelaySentLifetime = 0;
        long mRelayReceivedLifetime = 0;
        std::set<std::string> mUniqueObjectsDetectedLifetime;   // via own sensor CPMs
        std::set<std::string> mUniqueObjectsIdentifiedLifetime; // via received CPMs

        std::set<std::string> mDirectlyKnownVehicles;  // ever received a CAM from directly
        std::set<std::string> mRelayLearnedVehicles;   // ever mentioned in a received relay
		
        // CPM reception tracking (MRSU only)
        long windowCpmReceivedCount = 0;
        std::set<std::string> windowPerceivedObjects;
		int lastSeenScmNumber = 0;
        omnetpp::cModule* mChannelLoadRxModule = nullptr;

        static std::ofstream sCommReportFile;
        static bool sCommReportFileOpen;
        static std::ofstream sPositionReportFile;
        static bool sPositionReportFileOpen;
};

#endif /* TESTBROADCASTSERVICE_H_ */