#!/usr/bin/env bash
# collect_for_github.sh  -  run inside WSL (Ubuntu) as user "fahid"
#
# Collects (1) every software version used for CAMRA, and (2) a single patch
# (.diff) that turns a clean upstream Artery checkout into your working tree
# (scenarios + any changes to the Artery sources), without touching your repo.
#
# Usage:   bash "/mnt/d/Uni_Bremen/E-Learning/Project/CAMRA/New Camra/github_prep/collect_for_github.sh"
# Output:  /mnt/d/Uni_Bremen/E-Learning/Project/CAMRA/New Camra/github_prep/out/
set -u
ART="${ART:-$HOME/artery}"
OUT="${OUT:-/mnt/d/Uni_Bremen/E-Learning/Project/CAMRA/New Camra/github_prep/out}"
mkdir -p "$OUT"
ENVF="$OUT/env_versions.txt"

sec() { printf '\n== %s\n' "$*"; }
try() { "$@" 2>&1 | head -${LINES_MAX:-3}; }

{
sec "date";           date -Is
sec "os-release";     grep -E '^(PRETTY_NAME|VERSION)=' /etc/os-release
sec "kernel / wsl";   uname -r; (command -v wslinfo >/dev/null && wslinfo --version 2>&1 | head -2) || true
sec "compilers/build tools"
try gcc --version; try g++ --version; try cmake --version; try make --version
try ninja --version; try git --version; try python3 --version; try pip3 --version
sec "OMNeT++"
echo "OMNETPP_ROOT=${OMNETPP_ROOT:-<unset>}"
try which opp_run
try opp_run -v
[ -n "${OMNETPP_ROOT:-}" ] && { cat "$OMNETPP_ROOT/Version" 2>/dev/null; grep -E '^(WITH_|CXXFLAGS|MODE)' "$OMNETPP_ROOT/Makefile.inc" 2>/dev/null | head -20; }
sec "SUMO"
echo "SUMO_HOME=${SUMO_HOME:-<unset>}"
try which sumo
LINES_MAX=2 try sumo --version
sec "python packages"
pip3 list 2>/dev/null | grep -i -E '^(eclipse-sumo|traci|libsumo|sumolib|numpy|pandas|scipy|matplotlib|pillow|opencv|python-pptx|lxml) ' || echo "(none found via pip3 list)"
python3 - <<'PY' 2>&1
import importlib
for m in ("traci","libsumo","sumolib","numpy","pandas","scipy","matplotlib"):
    try:
        mod=importlib.import_module(m); print(m, getattr(mod,"__version__","(no __version__)"))
    except Exception as e: print(m,"NOT IMPORTABLE:",type(e).__name__)
PY
sec "system libraries (dpkg)"
dpkg -l 2>/dev/null | awk '/^ii/ {print $2" "$3}' | grep -E '^(libboost-dev|libboost1\.[0-9.]*-dev|libcrypto\+\+-dev|libgeographic-dev|libprotobuf-dev|libxml2-dev|libzmq3-dev|libgeos-dev|libproj-dev|libfox-1\.6-dev|libxerces-c-dev|qtbase5-dev|bison|flex|swig|tcl-dev|tk-dev|sumo|sumo-tools|sumo-doc) ' || true
sec "Artery repository"
if [ -d "$ART/.git" ] || git -C "$ART" rev-parse >/dev/null 2>&1; then
  echo "path: $ART"
  echo "HEAD: $(git -C "$ART" rev-parse HEAD)"
  echo "describe: $(git -C "$ART" describe --always --tags 2>&1)"
  echo "branch: $(git -C "$ART" branch --show-current)"
  echo "last commit: $(git -C "$ART" log -1 --format='%ci  %s')"
  git -C "$ART" remote -v
  sec "Artery submodules (INET / Vanetza / Veins / pybind11 ...)"
  git -C "$ART" submodule status 2>&1
  for s in $(git -C "$ART" submodule --quiet foreach 'echo $sm_path' 2>/dev/null); do
     echo "-- $s : $(git -C "$ART/$s" describe --always --tags 2>&1)  remote: $(git -C "$ART/$s" remote get-url origin 2>&1)"
  done
else
  echo "NOT A GIT REPO: $ART  (then the patch cannot be generated - tell Claude)"
fi
sec "Artery CMake configuration"
grep -E '^(CMAKE_BUILD_TYPE|CMAKE_CXX_COMPILER:|CMAKE_CXX_STANDARD|WITH_[A-Z_]+|OMNETPP_[A-Z_]*|Boost_INCLUDE_DIR|SUMO[A-Z_]*|BUILD_TESTING)' "$ART/build/CMakeCache.txt" 2>/dev/null || echo "(no build/CMakeCache.txt)"
sec "scenarios present"
ls -1 "$ART/scenarios" 2>/dev/null
sec "CAMRA run targets in build"
ls -1 "$ART/build/bin" 2>/dev/null | head -50
} > "$ENVF" 2>&1
echo "[1/3] versions written to: $ENVF"

# ---------------- patch ----------------
cd "$ART" || exit 1
git status --short > "$OUT/git_status.txt" 2>&1
TMPIDX="$(mktemp)"; export GIT_INDEX_FILE="$TMPIDX"
git read-tree HEAD
git add -A -- . \
  ':(exclude)build' ':(exclude)**/results' ':(exclude)**/results_batch' ':(exclude)**/results*' \
  ':(exclude)*.sca' ':(exclude)*.vec' ':(exclude)*.vci' ':(exclude)*.log' ':(exclude)*.o' ':(exclude)*.o.d' \
  ':(exclude)*Zone.Identifier' ':(exclude)*.pre_*' ':(exclude)*.bak' ':(exclude)*.orig' ':(exclude)**/__pycache__' \
  ':(exclude).cmdenv-log' ':(exclude).qtenvrc' ':(exclude)**/tripinfo*' ':(exclude)**/vehroutes*' ':(exclude)*.swp' 2>>"$OUT/git_add_warnings.txt"
git diff --cached --binary HEAD > "$OUT/camra-artery.diff"
git diff --cached --name-status HEAD > "$OUT/patch_files.txt"
git diff --cached --stat=140 HEAD > "$OUT/patch_stat.txt"
git diff --cached --name-only HEAD | xargs -d '\n' -I{} du -k "{}" 2>/dev/null | sort -rn | head -25 > "$OUT/patch_largest_files.txt"
unset GIT_INDEX_FILE; rm -f "$TMPIDX"
echo "[2/3] patch written: $OUT/camra-artery.diff  ($(du -h "$OUT/camra-artery.diff" | cut -f1))"
# modifications INSIDE submodules (not visible to the parent diff)
for s in $(git submodule --quiet foreach 'echo $sm_path' 2>/dev/null); do
  if ! git -C "$s" diff --quiet HEAD 2>/dev/null; then
     git -C "$s" diff --binary HEAD > "$OUT/submodule_$(echo "$s" | tr '/' '_').diff"
     echo "   NOTE: submodule $s has local changes -> $OUT/submodule_$(echo "$s" | tr '/' '_').diff"
  fi
done
# which files are changes to Artery itself (outside scenarios/)?
grep -v -E '^[AMD]\s+scenarios/' "$OUT/patch_files.txt" > "$OUT/patch_files_outside_scenarios.txt" || true
echo "[3/3] files changed OUTSIDE scenarios/: $(wc -l < "$OUT/patch_files_outside_scenarios.txt")  (see patch_files_outside_scenarios.txt)"
echo "DONE. Tell Claude: finished."
