#ifndef SEGMENTCOORDINATIONSERVER_H_
#define SEGMENTCOORDINATIONSERVER_H_

#include <map>
#include <set>
#include <string>
#include <vector>
#include <fstream>

struct SegmentCoordinationMessage
{
    int scmNumber = 0;
    double timestamp = 0;
    int activeMrsus = 0;
    long totalExpected = 0;
    long totalReceived = 0;
    double segmentPDR = 0;
    double segmentAvgDelay = 0;
    double segmentAvgVelocity = 0;
    double segmentAvgCbr = 0;
	double segmentAwareness = 0;
    long connectedVehicles = 0;
    long totalVehicles = 0;
    double coverage = 0;

    // QoS feedback loop outputs
    double e_pdr = 0;
    double e_delay = 0;
    double e_coverage = 0;
    double gamma_pdr = 0;
    double gamma_delay = 0;
    double gamma_coverage = 0;
    double qosError = 0;
    std::string qosStatus; // "COMFORTABLE" / "MONITOR" / "INCREASE_RESOURCES"
    std::string recommendedAction; // "NONE" / "ACTIVATE_1" / "SLEEP_1"
    std::string recommendedTargetMrsu; // specific MRSU ID chosen for the action, or ""
    std::string cbrStatus; // "UNDER_UTILIZED" / "NORMAL" / "CONGESTED" (informational only)
};

class SegmentCoordinationServer3
{
    public:
        static SegmentCoordinationServer3& getInstance();

        // NOTE: signature extended (vs. the pre-hotspot bremen3 version) with
        // hotspotX/hotspotY/hotspotCount (this candidate's own locally-observed
        // demand hotspot, computed by the caller from vehicles heard via CAM this
        // window -- captured into MrsuReport but not currently read by the
        // coverage-gap call-for-help logic below, which instead infers demand from
        // the gap between active-MRSU positions) and selfX/selfY (this MRSU's own
        // current position, REQUIRED by that coverage-gap search). Every call site
        // must be updated to pass all five.
            void reportSSM(const std::string& mrsuId, long expected, long received,
                       double windowPDR, double avgDelay, double avgVelocity, double avgCbr,
                       double avgAwareness,
                       const std::vector<std::string>& connectedVehicleIds,
                       const std::vector<std::string>& allVehicleIds,
                       double hotspotX, double hotspotY, int hotspotCount,
                       double selfX, double selfY, double now);

        const SegmentCoordinationMessage& getLatestSCM() const;
        bool isActive(const std::string& mrsuId) const;

    private:
        SegmentCoordinationServer3();
        ~SegmentCoordinationServer3();

        struct MrsuReport {
            long expected = 0;
            long received = 0;
            double windowPDR = 0;
            double avgDelay = 0;
            double avgVelocity = 0;
            double avgCbr = 0;
            double avgAwareness = 0;
            std::vector<std::string> connectedVehicleIds;
            std::vector<std::string> allVehicleIds;
            double hotspotX = 0;
            double hotspotY = 0;
            int hotspotCount = 0;
            double selfX = 0;
            double selfY = 0;
            double lastUpdated = 0;
        };

        std::map<std::string, MrsuReport> mLatestReports;
        std::map<std::string, bool> mActiveState; // true = active, false = sleeping (default)
		std::vector<std::string> mArrivalOrder;   // order candidates first reported, for STATIC_PERIODIC FCFS

        double mLastAggregation = 0;
        static constexpr double SCM_INTERVAL = 30.0;
        int mScmNumber = 0;

        // QoS targets (tunable)
        static constexpr double PDR_TARGET = 0.99;
        static constexpr double DELAY_TARGET = 0.05;      // seconds (50ms)
        static constexpr double COVERAGE_TARGET = 0.9;
        static constexpr double EPSILON = 0.01;             // avoids divide-by-zero in weighting

        // QoS classification thresholds
        static constexpr double E_COMFORTABLE = 0.10;   // classification ceiling for "comfortable"
        static constexpr double E_MONITOR = 0.15;        // classification ceiling for "monitor"
        static constexpr double E_SLEEP_THRESHOLD = 0.05; // stricter: only actually sleep below this, even within COMFORTABLE

		// simulation strategy mode: change this one line to switch between comparison runs
        enum class Strategy { CAMRA, ALWAYS_ACTIVE, FIXED_SINGLE, STATIC_PERIODIC };
        static constexpr Strategy MODE = Strategy::CAMRA;
		// STATIC_PERIODIC: first-come-first-served — the first candidate to ever report stays
        // active until it leaves the simulation, then the next-arrived candidate takes over.
        // No neighbour-count comparison, no QoS reaction — purely arrival-order driven.
        // hysteresis state: track consecutive SCM cycles spent in the same zone
        std::string mPreviousZone = "";
        int mConsecutiveCount = 0;
		double mPreviousError = 0;

        SegmentCoordinationMessage mLastScm;

        std::ofstream mCsvFile;
		std::ofstream mCallForHelpFile;

        void aggregateAndPublish(double now);
};

#endif /* SEGMENTCOORDINATIONSERVER_H_ */
