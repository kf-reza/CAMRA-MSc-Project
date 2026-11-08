#!/usr/bin/env bash
# 60% V2X: Always Active, Fixed Single, Static Periodic - one run each (same seeds as CAMRA run 1)
set -uo pipefail

SCEN="/home/kazi/artery/scenarios/mrsu_lust"
HDR="$SCEN/SegmentCoordinationServer.h"
BUILD_DIR="/home/kazi/artery/build"
RESULTS_SRC_DIR="$SCEN/results"
DEST_ROOT="/home/kazi/data/Simulation Data_Lust roi15_nvx60_baselines"
SEED_SET=6000
SUMO_SEED=91000
STRATEGIES=(ALWAYS_ACTIVE FIXED_SINGLE STATIC_PERIODIC)
NAMES=(alwaysactive fixedsingle staticperiodic)

set_mode() {
    sed -i "s/static constexpr Strategy MODE = Strategy::[A-Z_]*;/static constexpr Strategy MODE = Strategy::$1;/" "$HDR"
    [[ "$(grep -c "static constexpr Strategy MODE = Strategy::$1;" "$HDR")" == "1" ]]
}

cleanup() {
    set_mode CAMRA && echo "[cleanup] MODE restored to CAMRA" || echo "[cleanup] WARNING: could not restore MODE, check the header!"
}
trap cleanup EXIT

cp "$HDR" "$HDR.pre_baselines_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$DEST_ROOT"
echo "=== 60% V2X baselines, seeds $SEED_SET / $SUMO_SEED ==="

for idx in 0 1 2; do
    strat="${STRATEGIES[$idx]}"
    name="${NAMES[$idx]}"
    run_dest="$DEST_ROOT/$name/run1"
    log="/tmp/lust_nvx60_${name}.log"

    if ! set_mode "$strat"; then
        echo "[$name] ABORT: could not set MODE=$strat in the header"
        exit 1
    fi
    echo "[$name] MODE set to $strat, starting at $(date +%H:%M)"
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

    if [[ $status -ne 0 ]]; then
        echo "[$name] FAILED after ${elapsed}s, see $log. Moving on."
        continue
    fi

    mkdir -p "$run_dest"
    moved=0
    for f in "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca "$RESULTS_SRC_DIR"/sumo-0.log; do
        [[ -e "$f" ]] || continue
        cp "$f" "$run_dest/"
        moved=1
    done
    if [[ $moved -eq 0 ]]; then
        echo "[$name] WARNING: finished but no result files, check $log"
    else
        echo "[$name] done in ${elapsed}s, saved to: $run_dest"
    fi
    echo ""
done
echo "=== All baselines done: $DEST_ROOT ==="
