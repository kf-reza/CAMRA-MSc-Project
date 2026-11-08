#include "TestCpmServiceBremen.h"
#include "test_msgs/TestCpm_m.h"
#include "artery/envmod/EnvironmentModelObject.h"
#include "artery/utility/InitStages.h"
#include "artery/utility/Identity.h"
#include <vanetza/btp/data_request.hpp>
#include <vanetza/dcc/profile.hpp>
#include <vanetza/geonet/interface.hpp>
#include <omnetpp/checkandcast.h>
#include <iostream>
#include <sstream>

using namespace omnetpp;
using namespace vanetza;

Define_Module(TestCpmServiceBremen3)

namespace {
    omnetpp::simsignal_t ownCpmSentSignal = omnetpp::cComponent::registerSignal("CpmSent");
    omnetpp::simsignal_t ownCpmReceivedSignal = omnetpp::cComponent::registerSignal("CpmReceived");
}

TestCpmServiceBremen3::~TestCpmServiceBremen3()
{
    cancelAndDelete(mTrigger);
}

int TestCpmServiceBremen3::numInitStages() const
{
    return artery::InitStages::Total;
}

void TestCpmServiceBremen3::initialize(int stage)
{
    if (stage == artery::InitStages::Prepare) {
        ItsG5Service::initialize();
        mPositionProvider = &getFacilities().get_const<artery::PositionProvider>();
        mEnvironmentModel = &getFacilities().get_const<artery::LocalEnvironmentModel>();
        mCpmInterval = par("cpmInterval");
    }
    else if (stage == artery::InitStages::Self) {
        mTrigger = new cMessage("cpm trigger");
        mHostId = getFacilities().get_const<artery::Identity>().host->getId();
        scheduleAt(simTime() + mCpmInterval, mTrigger);
    }
}

void TestCpmServiceBremen3::handleMessage(cMessage* msg)
{
    if (msg == mTrigger) {
        generatePacket();
        scheduleAt(simTime() + mCpmInterval, mTrigger);
    }
    else {
        ItsG5Service::handleMessage(msg);
    }
}

void TestCpmServiceBremen3::trigger()
{
    // periodic scheduling handled directly in handleMessage
}

void TestCpmServiceBremen3::indicate(const vanetza::btp::DataIndication&, omnetpp::cPacket* packet)
{
    mCpmReceivedLifetime++;
    emit(ownCpmReceivedSignal, packet);
    delete packet;
}

void TestCpmServiceBremen3::generatePacket()
{
    vanetza::btp::DataRequestB req;
    req.destination_port = host_cast<TestCpmServiceBremen3::port_type>(getPortNumber());
    req.gn.transport_type = vanetza::geonet::TransportType::SHB;
    req.gn.traffic_class.tc_id(static_cast<unsigned>(vanetza::dcc::Profile::DP2));
    req.gn.communication_profile = vanetza::geonet::CommunicationProfile::ITS_G5;

    auto packet = new TestCpm();
    packet->setSourceStation(mHostId);
    packet->setTimestamp(simTime().dbl());

    std::ostringstream idList;
    int count = 0;
    using TrackedObject = artery::LocalEnvironmentModel::TrackedObject;
    for (const TrackedObject& object : mEnvironmentModel->allObjects()) {
        const artery::LocalEnvironmentModel::Tracking& tracking = object.second;
        if (tracking.expired()) {
            continue; // skip objects with lost tracking, same as Artery's own logic
        }
        auto obj_ptr = object.first.lock();
        if (!obj_ptr) {
            continue;
        }
        if (count > 0) {
            idList << ",";
        }
        idList << obj_ptr->getExternalId();
        count++;
    }

    packet->setObjectCount(count);
    packet->setObjectIds(idList.str().c_str());
    packet->setByteLength(20 + count * 8);

    mCpmSentLifetime++;
    emit(ownCpmSentSignal, packet);
    std::cout << "[CPM-OWN TX] host=" << mHostId << " sent CPM #" << mCpmSentLifetime
               << " at t=" << simTime().dbl() << " with " << count << " perceived object(s)" << std::endl;

    request(req, packet);
}