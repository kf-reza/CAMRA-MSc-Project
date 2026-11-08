#include "CpmLogger.h"
#include "artery/envmod/service/CollectivePerceptionMockMessage.h"
#include "artery/envmod/EnvironmentModelObject.h"
#include "artery/traci/VehicleController.h"
#include <omnetpp/checkandcast.h>
#include <iostream>

using namespace omnetpp;
using namespace artery;

std::ofstream CpmLogger::sReportFile;
bool CpmLogger::sReportFileOpen = false;

Define_Module(CpmLogger)

void CpmLogger::initialize()
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

void CpmLogger::indicate(const vanetza::btp::DataIndication&, omnetpp::cPacket* packet)
{
    delete packet;
}

void CpmLogger::receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details)
{
    auto cpm = dynamic_cast<const CollectivePerceptionMockMessage*>(obj);
    if (!cpm) {
        return;
    }

    const std::string myId = mVehicleController->getVehicleId();
    const auto& objects = cpm->getObjectContainers();

    mCpmSentCount++;
    mTotalObjectsReported += objects.size();

    std::cout << "[CPM] " << myId << " sent CPM #" << mCpmSentCount
               << " at t=" << simTime() << " with " << objects.size() << " perceived object(s)";

    if (sReportFile.is_open() && !objects.empty()) {
        sReportFile << "[t=" << simTime() << "] " << myId << " perceived " << objects.size() << " object(s): ";
    }

    for (const auto& objContainer : objects) {
        auto obj_ptr = objContainer.object.lock();
        std::string externalId = obj_ptr ? obj_ptr->getExternalId() : "unknown";

        mPerceivedObjectCounts[externalId]++;

        std::cout << " " << externalId;
        if (sReportFile.is_open()) {
            sReportFile << externalId << " ";
        }
    }

    std::cout << std::endl;
    if (sReportFile.is_open() && !objects.empty()) {
        sReportFile << "\n";
    }
}

void CpmLogger::finish()
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