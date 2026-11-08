#!/usr/bin/env bash
# LuST 60% V2X, reduced candidate pool (21 buses = Bremen3 density): 4 strategies, 1 run each, seeds 6000/91000
set -uo pipefail

SCEN="/home/kazi/artery/scenarios/mrsu_lust"
HDR="$SCEN/SegmentCoordinationServer.h"
CFG="$SCEN/mrsu_lust.sumo.cfg"
BUILD_DIR="/home/kazi/artery/build"
RESULTS_SRC_DIR="$SCEN/results"
DEST_ROOT="/home/kazi/data/Simulation Data_Lust roi15_nvx60_k21"
SEED_SET=6000
SUMO_SEED=91000
STRATEGIES=(FIXED_SINGLE CAMRA STATIC_PERIODIC ALWAYS_ACTIVE)
NAMES=(fixedsingle camra staticperiodic alwaysactive)

set_mode() {
    sed -i "s/static constexpr Strategy MODE = Strategy::[A-Z_]*;/static constexpr Strategy MODE = Strategy::$1;/" "$HDR"
    [[ "$(grep -c "static constexpr Strategy MODE = Strategy::$1;" "$HDR")" == "1" ]]
}

cleanup() {
    set_mode CAMRA && echo "[cleanup] MODE restored to CAMRA" || echo "[cleanup] WARNING: could not restore MODE, check the header!"
    sed -i 's/lust_roi15_buses_k21\.rou\.xml/lust_roi15_buses.rou.xml/' "$CFG"
    grep -q "lust_roi15_buses.rou.xml" "$CFG" && echo "[cleanup] route file restored" || echo "[cleanup] WARNING: check $CFG"
}
trap cleanup EXIT

cp "$HDR" "$HDR.pre_k21_$(date +%Y%m%d_%H%M%S)"
cp "$CFG" "$CFG.pre_k21_$(date +%Y%m%d_%H%M%S)"
sed -i 's/lust_roi15_buses\.rou\.xml/lust_roi15_buses_k21.rou.xml/' "$CFG"
if [[ "$(grep -c "lust_roi15_buses_k21.rou.xml" "$CFG")" != "1" ]]; then
    echo "ABORT: could not switch the config to the k21 bus file"
    exit 1
fi
mkdir -p "$DEST_ROOT"
echo "=== LuST 60% V2X, 21 candidates, seeds $SEED_SET / $SUMO_SEED ==="

for idx in 0 1 2 3; do
    strat="${STRATEGIES[$idx]}"
    name="${NAMES[$idx]}"
    run_dest="$DEST_ROOT/$name/run1"
    log="/tmp/lust_k21_${name}.log"

    if ! set_mode "$strat"; then
        echo "[$name] ABORT: could not set MODE=$strat"
        exit 1
    fi
    echo "[$name] MODE=$strat, starting at $(date +%H:%M)"
    start_time=$(date +%s)

    mkdir -p "$RESULTS_SRC_DIR"
    rm -f "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca 2>/dev/null
    (
        cd "$BUILD_DIR"
        cmake . -DRUN_FLAGS="-u Cmdenv -c inet --seed-set=$SEED_SET --traci.launcher.seed=$SUMO_SEED --sim-time-limit=600s" > /dev/null
        cmake --build . --target run_mrsu_lust -j4
    ) > "$log" 2>&1
    status=$?
    elapsed=$(( $(date +%s) - start_time ))

    mkdir -p "$run_dest"
    for f in "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca "$RESULTS_SRC_DIR"/sumo-0.log; do
        [[ -e "$f" ]] && cp "$f" "$run_dest/"
    done
    if [[ $status -ne 0 ]]; then
        echo "[$name] FAILED after ${elapsed}s (partial files kept in $run_dest), see $log. Moving on."
    else
        echo "[$name] done in ${elapsed}s, saved to: $run_dest"
    fi
    echo ""
done
echo "=== All done: $DEST_ROOT ==="
