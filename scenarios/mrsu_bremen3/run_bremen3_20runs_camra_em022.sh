#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# run_bremen3_20runs_camra_em022.sh — runs bremen3 (CAMRA, E_MONITOR=0.22)
# 20 times, headless, using the SAME seed convention as your 0.10/0.15/0.18
# batches (seed-set 1000..1019, sumo-seed 50000..50019), so this is a
# paired comparison — only the threshold changes.
#
# Safety: aborts immediately if the header doesn't actually say E_MONITOR=0.22,
# and verifies each run's seed against its saved folder as it goes.
#
# HOW TO RUN:
#   chmod +x run_bremen3_20runs_camra_em022.sh
#   ./run_bremen3_20runs_camra_em022.sh
#
# It runs unattended end-to-end. To let it keep going after you close the
# terminal / disconnect, launch it under nohup or tmux instead, e.g.:
#   nohup ./run_bremen3_20runs_camra_em022.sh > run_em022.log 2>&1 &
# ---------------------------------------------------------------------------
set -uo pipefail

BUILD_DIR="/home/fahid/artery/build"
HEADER_FILE="/home/fahid/artery/scenarios/mrsu_bremen3/SegmentCoordinationServer.h"
RESULTS_SRC_DIR="/home/fahid/artery/scenarios/mrsu_bremen3/results"
DEST_ROOT="/mnt/d/Uni_Bremen/E-Learning/Project/Simulation/batch results/CAMRA E_Monitor 22"
N_RUNS=20
SEED_SET_BASE=1000     # SAME as the E_MONITOR=0.10/0.15/0.18 runs: 1000..1019
SUMO_SEED_BASE=50000   # SAME as the E_MONITOR=0.10/0.15/0.18 runs: 50000..50019
EXPECTED_THRESHOLD="0.22"

# --- Guard: make sure the header actually has the threshold you think it does ---
if ! grep -qE "static constexpr double E_MONITOR = ${EXPECTED_THRESHOLD};" "$HEADER_FILE"; then
    echo "ABORTING: $HEADER_FILE does not contain 'E_MONITOR = ${EXPECTED_THRESHOLD};'"
    echo "Current value:"
    grep "E_MONITOR = " "$HEADER_FILE"
    echo "Edit the header to E_MONITOR = ${EXPECTED_THRESHOLD}; and re-run this script."
    exit 1
fi
if ! grep -q "static constexpr Strategy MODE = Strategy::CAMRA;" "$HEADER_FILE"; then
    echo "ABORTING: MODE is not set to Strategy::CAMRA in $HEADER_FILE"
    grep "static constexpr Strategy MODE" "$HEADER_FILE"
    exit 1
fi
echo "Header check passed: E_MONITOR=${EXPECTED_THRESHOLD}, MODE=CAMRA."
echo ""

mkdir -p "$DEST_ROOT"

echo "=== Starting $N_RUNS CAMRA runs of bremen3 (E_MONITOR=${EXPECTED_THRESHOLD}) ==="
echo "Results will land in: $DEST_ROOT"
echo ""

fail_count=0
seed_mismatch_count=0

for ((i = 1; i <= N_RUNS; i++)); do
    seed_set=$((SEED_SET_BASE + i - 1))
    sumo_seed=$((SUMO_SEED_BASE + i - 1))
    run_dest="$DEST_ROOT/run$i results"

    echo "[run $i/$N_RUNS] starting — seed-set=$seed_set, sumo-seed=$sumo_seed ..."
    start_time=$(date +%s)

    mkdir -p "$RESULTS_SRC_DIR"
    rm -f "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca 2>/dev/null

    (
        cd "$BUILD_DIR"
        cmake . -DRUN_FLAGS="-u Cmdenv -c inet --seed-set=$seed_set --traci.launcher.seed=$sumo_seed" > /dev/null
        cmake --build . --target run_mrsu_bremen3 -j4
    ) > "/tmp/bremen3_camra_em022_run${i}.log" 2>&1
    status=$?

    elapsed=$(( $(date +%s) - start_time ))

    if [[ $status -ne 0 ]]; then
        echo "[run $i/$N_RUNS] FAILED after ${elapsed}s — see /tmp/bremen3_camra_em022_run${i}.log for details. Moving on to the next run."
        fail_count=$((fail_count + 1))
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
        echo "[run $i/$N_RUNS] WARNING: finished but no result files found — check /tmp/bremen3_camra_em022_run${i}.log"
        fail_count=$((fail_count + 1))
        echo ""
        continue
    fi

    # --- Verify the seed actually recorded in this run's .sca matches what we asked for ---
    sca_file="$run_dest/inet-#0.sca"
    if [[ -f "$sca_file" ]]; then
        recorded_seed=$(grep -m1 "seedset" "$sca_file" | grep -oE "[0-9]+" | tail -1)
        if [[ "$recorded_seed" != "$seed_set" ]]; then
            echo "[run $i/$N_RUNS] *** SEED MISMATCH *** expected seedset=$seed_set, .sca says seedset=$recorded_seed"
            seed_mismatch_count=$((seed_mismatch_count + 1))
        fi
    else
        echo "[run $i/$N_RUNS] WARNING: no inet-#0.sca found to verify seed"
    fi

    echo "[run $i/$N_RUNS] done in ${elapsed}s — saved to: $run_dest"
    echo ""
done

echo "=== All $N_RUNS runs complete. Check: $DEST_ROOT ==="
echo "Failed/incomplete runs: $fail_count"
echo "Seed mismatches: $seed_mismatch_count"
if [[ $fail_count -gt 0 || $seed_mismatch_count -gt 0 ]]; then
    echo "!!! Review the warnings above before treating this batch as clean. !!!"
fi
