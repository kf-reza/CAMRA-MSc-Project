#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# run_bremen3_10runs_camra_em030_cc5.sh — runs bremen3 (CAMRA, E_MONITOR=0.30,
# activation hysteresis raised from 2 -> 5 consecutive rising MONITOR windows)
# 10 times, headless, using the SAME 10 seeds as every other CAMRA threshold
# batch in this ablation (0.10 / 0.15 / 0.18 / 0.22), so this is a paired
# comparison — only the threshold + hysteresis constant change, not the
# random traffic/network conditions.
#
# PREREQUISITE (do this manually before running):
#   1. SegmentCoordinationServer.h (mrsu_bremen3 scenario) — change:
#          static constexpr double E_MONITOR = 0.15;
#      to:
#          static constexpr double E_MONITOR = 0.30;
#
#   2. SegmentCoordinationServer.cc (mrsu_bremen3 scenario), around line 397
#      — change:
#          if (mConsecutiveCount >= 2) { // back to 2 consecutive rising MONITOR windows
#      to:
#          if (mConsecutiveCount >= 5) { // widened to 5 consecutive rising MONITOR windows
#
#      This is the ONLY place mConsecutiveCount's threshold is checked — it
#      gates the ACTIVATE_1 branch inside the
#      "qosStatus == mPreviousZone && qosStatus == MONITOR && errorIncreasing"
#      block. The SLEEP_1 branch (COMFORTABLE + errorDecreasing) is untouched
#      by this change.
#
#   3. Rebuild once by hand first (or just let this script's first `cmake
#      --build` do it) to confirm both edits compile before the batch starts.
#
# HOW TO RUN (from anywhere in WSL, after steps 1-2 above):
#   chmod +x run_bremen3_10runs_camra_em030_cc5.sh
#   ./run_bremen3_10runs_camra_em030_cc5.sh
# ---------------------------------------------------------------------------
set -uo pipefail

BUILD_DIR="/home/fahid/artery/build"
RESULTS_SRC_DIR="/home/fahid/artery/scenarios/mrsu_bremen3/results"
DEST_ROOT="/mnt/d/Uni_Bremen/E-Learning/Project/Simulation/batch results/CAMRA E_Monitor 30"
N_RUNS=10
SEED_SET_BASE=1000     # SAME as the 0.10 / 0.15 / 0.18 / 0.22 runs: 1000..1009
SUMO_SEED_BASE=50000   # SAME as the 0.10 / 0.15 / 0.18 / 0.22 runs: 50000..50009

mkdir -p "$DEST_ROOT"

echo "=== Starting 10 CAMRA runs of bremen3 (E_MONITOR=0.30, consecutive-count=5) ==="
echo "Results will land in: $DEST_ROOT"
echo ""
echo "!! Confirm both SegmentCoordinationServer.h (E_MONITOR=0.30) and"
echo "!! SegmentCoordinationServer.cc (mConsecutiveCount >= 5) edits are in"
echo "!! place before continuing -- this script does NOT edit source for you."
echo ""

for ((i = 1; i <= N_RUNS; i++)); do
    seed_set=$((SEED_SET_BASE + i - 1))
    sumo_seed=$((SUMO_SEED_BASE + i - 1))
    run_dest="$DEST_ROOT/run$i results"

    if [[ -d "$run_dest" ]] && compgen -G "$run_dest"/*.csv > /dev/null; then
        echo "[run $i/$N_RUNS] already has data — skipping."
        continue
    fi

    echo "[run $i/$N_RUNS] starting — seed-set=$seed_set, sumo-seed=$sumo_seed ..."
    start_time=$(date +%s)

    mkdir -p "$RESULTS_SRC_DIR"
    rm -f "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca 2>/dev/null

    (
        cd "$BUILD_DIR"
        cmake . -DRUN_FLAGS="-u Cmdenv -c inet --seed-set=$seed_set --traci.launcher.seed=$sumo_seed" > /dev/null
        cmake --build . --target run_mrsu_bremen3 -j4
    ) > "/tmp/bremen3_camra_em030_cc5_run${i}.log" 2>&1
    status=$?

    elapsed=$(( $(date +%s) - start_time ))

    if [[ $status -ne 0 ]]; then
        echo "[run $i/$N_RUNS] FAILED after ${elapsed}s — see /tmp/bremen3_camra_em030_cc5_run${i}.log for details. Moving on to the next run."
        continue
    fi

    mkdir -p "$run_dest"
    moved=0
    for f in "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca; do
        [[ -e "$f" ]] || continue
        cp "$f" "$run_dest/"
        moved=1
    done

    if [[ $moved -eq 0 ]]; then
        echo "[run $i/$N_RUNS] WARNING: finished but no result files found — check /tmp/bremen3_camra_em030_cc5_run${i}.log"
    else
        echo "seed_set=$seed_set sumo_seed=$sumo_seed elapsed_s=$elapsed e_monitor=0.30 consecutive_count_threshold=5" > "$run_dest/run_meta.txt"
        echo "[run $i/$N_RUNS] done in ${elapsed}s — saved to: $run_dest"
    fi
    echo ""
done

echo "=== All 10 runs complete. Check: $DEST_ROOT ==="
echo ""
echo "=== Reminder: revert SegmentCoordinationServer.h (E_MONITOR back to 0.15)"
echo "=== and SegmentCoordinationServer.cc (mConsecutiveCount back to >= 2)"
echo "=== before your next run on this scenario, unless you're continuing"
echo "=== this ablation line further. ==="
