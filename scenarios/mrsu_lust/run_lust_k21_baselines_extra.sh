#!/usr/bin/env bash
# LuST 60% V2X, 21 candidates, BASELINES only: runs 2-5 (seeds 6001..6004 / 91001..91004)
set -uo pipefail

SCEN="/home/kazi/artery/scenarios/mrsu_lust"
HDR="$SCEN/SegmentCoordinationServer.h"
CFG="$SCEN/mrsu_lust.sumo.cfg"
BUILD_DIR="/home/kazi/artery/build"
RESULTS_SRC_DIR="$SCEN/results"
DEST_ROOT="/home/kazi/data/Simulation Data_Lust roi15_nvx60_k21"
SEED_BASE=6000
SUMO_BASE=91000
STRATEGIES=(FIXED_SINGLE STATIC_PERIODIC ALWAYS_ACTIVE)
NAMES=(fixedsingle staticperiodic alwaysactive)
SCM_FILES=(scm_history_baseline_fixedsingle.csv scm_history_baseline_staticperiodic.csv scm_history_baseline_allactive.csv)

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

cp "$HDR" "$HDR.pre_k21b_$(date +%Y%m%d_%H%M%S)"
cp "$CFG" "$CFG.pre_k21b_$(date +%Y%m%d_%H%M%S)"
sed -i 's/lust_roi15_buses\.rou\.xml/lust_roi15_buses_k21.rou.xml/' "$CFG"
if [[ "$(grep -c "lust_roi15_buses_k21.rou.xml" "$CFG")" != "1" ]]; then
    echo "ABORT: could not switch the config to the k21 bus file"; exit 1
fi
echo "=== LuST 60% V2X, 21 candidates, baselines runs 2-5, started $(date '+%F %T') ==="

for i in 1 2 3 4; do                      # run2..run5 (outer loop = run, so counts stay balanced)
  run=$((i+1)); SEED_SET=$((SEED_BASE+i)); SUMO_SEED=$((SUMO_BASE+i))
  for idx in 0 1 2; do
    strat="${STRATEGIES[$idx]}"; name="${NAMES[$idx]}"; scm="${SCM_FILES[$idx]}"
    ok_dest="$DEST_ROOT/$name/run$run"
    log="/tmp/lust_k21_${name}_run${run}.log"

    if [[ -f "$ok_dest/$scm" ]]; then echo "[$name run$run] already done, skipping"; continue; fi
    if ! set_mode "$strat"; then echo "[$name run$run] ABORT: could not set MODE=$strat"; exit 1; fi
    echo "[$name run$run] MODE=$strat seeds $SEED_SET/$SUMO_SEED, start $(date '+%F %T')"
    t0=$(date +%s)

    mkdir -p "$RESULTS_SRC_DIR"
    rm -f "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca "$RESULTS_SRC_DIR"/sumo-0.log 2>/dev/null
    (
        cd "$BUILD_DIR"
        cmake . -DRUN_FLAGS="-u Cmdenv -c inet --seed-set=$SEED_SET --traci.launcher.seed=$SUMO_SEED --sim-time-limit=600s" > /dev/null
        cmake --build . --target run_mrsu_lust -j4
    ) > "$log" 2>&1
    status=$?
    el=$(( $(date +%s) - t0 ))

    # save, then judge completeness (19 windows, ~443 V2X rows expected)
    tmp="$DEST_ROOT/$name/_tmp_run$run"; rm -rf "$tmp"; mkdir -p "$tmp"
    for f in "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca "$RESULTS_SRC_DIR"/sumo-0.log; do
        [[ -e "$f" ]] && cp "$f" "$tmp/"
    done
    nw=$(( $(wc -l < "$tmp/$scm" 2>/dev/null || echo 1) - 1 ))
    nv=$(grep -c ',V2X' "$tmp/communication_summary.csv" 2>/dev/null || echo 0)
    if [[ $status -eq 0 && $nw -ge 19 && $nv -ge 400 ]]; then
        rm -rf "$ok_dest"; mv "$tmp" "$ok_dest"
        echo "[$name run$run] OK in ${el}s -> $ok_dest (windows=$nw, V2X rows=$nv)"
    else
        bad="$DEST_ROOT/$name/run${run}_CHECK"; rm -rf "$bad"; mv "$tmp" "$bad"
        echo "[$name run$run] CHECK: exit=$status windows=$nw V2X rows=$nv (expected 19 / ~443), saved to $bad, see $log"
    fi
    echo ""
  done
done
echo "=== All done $(date '+%F %T'): $DEST_ROOT ==="
