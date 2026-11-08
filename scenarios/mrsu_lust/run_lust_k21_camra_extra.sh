#!/usr/bin/env bash
# LuST 60% V2X, 21 candidates, CAMRA only: runs 2-5 (seeds 6001..6004 / 91001..91004)
set -uo pipefail

SCEN="/home/kazi/artery/scenarios/mrsu_lust"
HDR="$SCEN/SegmentCoordinationServer.h"
CFG="$SCEN/mrsu_lust.sumo.cfg"
BUILD_DIR="/home/kazi/artery/build"
RESULTS_SRC_DIR="$SCEN/results"
DEST="/home/kazi/data/Simulation Data_Lust roi15_nvx60_k21/camra"
SEED_BASE=6000
SUMO_BASE=91000

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

cp "$HDR" "$HDR.pre_k21x_$(date +%Y%m%d_%H%M%S)"
cp "$CFG" "$CFG.pre_k21x_$(date +%Y%m%d_%H%M%S)"
sed -i 's/lust_roi15_buses\.rou\.xml/lust_roi15_buses_k21.rou.xml/' "$CFG"
if [[ "$(grep -c "lust_roi15_buses_k21.rou.xml" "$CFG")" != "1" ]]; then
    echo "ABORT: could not switch the config to the k21 bus file"; exit 1
fi
set_mode CAMRA || { echo "ABORT: could not set MODE=CAMRA"; exit 1; }
mkdir -p "$DEST"
echo "=== LuST 60% V2X, 21 candidates, CAMRA runs 2-5 ==="

for i in 1 2 3 4; do
    run=$((i+1)); SEED_SET=$((SEED_BASE+i)); SUMO_SEED=$((SUMO_BASE+i))
    run_dest="$DEST/run$run"
    log="/tmp/lust_k21_camra_run${run}.log"

    if [[ -f "$run_dest/scm_history_CAMRA.csv" ]]; then
        echo "[run$run] already exists, skipping"; continue
    fi
    echo "[run$run] seeds $SEED_SET / $SUMO_SEED, starting at $(date +%H:%M)"
    start_time=$(date +%s)

    mkdir -p "$RESULTS_SRC_DIR"
    rm -f "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca "$RESULTS_SRC_DIR"/sumo-0.log 2>/dev/null
    (
        cd "$BUILD_DIR"
        cmake . -DRUN_FLAGS="-u Cmdenv -c inet --seed-set=$SEED_SET --traci.launcher.seed=$SUMO_SEED --sim-time-limit=600s" > /dev/null
        cmake --build . --target run_mrsu_lust -j4
    ) > "$log" 2>&1
    status=$?
    elapsed=$(( $(date +%s) - start_time ))

    if [[ $status -ne 0 ]]; then run_dest="${run_dest}_FAILED"; fi   # failed runs never block a re-run
    mkdir -p "$run_dest"
    for f in "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca "$RESULTS_SRC_DIR"/sumo-0.log; do
        [[ -e "$f" ]] && cp "$f" "$run_dest/"
    done
    nv=$(grep -c ',V2X' "$run_dest/communication_summary.csv" 2>/dev/null || echo 0)
    nw=$(( $(wc -l < "$run_dest/scm_history_CAMRA.csv" 2>/dev/null || echo 1) - 1 ))
    if [[ $status -ne 0 ]]; then
        echo "[run$run] FAILED after ${elapsed}s, partial files in $run_dest, see $log"
    else
        echo "[run$run] done in ${elapsed}s -> $run_dest  (windows=$nw, V2X rows=$nv; expect 19 / ~443)"
    fi
    echo ""
done
echo "=== All done: $DEST ==="
