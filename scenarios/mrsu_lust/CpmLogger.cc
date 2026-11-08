#include "CpmLogger.h"
#include "artery/envmod/service/CollectivePerceptionMockMessage.h"
#include "test_msgs/TestCpm_m.h"
#include "artery/envmod/EnvironmentModelObject.h"
#include "artery/traci/VehicleController.h"
#include <omnetpp/checkandcast.h>
#include <iostream>
#include <sstream>

using namespace omnetpp;
using namespace artery;

std::ofstream CpmLoggerLuST::sReportFile;
bool CpmLoggerLuST::sReportFileOpen = false;

Define_Module(CpmLoggerLuST)

void CpmLoggerLuST::initialize()
{
    ItsG5Service::initialize();
    mVehicleController = &getFacilities().get_const<traci::VehicleController>();
    if (!sReportFileOpen) {
        sReportFile.open("results/cpm_report.txt");
        sReportFileOpen = true;
        if (sReportFile.is_open()) {
            sReportFile << "=== CPM Perception Report ===\n\n";
        }
    }
    simsignal_t cpmSentSignal = cComponent::registerSignal("CpmSent");
    subscribe(cpmSentSignal);
}

void CpmLoggerLuST::indicate(const vanetza::btp::DataIndication&, omnetpp::cPacket* packet)
{
    delete packet;
}

void CpmLoggerLuST::receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details)
{
    const std::string myId = mVehicleController->getVehicleId();

    // Path 1: our own, fully custom CPM packet
    auto ownCpm = dynamic_cast<const TestCpm*>(obj);
    if (ownCpm) {
        int count = ownCpm->getObjectCount();
        mCpmSentCount++;
        mTotalObjectsReported += count;
        if (false) std::cout << "[CPM] " << myId << " sent CPM #" << mCpmSentCount
                   << " at t=" << simTime() << " with " << count << " perceived object(s)";

        std::string ids = ownCpm->getObjectIds();
        if (sReportFile.is_open() && count > 0) {
            sReportFile << "[t=" << simTime() << "] " << myId << " perceived " << count << " object(s): ";
        }
        std::stringstream ss(ids);
        std::string externalId;
        while (std::getline(ss, externalId, ',')) {
            if (externalId.empty()) continue;
            mPerceivedObjectCounts[externalId]++;
            if (false) std::cout << " " << externalId;
            if (sReportFile.is_open()) {
                sReportFile << externalId << " ";
            }
        }
        if (false) std::cout << std::endl;
        if (sReportFile.is_open() && count > 0) {
            sReportFile << "\n";
        }
        return;
    }

    // Path 2: legacy Artery mock CPM (kept for backward compatibility, unused once services.xml is switched over)
    auto cpm = dynamic_cast<const CollectivePerceptionMockMessage*>(obj);
    if (!cpm) {
        return;
    }
    const auto& objects = cpm->getObjectContainers();
    mCpmSentCount++;
    mTotalObjectsReported += objects.size();
    if (false) std::cout << "[CPM] " << myId << " sent CPM #" << mCpmSentCount
               << " at t=" << simTime() << " with " << objects.size() << " perceived object(s)";
    if (sReportFile.is_open() && !objects.empty()) {
        sReportFile << "[t=" << simTime() << "] " << myId << " perceived " << objects.size() << " object(s): ";
    }
    for (const auto& objContainer : objects) {
        auto obj_ptr = objContainer.object.lock();
        std::string externalId = obj_ptr ? obj_ptr->getExternalId() : "unknown";
        mPerceivedObjectCounts[externalId]++;
        if (false) std::cout << " " << externalId;
        if (sReportFile.is_open()) {
            sReportFile << externalId << " ";
        }
    }
    if (false) std::cout << std::endl;
    if (sReportFile.is_open() && !objects.empty()) {
        sReportFile << "\n";
    }
}

void CpmLoggerLuST::finish()
{
    const std::string myId = mVehicleController->getVehicleId();
    std::cout << "[CPM SUMMARY] " << myId << " sent " << mCpmSentCount
               << " CPMs, reporting " << mTotalObjectsReported << " total object observations" << std::endl;
    if (sReportFile.is_open()) {
        sReportFile << "--- Summary for " << myId << ": " << mCpmSentCount
                     << " CPMs sent, " << mTotalObjectsReported << " total object observations ---\n";
        for (const auto& entry : mPerceivedObjectCounts) {
            sReportFile << "    perceived " << entry.first << " " << entry.second << " time(s)\n";
        }
        sReportFile << "\n";
        sReportFile.flush();
    }
    ItsG5Service::finish();
}