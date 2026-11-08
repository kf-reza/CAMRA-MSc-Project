#include "SegmentCoordinationServer.h"
#include <iostream>
#include <algorithm>
#include <tuple>
#include <cmath>

SegmentCoordinationServerLuST::SegmentCoordinationServerLuST()
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
        mCallForHelpFile << "scm_number,time,active_mrsus,qos_error,dominant_deficit,e_pdr,e_delay,e_coverage,unserved_vehicles,priority1_x,priority1_y,priority1_distance_m,priority2_x,priority2_y,priority2_distance_m,priority3_x,priority3_y,priority3_distance_m,message\n";
        mCallForHelpFile.flush();
    }
    else {
        std::cout << "[SCS WARNING] Could not open results/call_for_help.csv for writing" << std::endl;
    }
}

SegmentCoordinationServerLuST::~SegmentCoordinationServerLuST()
{
    if (mCsvFile.is_open()) {
        mCsvFile.close();
    }
    if (mCallForHelpFile.is_open()) {
        mCallForHelpFile.close();
    }
}

SegmentCoordinationServerLuST& SegmentCoordinationServerLuST::getInstance()
{
    static SegmentCoordinationServerLuST instance;
    return instance;
}

const SegmentCoordinationMessage& SegmentCoordinationServerLuST::getLatestSCM() const
{
    return mLastScm;
}

bool SegmentCoordinationServerLuST::isActive(const std::string& mrsuId) const
{
    auto it = mActiveState.find(mrsuId);
    return (it != mActiveState.end()) ? it->second : false;
}

void SegmentCoordinationServerLuST::reportDeparture(const std::string& mrsuId, double secondsUntilDeparture)
{
    MrsuReport& report = mLatestReports[mrsuId];
    report.secondsUntilDeparture = secondsUntilDeparture;
    report.departureReportedAt = -1.0; // stamped with the SSM time in reportSSM()
}

void SegmentCoordinationServerLuST::reportSSM(const std::string& mrsuId, long expected, long received,
                                           double windowPDR, double avgDelay, double avgVelocity, double avgCbr,
                                           double avgAwareness,
                                           const std::vector<std::string>& connectedVehicleIds,
                                           const std::vector<std::string>& allVehicleIds,
                                           double hotspotX, double hotspotY, int hotspotCount,
                                           double selfX, double selfY, double now)
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
    report.hotspotX = hotspotX;
    report.hotspotY = hotspotY;
    report.hotspotCount = hotspotCount;
    report.selfX = selfX;
    report.selfY = selfY;
    report.lastUpdated = now;

	if (report.departureReportedAt < 0) report.departureReportedAt = now;

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

void SegmentCoordinationServerLuST::aggregateAndPublish(double now)
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

    // ---- CAMRA v2 ----------------------------------------------------------------
    if (CAMRA_V2 && MODE == Strategy::CAMRA) {
        // PDR-free error: delay + coverage only (PDR is still logged, weight 0)
        e_delay = std::max(0.0, (segmentAvgDelay - DELAY_TARGET_V2) / DELAY_TARGET_V2);
        double sumV2 = (e_delay + EPSILON) + (e_coverage + EPSILON);
        gamma_pdr = 0.0;
        gamma_delay = (e_delay + EPSILON) / sumV2;
        gamma_coverage = (e_coverage + EPSILON) / sumV2;
        qosError = gamma_delay * e_delay + gamma_coverage * e_coverage;
    }
    // pick the n sleeping candidates that would newly cover the most unserved vehicles
    auto pickBest = [&](int n) {
        std::vector<std::string> picked;
        std::set<std::string> covered = unionConnectedVehicles;
        for (int k = 0; k < n; k++) {
            std::string best; long bestGain = 0; long bestSi = -1;
            for (const auto& kv : mLatestReports) {
                const MrsuReport& r = kv.second;
                if ((now - r.lastUpdated) > SCM_INTERVAL) continue;
                if (isActive(kv.first)) continue;
                if (std::find(picked.begin(), picked.end(), kv.first) != picked.end()) continue;
                long gain = 0;
                for (const auto& v : r.connectedVehicleIds) if (!covered.count(v)) gain++;
                long si = static_cast<long>(r.connectedVehicleIds.size());
                if (gain > bestGain || (gain == bestGain && gain > 0 && si > bestSi)) { best = kv.first; bestGain = gain; bestSi = si; }
            }
            if (best.empty()) break;
            picked.push_back(best);
            for (const auto& v : mLatestReports.at(best).connectedVehicleIds) covered.insert(v);
        }
        return picked;
    };

    std::string qosStatus;
    std::string recommendedAction = "NONE";
    std::string recommendedTargetMrsu;

    if (MODE == Strategy::ALWAYS_ACTIVE) {
        // baseline: no decisions, just compute and record E for comparison
        // same PDR-free error as CAMRA v2: delay + coverage only (PDR is still logged, weight 0)
        e_pdr = std::max(0.0, (PDR_TARGET - segmentPDR) / PDR_TARGET);
        e_delay = std::max(0.0, (segmentAvgDelay - DELAY_TARGET_V2) / DELAY_TARGET_V2);
        e_coverage = std::max(0.0, (COVERAGE_TARGET - coverage) / COVERAGE_TARGET);
        double sumV2 = (e_delay + EPSILON) + (e_coverage + EPSILON);
        gamma_pdr = 0.0;
        gamma_delay = (e_delay + EPSILON) / sumV2;
        gamma_coverage = (e_coverage + EPSILON) / sumV2;
        qosError = gamma_delay * e_delay + gamma_coverage * e_coverage;
        if (qosError <= E_COMFORTABLE) qosStatus = "COMFORTABLE";
        else if (qosError <= E_MONITOR) qosStatus = "MONITOR";
        else qosStatus = "INCREASE_RESOURCES";
        recommendedAction = "BASELINE_NO_DECISION";
    }

	else if (MODE == Strategy::STATIC_PERIODIC) {
        // first-come-first-served: whoever arrived first stays active until it leaves
        // same PDR-free error as CAMRA v2: delay + coverage only (PDR is still logged, weight 0)
        e_pdr = std::max(0.0, (PDR_TARGET - segmentPDR) / PDR_TARGET);
        e_delay = std::max(0.0, (segmentAvgDelay - DELAY_TARGET_V2) / DELAY_TARGET_V2);
        e_coverage = std::max(0.0, (COVERAGE_TARGET - coverage) / COVERAGE_TARGET);
        double sumV2 = (e_delay + EPSILON) + (e_coverage + EPSILON);
        gamma_pdr = 0.0;
        gamma_delay = (e_delay + EPSILON) / sumV2;
        gamma_coverage = (e_coverage + EPSILON) / sumV2;
        qosError = gamma_delay * e_delay + gamma_coverage * e_coverage;
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
        // same PDR-free error as CAMRA v2: delay + coverage only (PDR is still logged, weight 0)
        e_pdr = std::max(0.0, (PDR_TARGET - segmentPDR) / PDR_TARGET);
        e_delay = std::max(0.0, (segmentAvgDelay - DELAY_TARGET_V2) / DELAY_TARGET_V2);
        e_coverage = std::max(0.0, (COVERAGE_TARGET - coverage) / COVERAGE_TARGET);
        double sumV2 = (e_delay + EPSILON) + (e_coverage + EPSILON);
        gamma_pdr = 0.0;
        gamma_delay = (e_delay + EPSILON) / sumV2;
        gamma_coverage = (e_coverage + EPSILON) / sumV2;
        qosError = gamma_delay * e_delay + gamma_coverage * e_coverage;
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
        if (CAMRA_V2) {
            std::vector<std::string> first = pickBest(BOOTSTRAP_N);
            std::string joined;
            for (const auto& id : first) { mActiveState[id] = true; joined += (joined.empty() ? "" : "+") + id; }
            if (!first.empty()) {
                recommendedAction = "ACTIVATE_" + std::to_string(first.size()) + "_BOOTSTRAP";
                recommendedTargetMrsu = joined;
                std::cout << "[SCS BOOTSTRAP v2] Activating " << joined << " at t=" << now << std::endl;
            }
        }
        if (!CAMRA_V2 && !bestSleepingCandidate.empty() && bestSleepingSi > 0) {
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

                if (CAMRA_V2) {
            // simple rule: E > 0.05 -> +1 best candidate; E <= 0.02 -> sleep 1; otherwise hold
            if (qosStatus == "INCREASE_RESOURCES") {
                std::vector<std::string> add = pickBest(qosError > E_BURST ? 2 : 1);
                if (!add.empty()) {
                    std::string joined;
                    for (const auto& id : add) { mActiveState[id] = true; joined += (joined.empty() ? "" : "+") + id; }
                    recommendedAction = "ACTIVATE_" + std::to_string(add.size());
                    recommendedTargetMrsu = joined;
                }
                else {
                    recommendedAction = "NO_CANDIDATE_AVAILABLE";
                    std::cout << "[SCS v2] E=" << qosError << " but no candidate would add coverage at t=" << now << std::endl;
                }
            }
            else if (qosStatus == "COMFORTABLE" && activeMrsus > 1) {
                // safe sleep: pick the active MRSU whose removal loses the least coverage and
                // only sleep it if coverage stays >= SAFE_SLEEP_COVERAGE without it
                std::string sleepId; double bestRemaining = -1.0;
                for (const auto& kv : mLatestReports) {
                    if ((now - kv.second.lastUpdated) > SCM_INTERVAL) continue;
                    if (!isActive(kv.first)) continue;
                    std::set<std::string> others;
                    for (const auto& kv2 : mLatestReports) {
                        if (kv2.first == kv.first) continue;
                        if ((now - kv2.second.lastUpdated) > SCM_INTERVAL) continue;
                        if (!isActive(kv2.first)) continue;
                        for (const auto& v : kv2.second.connectedVehicleIds) others.insert(v);
                    }
                    double remaining = (totalCount > 0) ? static_cast<double>(others.size()) / totalCount : 0.0;
                    if (remaining > bestRemaining) { bestRemaining = remaining; sleepId = kv.first; }
                }
                if (!sleepId.empty() && bestRemaining >= SAFE_SLEEP_COVERAGE) {
                    mActiveState[sleepId] = false;
                    recommendedAction = "SLEEP_1";
                    recommendedTargetMrsu = sleepId;
                }
            }
            mPreviousZone = qosStatus;
            mPreviousError = qosError;
            mConsecutiveCount = 1;
        }
        else if (qosStatus == "INCREASE_RESOURCES") {
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

                // Rank sleeping candidates' local hotspots by vehicle count, take top 3
                std::vector<std::tuple<int, double, double>> hotspots; // count, x, y
                // Coverage-gap inference: find the spots geographically farthest from
                // any currently active MRSU. This needs no CAM reception of the stranded
                // vehicles themselves -- it directly answers "where is nobody covering?"
                // which is exactly where unmet demand is most likely to be.
                std::vector<std::pair<double, double>> activePositions;
                for (const auto& kv : mLatestReports) {
                    bool isCurrentlyActive = mActiveState.count(kv.first) && mActiveState.at(kv.first);
                    if (isCurrentlyActive) {
                        activePositions.push_back({kv.second.selfX, kv.second.selfY});
                    }
                }
                if (!activePositions.empty()) {
                    const double GRID_CELL = 500.0;
                    double minX = activePositions[0].first, maxX = activePositions[0].first;
                    double minY = activePositions[0].second, maxY = activePositions[0].second;
                    for (const auto& kv : mLatestReports) {
                        minX = std::min(minX, kv.second.selfX); maxX = std::max(maxX, kv.second.selfX);
                        minY = std::min(minY, kv.second.selfY); maxY = std::max(maxY, kv.second.selfY);
                    }
                    std::vector<std::tuple<double, double, double>> gapCandidates; // distance, x, y
                    for (double gx = minX; gx <= maxX; gx += GRID_CELL) {
                        for (double gy = minY; gy <= maxY; gy += GRID_CELL) {
                            double nearestDist = 1e18;
                            for (const auto& ap : activePositions) {
                                double dx = gx - ap.first, dy = gy - ap.second;
                                double dist = std::sqrt(dx * dx + dy * dy);
                                nearestDist = std::min(nearestDist, dist);
                            }
                            gapCandidates.push_back({nearestDist, gx, gy});
                        }
                    }
                    std::sort(gapCandidates.begin(), gapCandidates.end(),
                              [](const auto& a, const auto& b) { return std::get<0>(a) > std::get<0>(b); });
                    int rank = 0;
                    for (const auto& gc : gapCandidates) {
                        if (rank >= 3) break;
                        hotspots.push_back({static_cast<int>(std::get<0>(gc)), std::get<1>(gc), std::get<2>(gc)});
                        rank++;
                    }
                }
                std::sort(hotspots.begin(), hotspots.end(),
                          [](const auto& a, const auto& b) { return std::get<0>(a) > std::get<0>(b); });
                while (hotspots.size() < 3) {
                    hotspots.push_back({0, 0.0, 0.0});
                }

                if (mCallForHelpFile.is_open()) {
                    mCallForHelpFile << mScmNumber << "," << now << "," << activeMrsus << ","
                                      << qosError << "," << dominantDeficit << ","
                                      << e_pdr << "," << e_delay << "," << e_coverage << ","
                                      << unservedVehicles << ","
                                      << std::get<1>(hotspots[0]) << "," << std::get<2>(hotspots[0]) << "," << std::get<0>(hotspots[0]) << ","
                                      << std::get<1>(hotspots[1]) << "," << std::get<2>(hotspots[1]) << "," << std::get<0>(hotspots[1]) << ","
                                      << std::get<1>(hotspots[2]) << "," << std::get<2>(hotspots[2]) << "," << std::get<0>(hotspots[2]) << ","
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
                mConsecutiveCount++;
                if (mConsecutiveCount >= 2) { // back to 2 consecutive rising MONITOR windows
                    if (!bestSleepingCandidate.empty()) {
                        mActiveState[bestSleepingCandidate] = true;
                        recommendedAction = "ACTIVATE_1";
                        recommendedTargetMrsu = bestSleepingCandidate;
                    }
                    mConsecutiveCount = 0;
                }
            }
            else if (qosStatus == "COMFORTABLE" && errorDecreasing && qosError < E_SLEEP_THRESHOLD) {
                // only actually sleep if error is BOTH falling AND genuinely low (stricter than the classification bar)
                if (!worstActiveCandidate.empty() && activeMrsus > 1) {
                    // never sleep the last remaining active MRSU — avoids the mid-run zero-active blackout
                    mActiveState[worstActiveCandidate] = false;
                    recommendedAction = "SLEEP_1";
                    recommendedTargetMrsu = worstActiveCandidate;
                }
            }
            else {
                mConsecutiveCount = 0;
            }

            mPreviousZone = qosStatus;
            mPreviousError = qosError;
        }
        else {
            mPreviousZone = qosStatus;
            mPreviousError = qosError;
            mConsecutiveCount = 1;
        }
    }

    // ---- Proactive blackout mitigation (CAMRA only) ---------------------------------
    // Blackout mechanism: the only active MRSU(s) drive out of the scenario between two
    // SCM decisions, leaving 0 active MRSUs (and 0 coverage) until the next SCM reacts.
    // Fix: if no active MRSU is "safe" (all leave within DEPARTURE_LOOKAHEAD), activate the
    // sleeping candidate with the best neighbour count S_i that is itself not about to leave.
    if (ENABLE_PROACTIVE_BACKUP && MODE == Strategy::CAMRA && activeMrsus > 0) {
        auto remaining = [&](const MrsuReport& r) {
            return r.secondsUntilDeparture - (now - r.departureReportedAt);
        };
        int safeActive = 0;
        std::string backup;
        long backupSi = -1;
        for (const auto& entry : mLatestReports) {
            const MrsuReport& r = entry.second;
            if ((now - r.lastUpdated) > SCM_INTERVAL) continue;      // not present any more
            bool leavingSoon = remaining(r) <= DEPARTURE_LOOKAHEAD;
            if (isActive(entry.first)) {
                if (!leavingSoon) safeActive++;
            }
            else if (!leavingSoon) {
                long si = static_cast<long>(r.connectedVehicleIds.size());
                if (si > backupSi) { backupSi = si; backup = entry.first; }
            }
        }
        if (safeActive == 0) {
            if (!backup.empty()) {
                mActiveState[backup] = true;
                mBackupActivations++;
                recommendedAction = "ACTIVATE_BACKUP";
                recommendedTargetMrsu = backup;
                std::cout << "[SCS PROACTIVE BACKUP] all active MRSUs leave within "
                           << DEPARTURE_LOOKAHEAD << " s - activating " << backup
                           << " (S_i=" << backupSi << ") at t=" << now
                           << " (backup activation #" << mBackupActivations << ")" << std::endl;
            }
            else {
                std::cout << "[SCS PROACTIVE BACKUP] all active MRSUs leave soon but no stable "
                              "sleeping candidate is available at t=" << now << std::endl;
            }
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
