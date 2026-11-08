#include "SegmentCoordinationServer.h"
#include <iostream>
#include <algorithm>

SegmentCoordinationServer::SegmentCoordinationServer()
{
    std::string csvName = "results/scm_history_CAMRA.csv";
    if (MODE == Strategy::ALWAYS_ACTIVE) csvName = "results/scm_history_baseline_allactive.csv";
    else if (MODE == Strategy::FIXED_SINGLE) csvName = "results/scm_history_baseline_fixedsingle.csv";
    else if (MODE == Strategy::STATIC_PERIODIC) csvName = "results/scm_history_baseline_staticperiodic.csv";
    mCsvFile.open(csvName);
    if (mCsvFile.is_open()) {
        mCsvFile << "scm_number,time,active_mrsus,reporting_mrsus,total_expected,total_received,segment_pdr,segment_avg_delay,segment_avg_velocity,segment_avg_cbr,segment_awareness,connected_vehicles,total_vehicles,coverage,e_pdr,e_delay,e_coverage,gamma_pdr,gamma_delay,gamma_coverage,qos_error,qos_status,recommended_action,recommended_target,cbr_status\n";
        mCsvFile.flush();
    }
    else {
        std::cout << "[SCS WARNING] Could not open results/scm_history.csv for writing" << std::endl;
    }

    mCallForHelpFile.open("results/call_for_help.csv");
    if (mCallForHelpFile.is_open()) {
        mCallForHelpFile << "scm_number,time,active_mrsus,qos_error,dominant_deficit,e_pdr,e_delay,e_coverage,unserved_vehicles,message\n";
        mCallForHelpFile.flush();
    }
    else {
        std::cout << "[SCS WARNING] Could not open results/call_for_help.csv for writing" << std::endl;
    }
}

SegmentCoordinationServer::~SegmentCoordinationServer()
{
    if (mCsvFile.is_open()) {
        mCsvFile.close();
    }
    if (mCallForHelpFile.is_open()) {
        mCallForHelpFile.close();
    }
}

SegmentCoordinationServer& SegmentCoordinationServer::getInstance()
{
    static SegmentCoordinationServer instance;
    return instance;
}

const SegmentCoordinationMessage& SegmentCoordinationServer::getLatestSCM() const
{
    return mLastScm;
}

bool SegmentCoordinationServer::isActive(const std::string& mrsuId) const
{
    auto it = mActiveState.find(mrsuId);
    return (it != mActiveState.end()) ? it->second : false;
}

void SegmentCoordinationServer::reportSSM(const std::string& mrsuId, long expected, long received,
                                           double windowPDR, double avgDelay, double avgVelocity, double avgCbr,
                                           double avgAwareness,
                                           const std::vector<std::string>& connectedVehicleIds,
                                           const std::vector<std::string>& allVehicleIds, double now)
{
    MrsuReport& report = mLatestReports[mrsuId];
    report.expected = expected;
    report.received = received;
    report.windowPDR = windowPDR;
    report.avgDelay = avgDelay;
    report.avgVelocity = avgVelocity;
    report.avgCbr = avgCbr;
    report.avgAwareness = avgAwareness;
    report.connectedVehicleIds = connectedVehicleIds;
    report.allVehicleIds = allVehicleIds;
    report.lastUpdated = now;

    bool isFirstContact = (mActiveState.find(mrsuId) == mActiveState.end());

    if (MODE == Strategy::ALWAYS_ACTIVE) {
        mActiveState[mrsuId] = true; // baseline: every candidate is always active
    }
    else {
        mActiveState[mrsuId]; // ensures entry exists, defaults to false (sleeping) on first contact
    }

    if (isFirstContact && MODE == Strategy::STATIC_PERIODIC) {
        mArrivalOrder.push_back(mrsuId); // record the order candidates first appeared
    }

    std::cout << "[SSM->SCS] " << mrsuId << " (" << (mActiveState[mrsuId] ? "ACTIVE" : "SLEEPING")
               << ") reported at t=" << now
               << " (expected=" << expected << " received=" << received
               << " windowPDR=" << windowPDR << " avgVelocity=" << avgVelocity
               << " avgCbr=" << avgCbr
               << " connectedVehicles=" << connectedVehicleIds.size()
               << " totalVehicles=" << allVehicleIds.size() << ")" << std::endl;

    if ((now - mLastAggregation) >= SCM_INTERVAL) {
        aggregateAndPublish(now);
    }
}

void SegmentCoordinationServer::aggregateAndPublish(double now)
{
    mScmNumber++;
	// (baseline mode still runs the same aggregation below; only the activation/sleep
    //  DECISION logic is skipped further down, since every candidate is already active)

    std::string bestSleepingCandidate;
    long bestSleepingSi = -1;
    std::string worstActiveCandidate;
    long worstActiveSiMin = -1;
    bool haveActiveCandidate = false;

    int activeMrsus = 0;
    int reportingMrsus = 0; // active + sleeping, anyone who reported recently

    long totalExpected = 0;
    long totalReceived = 0;
    double sumDelay = 0;
    double sumVelocity = 0;
    double sumCbr = 0;
	double sumAwareness = 0;

    std::set<std::string> unionConnectedVehicles; // active-only (coverage numerator)
    std::set<std::string> unionAllVehicles;        // all reporting (coverage denominator)

    for (const auto& entry : mLatestReports) {
        const std::string& mrsuId = entry.first;
        const MrsuReport& r = entry.second;

        if ((now - r.lastUpdated) > SCM_INTERVAL) {
            continue; // stale, not currently reporting
        }

        long si = static_cast<long>(r.connectedVehicleIds.size());
        bool active = isActive(mrsuId);

        // measurement data: counted for EVERY reporting MRSU, active or sleeping
        reportingMrsus++;
        totalExpected += r.expected;
        totalReceived += r.received;
        sumDelay += r.avgDelay;
        sumVelocity += r.avgVelocity;
        sumCbr += r.avgCbr;
        sumAwareness += r.avgAwareness;
        for (const auto& vid : r.allVehicleIds) unionAllVehicles.insert(vid);

        if (active) {
            if (!haveActiveCandidate || si < worstActiveSiMin) {
                worstActiveSiMin = si;
                worstActiveCandidate = mrsuId;
                haveActiveCandidate = true;
            }
            activeMrsus++;
            for (const auto& vid : r.connectedVehicleIds) unionConnectedVehicles.insert(vid);
        }
        else {
            if (si > bestSleepingSi) {
                bestSleepingSi = si;
                bestSleepingCandidate = mrsuId;
            }
        }
    }

    double segmentPDR = 0;
    if (totalExpected > 0) {
        segmentPDR = std::min(static_cast<double>(totalReceived) / totalExpected, 1.0);
    }

    double segmentAvgDelay = 0, segmentAvgVelocity = 0, segmentAvgCbr = 0, segmentAwareness = 0;
    if (reportingMrsus > 0) {
        segmentAvgDelay = sumDelay / reportingMrsus;
        segmentAvgVelocity = sumVelocity / reportingMrsus;
        segmentAvgCbr = sumCbr / reportingMrsus;
        segmentAwareness = sumAwareness / reportingMrsus;
    }

    long connectedCount = static_cast<long>(unionConnectedVehicles.size()); // active-only
    long totalCount = static_cast<long>(unionAllVehicles.size());           // all reporting
    double coverage = 0;
    if (totalCount > 0) {
        coverage = std::min(static_cast<double>(connectedCount) / totalCount, 1.0);
    }

    double e_pdr = std::max(0.0, (PDR_TARGET - segmentPDR) / PDR_TARGET);
    double e_delay = std::max(0.0, (segmentAvgDelay - DELAY_TARGET) / DELAY_TARGET);
    double e_coverage = std::max(0.0, (COVERAGE_TARGET - coverage) / COVERAGE_TARGET);

    double sumWithEpsilon = (e_pdr + EPSILON) + (e_delay + EPSILON) + (e_coverage + EPSILON);
    double gamma_pdr = (e_pdr + EPSILON) / sumWithEpsilon;
    double gamma_delay = (e_delay + EPSILON) / sumWithEpsilon;
    double gamma_coverage = (e_coverage + EPSILON) / sumWithEpsilon;

    double qosError = gamma_pdr * e_pdr + gamma_delay * e_delay + gamma_coverage * e_coverage;

    std::string qosStatus;
    std::string recommendedAction = "NONE";
    std::string recommendedTargetMrsu;

    if (MODE == Strategy::ALWAYS_ACTIVE) {
        // baseline: no decisions, just compute and record E for comparison
        e_pdr = std::max(0.0, (PDR_TARGET - segmentPDR) / PDR_TARGET);
        e_delay = std::max(0.0, (segmentAvgDelay - DELAY_TARGET) / DELAY_TARGET);
        e_coverage = std::max(0.0, (COVERAGE_TARGET - coverage) / COVERAGE_TARGET);
        double sumWithEpsilon = (e_pdr + EPSILON) + (e_delay + EPSILON) + (e_coverage + EPSILON);
        gamma_pdr = (e_pdr + EPSILON) / sumWithEpsilon;
        gamma_delay = (e_delay + EPSILON) / sumWithEpsilon;
        gamma_coverage = (e_coverage + EPSILON) / sumWithEpsilon;
        qosError = gamma_pdr * e_pdr + gamma_delay * e_delay + gamma_coverage * e_coverage;
        if (qosError <= E_COMFORTABLE) qosStatus = "COMFORTABLE";
        else if (qosError <= E_MONITOR) qosStatus = "MONITOR";
        else qosStatus = "INCREASE_RESOURCES";
        recommendedAction = "BASELINE_NO_DECISION";
    }
    
	else if (MODE == Strategy::STATIC_PERIODIC) {
        // first-come-first-served: whoever arrived first stays active until it leaves
        e_pdr = std::max(0.0, (PDR_TARGET - segmentPDR) / PDR_TARGET);
        e_delay = std::max(0.0, (segmentAvgDelay - DELAY_TARGET) / DELAY_TARGET);
        e_coverage = std::max(0.0, (COVERAGE_TARGET - coverage) / COVERAGE_TARGET);
        double sumWithEpsilon = (e_pdr + EPSILON) + (e_delay + EPSILON) + (e_coverage + EPSILON);
        gamma_pdr = (e_pdr + EPSILON) / sumWithEpsilon;
        gamma_delay = (e_delay + EPSILON) / sumWithEpsilon;
        gamma_coverage = (e_coverage + EPSILON) / sumWithEpsilon;
        qosError = gamma_pdr * e_pdr + gamma_delay * e_delay + gamma_coverage * e_coverage;
        if (qosError <= E_COMFORTABLE) qosStatus = "COMFORTABLE";
        else if (qosError <= E_MONITOR) qosStatus = "MONITOR";
        else qosStatus = "INCREASE_RESOURCES";

        if (!haveActiveCandidate) {
            // nobody active right now: find the earliest-arrived candidate that is
            // currently reporting (present) and not yet active
            for (const auto& candidateId : mArrivalOrder) {
                auto it = mLatestReports.find(candidateId);
                if (it != mLatestReports.end() && (now - it->second.lastUpdated) <= SCM_INTERVAL) {
                    mActiveState[candidateId] = true;
                    recommendedAction = "STATIC_FCFS_ACTIVATE";
                    recommendedTargetMrsu = candidateId;
                    break;
                }
            }
        }
        else {
            recommendedAction = "STATIC_FCFS_KEEP";
            recommendedTargetMrsu = worstActiveCandidate; // the one (and only) active candidate
        }
    }
    else if (MODE == Strategy::FIXED_SINGLE) {
        // baseline: keep exactly one MRSU active, only swap to a candidate with strictly more neighbours
        e_pdr = std::max(0.0, (PDR_TARGET - segmentPDR) / PDR_TARGET);
        e_delay = std::max(0.0, (segmentAvgDelay - DELAY_TARGET) / DELAY_TARGET);
        e_coverage = std::max(0.0, (COVERAGE_TARGET - coverage) / COVERAGE_TARGET);
        double sumWithEpsilon = (e_pdr + EPSILON) + (e_delay + EPSILON) + (e_coverage + EPSILON);
        gamma_pdr = (e_pdr + EPSILON) / sumWithEpsilon;
        gamma_delay = (e_delay + EPSILON) / sumWithEpsilon;
        gamma_coverage = (e_coverage + EPSILON) / sumWithEpsilon;
        qosError = gamma_pdr * e_pdr + gamma_delay * e_delay + gamma_coverage * e_coverage;
        if (qosError <= E_COMFORTABLE) qosStatus = "COMFORTABLE";
        else if (qosError <= E_MONITOR) qosStatus = "MONITOR";
        else qosStatus = "INCREASE_RESOURCES";

        if (!haveActiveCandidate) {
            if (!bestSleepingCandidate.empty() && bestSleepingSi > 0) {
                mActiveState[bestSleepingCandidate] = true;
                recommendedAction = "FIXED_SINGLE_ACTIVATE";
                recommendedTargetMrsu = bestSleepingCandidate;
            }
        }
        else if (!bestSleepingCandidate.empty() && bestSleepingSi > worstActiveSiMin) {
            mActiveState[worstActiveCandidate] = false;
            mActiveState[bestSleepingCandidate] = true;
            recommendedAction = "FIXED_SINGLE_SWAP";
            recommendedTargetMrsu = bestSleepingCandidate + "<-" + worstActiveCandidate;
        }
        else {
            recommendedAction = "FIXED_SINGLE_KEEP";
            recommendedTargetMrsu = worstActiveCandidate;
        }
    }
    else if (activeMrsus == 0) {
        qosStatus = "BOOTSTRAP";
        if (!bestSleepingCandidate.empty() && bestSleepingSi > 0) {
            mActiveState[bestSleepingCandidate] = true;
            recommendedAction = "ACTIVATE_1";
            recommendedTargetMrsu = bestSleepingCandidate;
            std::cout << "[SCS BOOTSTRAP] Activating " << bestSleepingCandidate
                       << " (S_i=" << bestSleepingSi << ") - first MRSU for this segment" << std::endl;
        }
        mPreviousZone = "";
        mConsecutiveCount = 0;
    }
    else {
        if (qosError <= E_COMFORTABLE) qosStatus = "COMFORTABLE";
        else if (qosError <= E_MONITOR) qosStatus = "MONITOR";
        else qosStatus = "INCREASE_RESOURCES";

                if (qosStatus == "INCREASE_RESOURCES") {
            if (!bestSleepingCandidate.empty()) {
                mActiveState[bestSleepingCandidate] = true;
                recommendedAction = "ACTIVATE_1";
                recommendedTargetMrsu = bestSleepingCandidate;
            }
            else {
                // demand exists, but no candidate is available anywhere
				recommendedTargetMrsu = "CALL_FOR_HELP_SENT";
                std::string dominantDeficit = "coverage";
                double maxDeficit = e_coverage;
                if (e_pdr > maxDeficit) { dominantDeficit = "pdr"; maxDeficit = e_pdr; }
                if (e_delay > maxDeficit) { dominantDeficit = "delay"; maxDeficit = e_delay; }

                long unservedVehicles = totalCount - connectedCount;

                std::cout << "\n[CALL FOR HELP] Segment has demand (E=" << qosError
                           << ", dominant deficit=" << dominantDeficit
                           << ") but no MRSU candidate is available to activate. "
                           << unservedVehicles << " vehicle(s) currently unserved at t=" << now
                           << std::endl;

                if (mCallForHelpFile.is_open()) {
                    mCallForHelpFile << mScmNumber << "," << now << "," << activeMrsus << ","
                                      << qosError << "," << dominantDeficit << ","
                                      << e_pdr << "," << e_delay << "," << e_coverage << ","
                                      << unservedVehicles << ","
                                      << "\"Segment requesting additional MRSU capacity\"\n";
                    mCallForHelpFile.flush();
                }
            }
            mPreviousZone = qosStatus;
			mPreviousError = qosError;
            mConsecutiveCount = 1;
        }
        else if (qosStatus == mPreviousZone && (qosStatus == "MONITOR" || qosStatus == "COMFORTABLE")) {
            bool errorIncreasing = qosError > mPreviousError;
            bool errorDecreasing = qosError < mPreviousError;

            if (qosStatus == "MONITOR" && errorIncreasing) {
                if (!bestSleepingCandidate.empty()) {
                    mActiveState[bestSleepingCandidate] = true;
                    recommendedAction = "ACTIVATE_1";
                    recommendedTargetMrsu = bestSleepingCandidate;
                }
            }
            else if (qosStatus == "COMFORTABLE" && errorDecreasing) {
                if (!worstActiveCandidate.empty()) {
                    mActiveState[worstActiveCandidate] = false;
                    recommendedAction = "SLEEP_1";
                    recommendedTargetMrsu = worstActiveCandidate;
                }
            }
            // MONITOR-decreasing and COMFORTABLE-increasing: no action, keep monitoring

            mPreviousZone = qosStatus;
            mPreviousError = qosError;
        }
        else {
            mPreviousZone = qosStatus;
            mPreviousError = qosError;
            mConsecutiveCount = 1;
        }
    }

    std::string cbrStatus;
    if (segmentAvgCbr < 0.40) cbrStatus = "UNDER_UTILIZED";
    else if (segmentAvgCbr <= 0.70) cbrStatus = "NORMAL";
    else cbrStatus = "CONGESTED";

    std::cout << "\n=== [SCM] Segment Coordination Message #" << mScmNumber
               << " at t=" << now << " ===" << std::endl;
    std::cout << "  Active MRSUs / Reporting: " << activeMrsus << " / " << reportingMrsus << std::endl;
    std::cout << "  Segment-wide Expected:    " << totalExpected << std::endl;
    std::cout << "  Segment-wide Received:    " << totalReceived << std::endl;
    std::cout << "  Segment-wide PDR:         " << segmentPDR << std::endl;
    std::cout << "  Segment-wide Avg Delay:   " << segmentAvgDelay << "s" << std::endl;
    std::cout << "  Segment-wide Avg Velocity:" << segmentAvgVelocity << " m/s" << std::endl;
    std::cout << "  Segment-wide Avg CBR:     " << segmentAvgCbr << std::endl;
	std::cout << "  Segment Awareness:        " << segmentAwareness << std::endl;
    std::cout << "  Connected Vehicles:       " << connectedCount << " (active-served)" << std::endl;
    std::cout << "  Total Vehicles Present:   " << totalCount << std::endl;
    std::cout << "  Infrastructure Coverage:  " << coverage << std::endl;
    std::cout << "  --- QoS Feedback Loop ---" << std::endl;
    std::cout << "  e_pdr=" << e_pdr << " e_delay=" << e_delay << " e_coverage=" << e_coverage << std::endl;
    std::cout << "  gamma_pdr=" << gamma_pdr << " gamma_delay=" << gamma_delay << " gamma_coverage=" << gamma_coverage << std::endl;
    std::cout << "  QoS Error (E):            " << qosError << std::endl;
    std::cout << "  QoS Status:               " << qosStatus << std::endl;
    std::cout << "  Recommended Action:       " << recommendedAction;
    if (!recommendedTargetMrsu.empty()) std::cout << " -> " << recommendedTargetMrsu;
    std::cout << std::endl;
    std::cout << "  CBR Status:               " << cbrStatus << std::endl;
    std::cout << "===========================================\n" << std::endl;

    if (mCsvFile.is_open()) {
         mCsvFile << mScmNumber << "," << now << "," << activeMrsus << "," << reportingMrsus << "," << totalExpected << ","
                  << totalReceived << "," << segmentPDR << "," << segmentAvgDelay << ","
                  << segmentAvgVelocity << "," << segmentAvgCbr << "," << segmentAwareness << "," << connectedCount << ","
                  << totalCount << "," << coverage << "," << e_pdr << "," << e_delay << ","
                  << e_coverage << "," << gamma_pdr << "," << gamma_delay << "," << gamma_coverage << ","
                  << qosError << "," << qosStatus << "," << recommendedAction << ","
                  << recommendedTargetMrsu << "," << cbrStatus << "\n";
        mCsvFile.flush();
    }

    mLastScm.scmNumber = mScmNumber;
    mLastScm.timestamp = now;
    mLastScm.activeMrsus = activeMrsus;
    mLastScm.totalExpected = totalExpected;
    mLastScm.totalReceived = totalReceived;
    mLastScm.segmentPDR = segmentPDR;
    mLastScm.segmentAvgDelay = segmentAvgDelay;
    mLastScm.segmentAvgVelocity = segmentAvgVelocity;
    mLastScm.segmentAvgCbr = segmentAvgCbr;
	mLastScm.segmentAwareness = segmentAwareness;
    mLastScm.connectedVehicles = connectedCount;
    mLastScm.totalVehicles = totalCount;
    mLastScm.coverage = coverage;
    mLastScm.e_pdr = e_pdr;
    mLastScm.e_delay = e_delay;
    mLastScm.e_coverage = e_coverage;
    mLastScm.gamma_pdr = gamma_pdr;
    mLastScm.gamma_delay = gamma_delay;
    mLastScm.gamma_coverage = gamma_coverage;
    mLastScm.qosError = qosError;
    mLastScm.qosStatus = qosStatus;
    mLastScm.recommendedAction = recommendedAction;
    mLastScm.recommendedTargetMrsu = recommendedTargetMrsu;
    mLastScm.cbrStatus = cbrStatus;

    mLastAggregation = now;
}