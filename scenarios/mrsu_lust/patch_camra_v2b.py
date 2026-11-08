import re, shutil, sys
H = 'SegmentCoordinationServer.h'; C = 'SegmentCoordinationServer.cc'
h = open(H).read(); c = open(C).read()
def sub1(s, pat, fn, name):
    n = len(re.findall(pat, s))
    if n != 1:
        sys.exit('ABORT: anchor %s found %d times (expected 1) - nothing changed' % (name, n))
    return re.sub(pat, fn, s)

# constants
h = sub1(h, r'static constexpr double DELAY_TARGET_V2 = 0\.10;[^\n]*\n',
         lambda m: m.group(0) + """        static constexpr double E_BURST = 0.50;          // E above this -> activate 2 instead of 1
        static constexpr double SAFE_SLEEP_COVERAGE = 0.88; // sleep only if coverage stays >= this without the MRSU
""", 'H1')

# fix 1: burst of 2 when E > E_BURST
ACT_PAT = r'std::vector<std::string> add = pickBest\(1\);\s*if \(!add\.empty\(\)\) \{\s*mActiveState\[add\[0\]\] = true;\s*recommendedAction = "ACTIVATE_1";\s*recommendedTargetMrsu = add\[0\];\s*\}'
ACT_NEW = """std::vector<std::string> add = pickBest(qosError > E_BURST ? 2 : 1);
                if (!add.empty()) {
                    std::string joined;
                    for (const auto& id : add) { mActiveState[id] = true; joined += (joined.empty() ? "" : "+") + id; }
                    recommendedAction = "ACTIVATE_" + std::to_string(add.size());
                    recommendedTargetMrsu = joined;
                }"""
c = sub1(c, ACT_PAT, lambda m: ACT_NEW, 'C1')

# fix 2: safe sleep
SL_PAT = r'else if \(qosStatus == "COMFORTABLE" && !worstActiveCandidate\.empty\(\) && activeMrsus > 1\) \{\s*mActiveState\[worstActiveCandidate\] = false;\s*recommendedAction = "SLEEP_1";\s*recommendedTargetMrsu = worstActiveCandidate;\s*\}'
SL_NEW = """else if (qosStatus == "COMFORTABLE" && activeMrsus > 1) {
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
            }"""
c = sub1(c, SL_PAT, lambda m: SL_NEW, 'C2')

shutil.copy2(H, H + '.pre_camrav2b_bak'); shutil.copy2(C, C + '.pre_camrav2b_bak')
open(H, 'w').write(h); open(C, 'w').write(c)
print('PATCH B APPLIED (backups: *.pre_camrav2b_bak)')
