#!/usr/bin/env bash
set -uo pipefail

BUILD_DIR="/home/fahid/artery/build"
SCENARIO_DIR="/home/fahid/artery/scenarios/mrsu_bremen3"
RESULTS_SRC_DIR="$SCENARIO_DIR/results"
BATCH_ROOT="/mnt/d/Uni_Bremen/E-Learning/Project/Simulation/batch results/Camra coverage-primary dynamic"
N_RUNS=20
SEED_SET_BASE=1000     # 1000..1019
SUMO_SEED_BASE=50000   # 50000..50019

mkdir -p "$BATCH_ROOT"
MASTER_LOG="$BATCH_ROOT/batch_run.log"
log() { printf '[%(%Y-%m-%d %H:%M:%S)T] %s\n' -1 "$*" | tee -a "$MASTER_LOG"; }

log "=== CAMRA dynamic threshold: $N_RUNS runs ==="
for ((i = 0; i < N_RUNS; i++)); do
    run_tag=$(printf "run%d results" $((i + 1)))
    run_dir="$BATCH_ROOT/$run_tag"
    if [[ -d "$run_dir" ]] && compgen -G "$run_dir/*.csv" > /dev/null; then
        log "[$run_tag] already archived - skipping"; continue
    fi
    seed_set=$((SEED_SET_BASE + i)); sumo_seed=$((SUMO_SEED_BASE + i))
    log "[$run_tag] start seed-set=$seed_set sumo-seed=$sumo_seed"
    t0=$(date +%s)
    mkdir -p "$RESULTS_SRC_DIR"
    rm -f "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca "$RESULTS_SRC_DIR"/*.vec 2>/dev/null || true
    (
        cd "$BUILD_DIR" || exit 1
        cmake . -DRUN_FLAGS="-u Cmdenv -c inet --seed-set=$seed_set --traci.launcher.seed=$sumo_seed" > /dev/null
        cmake --build . --target run_mrsu_bremen3 -j4
    ) > "/tmp/camra_covdyn_run$((i + 1)).log" 2>&1
    status=$?
    mkdir -p "$run_dir"
    shopt -s nullglob
    for f in "$RESULTS_SRC_DIR"/*.csv "$RESULTS_SRC_DIR"/*.sca "$RESULTS_SRC_DIR"/*.vec; do mv "$f" "$run_dir/"; done
    shopt -u nullglob
    cp "/tmp/camra_covdyn_run$((i + 1)).log" "$run_dir/console.log"
    echo "seed_set=$seed_set sumo_seed=$sumo_seed exit=$status elapsed_s=$(( $(date +%s) - t0 ))" > "$run_dir/run_meta.txt"
    log "[$run_tag] done exit=$status in $(( $(date +%s) - t0 ))s"
done
log "=== batch complete ==="