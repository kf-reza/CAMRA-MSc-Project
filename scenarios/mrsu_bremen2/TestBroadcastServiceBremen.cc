#include "TestBroadcastServiceBremen.h"
#include "test_msgs/TestBroadcast_m.h"
#include "test_msgs/MrsuRelay_m.h"
#include "artery/traci/VehicleController.h"
#include "SegmentCoordinationServer.h"
#include <vanetza/btp/data_request.hpp>
#include <vanetza/dcc/profile.hpp>
#include <vanetza/geonet/interface.hpp>
#include <libsumo/TraCIDefs.h>
#include <iostream>
#include <algorithm>
#include <sstream>
#include "SegmentCoordinationServer.h"
#include "artery/envmod/service/CollectivePerceptionMockMessage.h"
#include "artery/envmod/EnvironmentModelObject.h"

using namespace omnetpp;
using namespace vanetza;

Define_Module(TestBroadcastServiceBremen)

std::ofstream TestBroadcastServiceBremen::sCommReportFile;
bool TestBroadcastServiceBremen::sCommReportFileOpen = false;

void TestBroadcastServiceBremen::initialize()
{
    ItsG5BaseService::initialize();
    mVehicleController = &getFacilities().get_const<traci::VehicleController>();
    mStartTime = simTime().dbl();

    const std::string id = mVehicleController->getVehicleId();

    cModule* hostModule = getParentModule()->getParentModule();
    cModule* radioModule = hostModule->getSubmodule("wlan", 0)->getSubmodule("radio");
    if (radioModule) {
        cModule* transmitterModule = radioModule->getSubmodule("transmitter");
        if (transmitterModule) {
            mTransmitPower = transmitterModule->par("power").doubleValue();
        }

        cModule* receiverModule = radioModule->getSubmodule("receiver");
        if (receiverModule) {
            if (receiverModule->hasPar("sensitivity")) {
                double sensitivity = receiverModule->par("sensitivity").doubleValue();
                std::cout << "[RADIO DIAGNOSTIC] " << id << " receiver sensitivity = "
                           << sensitivity << " dBm" << std::endl;
            }
            else {
                std::cout << "[RADIO DIAGNOSTIC] " << id << " receiver has no 'sensitivity' parameter" << std::endl;
            }
        }
        else {
            std::cout << "[RADIO DIAGNOSTIC] " << id << " could not find 'receiver' submodule" << std::endl;
        }

        cModule* antennaModule = radioModule->getSubmodule("antenna");
        if (antennaModule) {
            if (antennaModule->hasPar("gain")) {
                double gain = antennaModule->par("gain").doubleValue();
                std::cout << "[RADIO DIAGNOSTIC] " << id << " antenna gain = "
                           << gain << std::endl;
            }
            else {
                std::cout << "[RADIO DIAGNOSTIC] " << id << " antenna has no 'gain' parameter" << std::endl;
            }
        }
        else {
            std::cout << "[RADIO DIAGNOSTIC] " << id << " could not find 'antenna' submodule" << std::endl;
        }
    }

    cModule* macModule = hostModule->getSubmodule("wlan", 0)->getSubmodule("mac");
    if (macModule) {
        mChannelLoadRxModule = macModule->getSubmodule("rx");
        if (mChannelLoadRxModule) {
            simsignal_t chLoadSignal = cComponent::registerSignal("ChannelLoad");
            mChannelLoadRxModule->subscribe(chLoadSignal, this);
            std::cout << "[CBR DIAGNOSTIC] " << id << " subscribed to ChannelLoad signal successfully" << std::endl;
        }
        else {
            std::cout << "[CBR DIAGNOSTIC] " << id << " could not find 'rx' submodule under mac" << std::endl;
        }
    }
    else {
        std::cout << "[CBR DIAGNOSTIC] " << id << " could not find 'mac' submodule" << std::endl;
    }

    
        subscribe(cComponent::registerSignal("CpmReceived"));
		subscribe(cComponent::registerSignal("CpmSent"));
        std::cout << "[CPM DIAGNOSTIC] " << id << " subscribed to CpmReceived signal" << std::endl;
    


    if (id.find("tram") != std::string::npos || id.find("mrsu") != std::string::npos) {
        isMRSU = true;
        std::cout << "[MRSU ROLE] " << id << " acting as MRSU candidate (collector)" << std::endl;

        mWindowTimer = new cMessage("windowTimer");
        scheduleAt(simTime() + WINDOW_INTERVAL, mWindowTimer);

        mRelayTimer = new cMessage("relayTimer");
        scheduleAt(simTime() + RELAY_INTERVAL, mRelayTimer);
    }
}

void TestBroadcastServiceBremen::receiveSignal(cComponent* source, simsignal_t signalID, double value, cObject* details)
{
    windowCbrSum += value;
    windowCbrSampleCount++;
}

void TestBroadcastServiceBremen::receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details)
{
    static simsignal_t cpmSentSignal = cComponent::registerSignal("CpmSent");
    static simsignal_t cpmReceivedSignal = cComponent::registerSignal("CpmReceived");

    auto cpm = dynamic_cast<const artery::CollectivePerceptionMockMessage*>(obj);
    if (!cpm) {
        return;
    }

    if (signalID == cpmSentSignal) {
        mCpmSentLifetime++;
        for (const auto& objContainer : cpm->getObjectContainers()) {
            auto obj_ptr = objContainer.object.lock();
            if (obj_ptr) {
                mUniqueObjectsDetectedLifetime.insert(obj_ptr->getExternalId());
            }
        }
    }
    else if (signalID == cpmReceivedSignal) {
        mCpmReceivedLifetime++;

        for (const auto& objContainer : cpm->getObjectContainers()) {
            auto obj_ptr = objContainer.object.lock();
            if (obj_ptr) {
                std::string extId = obj_ptr->getExternalId();
                mUniqueObjectsIdentifiedLifetime.insert(extId);
                if (isMRSU) {
                    windowCpmReceivedCount++;
                    windowPerceivedObjects.insert(extId);
                }
            }
        }
    }
}

void TestBroadcastServiceBremen::handleMessage(cMessage* msg)
{
    if (msg == mWindowTimer) {
        reportWindow();
        scheduleAt(simTime() + WINDOW_INTERVAL, mWindowTimer);
    }
    else if (msg == mRelayTimer) {
        broadcastRelay();
        scheduleAt(simTime() + RELAY_INTERVAL, mRelayTimer);
    }
    else {
        delete msg;
    }
}

void TestBroadcastServiceBremen::reportWindow()
{
    windowNumber++;

	std::vector<std::string> allVehicleIds;
    try {
        auto idList = mVehicleController->getTraCI()->vehicle.getIDList();
        allVehicleIds.assign(idList.begin(), idList.end());
    }
    catch (const libsumo::TraCIException& e) {
        std::cout << "[WARN] Could not query total vehicle list: " << e.what() << std::endl;
    }

    long totalExpected = 0;
    long totalReceived = 0;
    int activeSenders = 0;

    std::vector<std::string> connectedVehicleIds;

    for (auto& entry : senderStats) {
        SenderRecord& r = entry.second;

        if (r.windowStartSentCount == -1 || r.receivedCountThisWindow == 0) {
            continue;
        }

        long expectedThisSender = (r.lastKnownSentCount - r.windowStartSentCount) + 1;
        totalExpected += expectedThisSender;
        totalReceived += r.receivedCountThisWindow;
        activeSenders++;
        connectedVehicleIds.push_back(entry.first);

        if (allVehicleIds.size() > 0) {
            double awarenessRatio = static_cast<double>(r.lastKnownAwareness) / static_cast<double>(allVehicleIds.size());
            awarenessRatio = std::min(awarenessRatio, 1.0);
            windowAwarenessSum += awarenessRatio;
            windowAwarenessSampleCount++;
        }

        r.windowStartSentCount = r.lastKnownSentCount;
        r.receivedCountThisWindow = 0;
    }

    double windowPDR = 0;
    if (totalExpected > 0) {
        windowPDR = static_cast<double>(totalReceived) / totalExpected;
        windowPDR = std::min(windowPDR, 1.0);
    }

    cumulativeExpected += totalExpected;
    cumulativeReceived += totalReceived;
    sumOfWindowPDRs += windowPDR;

    double avgDelayThisWindow = 0;
    double avgVelocityThisWindow = 0;
    if (windowMessageCount > 0) {
        avgDelayThisWindow = windowDelaySum / windowMessageCount;
        avgVelocityThisWindow = windowSpeedSum / windowMessageCount;
    }

    double avgCbrThisWindow = 0;
    if (windowCbrSampleCount > 0) {
        avgCbrThisWindow = windowCbrSum / windowCbrSampleCount;
    }

    double avgAwarenessThisWindow = 0;
    if (windowAwarenessSampleCount > 0) {
        avgAwarenessThisWindow = windowAwarenessSum / windowAwarenessSampleCount;
    }

    const std::string myId = mVehicleController->getVehicleId();

    std::cout << "[SSM] " << myId << " window#" << windowNumber
               << " t=" << simTime()
               << " activeSenders=" << activeSenders
               << " expected=" << totalExpected
               << " received=" << totalReceived
               << " windowPDR=" << windowPDR
               << " avgDelay=" << avgDelayThisWindow
               << " avgVelocity=" << avgVelocityThisWindow
               << " avgCBR=" << avgCbrThisWindow
			   << " avgAwareness=" << avgAwarenessThisWindow
               << " receivedCPM=" << windowCpmReceivedCount
               << " perceivedObjects=" << windowPerceivedObjects.size() << std::endl;

    

    // ensure anyone counted as "connected" is also counted as "present",
    // even if they left the simulation between being heard and this snapshot
    for (const auto& vid : connectedVehicleIds) {
        if (std::find(allVehicleIds.begin(), allVehicleIds.end(), vid) == allVehicleIds.end()) {
            allVehicleIds.push_back(vid);
        }
    }

    SegmentCoordinationServer::getInstance().reportSSM(myId, totalExpected, totalReceived,
                                                          windowPDR, avgDelayThisWindow,
                                                          avgVelocityThisWindow, avgCbrThisWindow,
                                                          avgAwarenessThisWindow,
                                                          connectedVehicleIds, allVehicleIds, simTime().dbl());

    // pull the latest SCM and check if it's new since we last checked
    const SegmentCoordinationMessage& scm = SegmentCoordinationServer::getInstance().getLatestSCM();
    if (scm.scmNumber > lastSeenScmNumber) {
        lastSeenScmNumber = scm.scmNumber;
        std::cout << "[SCM RECEIVED] " << myId << " got SCM #" << scm.scmNumber
                   << " sentAt=" << scm.timestamp
                   << "s receivedAt=" << simTime()
                   << "s (segmentPDR=" << scm.segmentPDR
                   << " segmentAvgDelay=" << scm.segmentAvgDelay
                   << "s segmentAvgVelocity=" << scm.segmentAvgVelocity
                   << "m/s coverage=" << scm.coverage
                   << " activeMrsus=" << scm.activeMrsus
                   << " connectedVehicles=" << scm.connectedVehicles
                   << " totalVehicles=" << scm.totalVehicles
                   << " action=" << scm.recommendedAction
                   << " target=" << scm.recommendedTargetMrsu
                   << " myState=" << (SegmentCoordinationServer::getInstance().isActive(myId) ? "ACTIVE" : "SLEEPING")
                   << ")" << std::endl;
    }

    windowDelaySum = 0;
    windowSpeedSum = 0;
    windowMessageCount = 0;

    windowCbrSum = 0;
    windowCbrSampleCount = 0;
	
	windowAwarenessSum = 0;
    windowAwarenessSampleCount = 0;

    windowCpmReceivedCount = 0;
    windowPerceivedObjects.clear();
}


void TestBroadcastServiceBremen::broadcastRelay()
{
    if (!SegmentCoordinationServer::getInstance().isActive(mVehicleController->getVehicleId())) {
        return; // sleeping MRSUs don't relay
    }

    try {
        std::string idList;
        std::string dataList;
        int count = 0;

        const double now = simTime().dbl();
        const double RELAY_FRESHNESS_WINDOW = 3.0; // only relay vehicles heard within the last 3s

        for (const auto& entry : senderStats) {
            const SenderRecord& r = entry.second;
            if ((now - r.lastHeardTime) > RELAY_FRESHNESS_WINDOW) {
                continue; // stale, skip
            }

            if (count > 0) {
                idList += ",";
                dataList += ",";
            }
            idList += entry.first;
            dataList += std::to_string(r.lastKnownPosX) + ":" +
                        std::to_string(r.lastKnownPosY) + ":" +
                        std::to_string(r.lastKnownSpeed);
            count++;
        }

        if (count == 0) {
            return; // nothing fresh to relay
        }

        btp::DataRequestB req;
        req.destination_port = host_cast<TestBroadcastServiceBremen::port_type>(getPortNumber());
        req.gn.transport_type = geonet::TransportType::SHB;
        req.gn.traffic_class.tc_id(static_cast<unsigned>(dcc::Profile::DP1));
        req.gn.communication_profile = geonet::CommunicationProfile::ITS_G5;

        auto packet = new MrsuRelay();
        packet->setMrsuId(mVehicleController->getVehicleId().c_str());
        packet->setTimestamp(now);
        packet->setRelayedVehicleIds(idList.c_str());
        packet->setRelayedData(dataList.c_str());
        packet->setVehicleCount(count);
        packet->setByteLength(20 + count * 24); // rough estimate for realistic channel load

        mRelaySentLifetime++;

        std::cout << "[RELAY TX] " << mVehicleController->getVehicleId()
                   << " broadcasting relay #" << mRelaySentLifetime
                   << " with " << count << " vehicle(s) at t=" << now << std::endl;

        request(req, packet);
    }
    catch (const libsumo::TraCIException& e) {
        std::cout << "[WARN] broadcastRelay() skipped: " << e.what() << std::endl;
    }
}


void TestBroadcastServiceBremen::trigger()
{
    Enter_Method("TestBroadcastServiceBremen trigger");

    try {
        btp::DataRequestB req;
        req.destination_port = host_cast<TestBroadcastServiceBremen::port_type>(getPortNumber());
        req.gn.transport_type = geonet::TransportType::SHB;
        req.gn.traffic_class.tc_id(static_cast<unsigned>(dcc::Profile::DP1));
        req.gn.communication_profile = geonet::CommunicationProfile::ITS_G5;

        const std::string id = mVehicleController->getVehicleId();
	

        auto pos = mVehicleController->getPosition();
        double speed = mVehicleController->getSpeed().value();

        packetsSent++;

        std::set<std::string> unifiedKnown = mDirectlyKnownVehicles;
        unifiedKnown.insert(mRelayLearnedVehicles.begin(), mRelayLearnedVehicles.end());
        long knownCount = static_cast<long>(unifiedKnown.size());

        auto packet = new TestBroadcast();
        packet->setVehicleId(id.c_str());
        packet->setTimestamp(simTime().dbl());
        packet->setPosX(pos.x.value());
        packet->setPosY(pos.y.value());
        packet->setSpeed(speed);
        packet->setPacketsSentTillNow(packetsSent);
        packet->setPacketsReceivedTillNow(packetsReceived);
        packet->setKnownVehicleCount(knownCount);
        packet->setByteLength(60);

        std::cout << "[TX] " << id << " sent=" << packetsSent
                   << " recv=" << packetsReceived << " at t=" << simTime()
                   << "s with p=" << mTransmitPower << "W" << std::endl;

        request(req, packet);
    }
    catch (const libsumo::TraCIException& e) {
        std::cout << "[WARN] trigger() skipped, vehicle likely mid-teardown: " << e.what() << std::endl;
    }
}

void TestBroadcastServiceBremen::indicate(const vanetza::btp::DataIndication&, omnetpp::cPacket* packet)
{
    Enter_Method("TestBroadcastServiceBremen indicate");

    if (auto relay = dynamic_cast<const MrsuRelay*>(packet)) {
        mRelayReceivedLifetime++;

        std::string idListStr = relay->getRelayedVehicleIds();
        std::stringstream ss(idListStr);
        std::string vehId;
        while (std::getline(ss, vehId, ',')) {
            if (!vehId.empty()) {
                mRelayLearnedVehicles.insert(vehId);
            }
        }

        std::cout << "[RELAY RX] " << mVehicleController->getVehicleId()
                   << " received relay from " << relay->getMrsuId()
                   << " covering " << relay->getVehicleCount() << " vehicle(s)" << std::endl;
        delete packet;
        return;
    }

    auto msg = check_and_cast<const TestBroadcast*>(packet);
    const std::string myId = mVehicleController->getVehicleId();
    const std::string senderId = msg->getVehicleId();

    mDirectlyKnownVehicles.insert(senderId);

    simtime_t delay = simTime() - msg->getTimestamp();
    totalDelay += delay;
    packetsReceived++;

    windowDelaySum += delay.dbl();
    windowSpeedSum += msg->getSpeed();
    windowMessageCount++;

    if (isMRSU) {
        auto& record = senderStats[senderId];
        if (record.windowStartSentCount == -1) {
            record.windowStartSentCount = msg->getPacketsSentTillNow();
        }
        record.lastKnownSentCount = msg->getPacketsSentTillNow();
        record.receivedCountThisWindow++;
        record.lastKnownPosX = msg->getPosX();
        record.lastKnownPosY = msg->getPosY();
        record.lastKnownSpeed = msg->getSpeed();
        record.lastHeardTime = simTime().dbl();
        record.lastKnownAwareness = msg->getKnownVehicleCount();

        std::cout << "[MRSU RX] " << myId << " logged " << senderId
                   << " (sender total sent=" << record.lastKnownSentCount
                   << ", this-window received=" << record.receivedCountThisWindow << ")" << std::endl;
    }
    else {
        std::cout << "[RX] " << myId << " received broadcast from " << senderId
                   << " sent at t=" << msg->getTimestamp()
                   << "s (now t=" << simTime() << "s)" << std::endl;
    }

    delete msg;
}

void TestBroadcastServiceBremen::finish()
{
    if (isMRSU) {
        double cumulativePDR = 0;
        if (cumulativeExpected > 0) {
            cumulativePDR = static_cast<double>(cumulativeReceived) / cumulativeExpected;
            cumulativePDR = std::min(cumulativePDR, 1.0);
        }

        double avgWindowPDR = 0;
        if (windowNumber > 0) {
            avgWindowPDR = sumOfWindowPDRs / windowNumber;
        }

        recordScalar("Total Windows Reported", windowNumber);
        recordScalar("Cumulative Expected", cumulativeExpected);
        recordScalar("Cumulative Received", cumulativeReceived);
        recordScalar("Cumulative PDR", cumulativePDR);
        recordScalar("Average Window PDR", avgWindowPDR);

        if (mWindowTimer) {
            cancelAndDelete(mWindowTimer);
            mWindowTimer = nullptr;
        }
        if (mRelayTimer) {
            cancelAndDelete(mRelayTimer);
            mRelayTimer = nullptr;
        }
    }

    // --- Communication Summary Report (all vehicles) ---
    if (!sCommReportFileOpen) {
        sCommReportFile.open("results/communication_summary.csv");
        sCommReportFileOpen = true;
        if (sCommReportFile.is_open()) {
            sCommReportFile << "vehicle_id,role,time_in_sim,cam_sent,cpm_sent,unique_objects_detected,"
                            << "cam_received,cpm_received,unique_objects_identified,relay_sent,relay_received,"
                            << "directly_known_vehicles,relay_learned_vehicles,extended_awareness\n";
        }
    }

    if (sCommReportFile.is_open()) {
        const std::string myId = mVehicleController->getVehicleId();
        double timeInSim = simTime().dbl() - mStartTime;
        std::string role = isMRSU ? "MRSU" : "V2X";

        int extendedAwareness = 0;
        for (const auto& vid : mRelayLearnedVehicles) {
            if (mDirectlyKnownVehicles.find(vid) == mDirectlyKnownVehicles.end()) {
                extendedAwareness++;
            }
        }

        sCommReportFile << myId << ","
                        << role << ","
                        << timeInSim << ","
                        << packetsSent << ","
                        << mCpmSentLifetime << ","
                        << mUniqueObjectsDetectedLifetime.size() << ","
                        << packetsReceived << ","
                        << mCpmReceivedLifetime << ","
                        << mUniqueObjectsIdentifiedLifetime.size() << ","
                        << mRelaySentLifetime << ","
                        << mRelayReceivedLifetime << ","
                        << mDirectlyKnownVehicles.size() << ","
                        << mRelayLearnedVehicles.size() << ","
                        << extendedAwareness << "\n";
        sCommReportFile.flush();
    }
}