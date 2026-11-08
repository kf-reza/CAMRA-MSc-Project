#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# run_lust_segment_5runs.sh — runs the LuST 20km^2 segment (CAMRA, 600s,
# 8:00-8:10am window, 40% downsampled cars/through-traffic, real buses as
# MRSU candidates) 5 times, headless, each with a different seed, and
# copies each run's results straight into your data folder.
#
# HOW TO RUN (just these two lines, from anywhere in your VM terminal):
#   chmod +x run_lust_segment_5runs.sh
#   ./run_lust_segment_5runs.sh
#
# It will take a while (5x however long one run takes). Just let it run --
# it prints progress as it goes, and each run's results land in their own
# folder under /home/kazi/data/Simulation Data_Lust segment/ as soon as
# that run finishes. Safe to walk away / close the terminal if you run it
# inside tmux (see the note at the bottom).
# ---------------------------------------------------------------------------
set -uo pipefail

BUILD_DIR="/home/kazi/artery/build"
RESULTS_SRC_DIR="/home/kazi/artery/scenarios/mrsu_lust/results"
DEST_ROOT="/home/kazi/data/Simulation Data_Lust segment"
N_RUNS=5
SEED_SET_BASE=4000     # seed-set values used: 4000..4004 (OMNeT++'s own RNG stream)
SUMO_SEED_BASE=80000   # SUMO seeds used:      80000..80004

mkdir -p "$DEST_ROOT"

echo "=== Starting $N_RUNS CAMRA runs of the LuST segment ==="
echo "Results will land in: $DEST_ROOT"
echo ""

for ((i = 1; i <= N_RUNS; i++)); do
    seed_set=$((SEED_SET_BASE + i - 1))
    sumo_seed=$((SUMO_SEED_BASE + i - 1))
    run_dest="$DEST_ROOT/run$i"

    echo "[run $i/$N_RUNS] starting -- seed-set=$seed_set, sumo-seed=$sumo_seed ..."
    start_time=$(date +%s)

    # clear out any leftover files from a previous attempt so this run's
    # results don't get mixed up with an old one
    mkdir -p "$RESULTS_SRC_DIR"
    rm -f "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca 2>/dev/null

    (
        cd "$BUILD_DIR"
        cmake . -DRUN_FLAGS="-u Cmdenv -c inet --seed-set=$seed_set --traci.launcher.seed=$sumo_seed --sim-time-limit=600s" > /dev/null
        cmake --build . --target run_mrsu_lust -j4
    ) > "/tmp/lust_segment_run${i}.log" 2>&1
    status=$?

    elapsed=$(( $(date +%s) - start_time ))

    if [[ $status -ne 0 ]]; then
        echo "[run $i/$N_RUNS] FAILED after ${elapsed}s -- see /tmp/lust_segment_run${i}.log for details. Moving on to the next run."
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
        echo "[run $i/$N_RUNS] WARNING: finished but no result files found -- check /tmp/lust_segment_run${i}.log"
    else
        echo "[run $i/$N_RUNS] done in ${elapsed}s -- saved to: $run_dest"
    fi
    echo ""
done

echo "=== All $N_RUNS runs complete. Check: $DEST_ROOT ==="

# ---------------------------------------------------------------------------
# TIP: to let this survive you closing your terminal / disconnecting,
# run it inside tmux instead of directly:
#   tmux new -s lustbatch
#   ./run_lust_segment_5runs.sh
#   (press Ctrl+B then D to detach; reconnect any time with:)
#   tmux attach -t lustbatch
# ---------------------------------------------------------------------------

