import re, shutil, sys
H = 'SegmentCoordinationServer.h'; C = 'SegmentCoordinationServer.cc'
h = open(H).read(); c = open(C).read()
def sub1(s, pat, fn, name):
    n = len(re.findall(pat, s))
    if n != 1:
        sys.exit('ABORT: anchor %s found %d times (expected 1) - nothing changed' % (name, n))
    return re.sub(pat, fn, s)

V2HDR = """// ---- CAMRA v2 (LuST experiment): simple activation rule + PDR-free error ----
        static constexpr bool   CAMRA_V2 = true;
        static constexpr int    BOOTSTRAP_N = 2;         // candidates switched on at the first SCM
        static constexpr double DELAY_TARGET_V2 = 0.10;  // seconds, used by the PDR-free error
"""
h = sub1(h, r'// QoS targets \(tunable\)', lambda m: V2HDR + '        // QoS targets (tunable)', 'H0')
h = sub1(h, r'static constexpr double E_COMFORTABLE = 0\.10;', lambda m: 'static constexpr double E_COMFORTABLE = CAMRA_V2 ? 0.02 : 0.10;', 'H1')
h = sub1(h, r'static constexpr double E_MONITOR = 0\.15;', lambda m: 'static constexpr double E_MONITOR = CAMRA_V2 ? 0.05 : 0.15;', 'H2')
h = sub1(h, r'static constexpr Strategy MODE = Strategy::[A-Z_]*;', lambda m: 'static constexpr Strategy MODE = Strategy::CAMRA;', 'MODE')

BLOCK = """

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
    };"""
c = sub1(c, r'double qosError = gamma_pdr \* e_pdr \+ gamma_delay \* e_delay \+ gamma_coverage \* e_coverage;', lambda m: m.group(0) + BLOCK, 'C1')

BOOT = """if (CAMRA_V2) {
            std::vector<std::string> first = pickBest(BOOTSTRAP_N);
            std::string joined;
            for (const auto& id : first) { mActiveState[id] = true; joined += (joined.empty() ? "" : "+") + id; }
            if (!first.empty()) {
                recommendedAction = "ACTIVATE_" + std::to_string(first.size()) + "_BOOTSTRAP";
                recommendedTargetMrsu = joined;
                std::cout << "[SCS BOOTSTRAP v2] Activating " << joined << " at t=" << now << std::endl;
            }
        }
        """
c = sub1(c, r'(qosStatus = "BOOTSTRAP";\s*)if \(!bestSleepingCandidate\.empty\(\) && bestSleepingSi > 0\) \{',
         lambda m: m.group(1) + BOOT + 'if (!CAMRA_V2 && !bestSleepingCandidate.empty() && bestSleepingSi > 0) {', 'C2')

RULE = """if (CAMRA_V2) {
            // simple rule: E > 0.05 -> +1 best candidate; E <= 0.02 -> sleep 1; otherwise hold
            if (qosStatus == "INCREASE_RESOURCES") {
                std::vector<std::string> add = pickBest(1);
                if (!add.empty()) {
                    mActiveState[add[0]] = true;
                    recommendedAction = "ACTIVATE_1";
                    recommendedTargetMrsu = add[0];
                }
                else {
                    recommendedAction = "NO_CANDIDATE_AVAILABLE";
                    std::cout << "[SCS v2] E=" << qosError << " but no candidate would add coverage at t=" << now << std::endl;
                }
            }
            else if (qosStatus == "COMFORTABLE" && !worstActiveCandidate.empty() && activeMrsus > 1) {
                mActiveState[worstActiveCandidate] = false;
                recommendedAction = "SLEEP_1";
                recommendedTargetMrsu = worstActiveCandidate;
            }
            mPreviousZone = qosStatus;
            mPreviousError = qosError;
            mConsecutiveCount = 1;
        }
        else if (qosStatus == "INCREASE_RESOURCES") {"""
c = sub1(c, r'if \(qosStatus == "INCREASE_RESOURCES"\) \{', lambda m: RULE, 'C3')

shutil.copy2(H, H + '.pre_camrav2_bak'); shutil.copy2(C, C + '.pre_camrav2_bak')
open(H, 'w').write(h); open(C, 'w').write(c)
print('PATCH APPLIED (backups: *.pre_camrav2_bak)')
