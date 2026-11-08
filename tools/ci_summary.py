#!/usr/bin/env python3
"""
ci_summary.py - mean, standard deviation and 95 % confidence interval (Student's t)
of the CAMRA evaluation metrics over repeated simulation runs.

The per-run metrics are computed exactly as in compute_efficiency.py
(cold-start window excluded, blackout = no active MRSU and 0 % coverage, 0.15 penalty
per blackout window, quality bar 0.75).

Usage
-----
  python3 ci_summary.py --name CAMRA "batch results/Camra dynamic threshold/run*/scm_history_CAMRA.csv"
  python3 ci_summary.py --name Always-Active "batch results/Always Active 15/run*/scm_history_baseline_allactive.csv" --candidates 14

Optional: --normal  uses the normal distribution (z = 1.96) instead of Student's t.
"""
import argparse, csv, glob, math, sys
import numpy as np
from scipy import stats

BLACKOUT_PENALTY = 0.15
QUALITY_BAR = 0.75


def run_metrics(path, candidates, skip_cold_start=True):
    rows = list(csv.DictReader(open(path, newline="")))
    if skip_cold_start:
        rows = rows[1:]
    err = [float(r["qos_error"]) for r in rows]
    cov = [float(r["coverage"]) for r in rows]
    blackouts, active_ok, per_window_r = 0, [], []
    for r in rows:
        a = int(r["active_mrsus"]); rep = int(r.get("reporting_mrsus") or 0); c = float(r["coverage"])
        if a == 0 and c == 0:
            blackouts += 1
        else:
            active_ok.append(a)
            if rep > 0:
                per_window_r.append(a / rep)
    q_raw = 1 - sum(err) / len(err)
    q = max(0.0, q_raw - blackouts * BLACKOUT_PENALTY)
    avg_active = float(np.mean(active_ok)) if active_ok else 0.0
    R = float(np.mean(per_window_r)) if per_window_r else avg_active / candidates
    eff = q / R if (q >= QUALITY_BAR and R > 0) else None
    return {"Q": q, "coverage": float(np.mean(cov)), "avg_active_mrsus": avg_active, "R": R,
            "blackout_windows": blackouts, "efficiency": eff}


def mean_ci(values, normal=False, level=0.95):
    v = np.asarray(values, float)
    n = len(v)
    if n == 0:
        return float("nan"), float("nan"), float("nan"), 0
    m = float(v.mean())
    if n < 2:
        return m, 0.0, 0.0, n
    sd = float(v.std(ddof=1))
    crit = stats.norm.ppf(0.5 + level / 2) if normal else stats.t.ppf(0.5 + level / 2, n - 1)
    return m, sd, float(crit * sd / math.sqrt(n)), n


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pattern", help="glob for the per-run scm_history CSV files (quote it)")
    ap.add_argument("--name", default="strategy")
    ap.add_argument("--candidates", type=int, default=14, help="number of MRSU candidates (only used if reporting_mrsus is missing)")
    ap.add_argument("--normal", action="store_true", help="use the normal distribution instead of Student's t")
    ap.add_argument("--csv", help="also write the per-run values to this CSV file")
    a = ap.parse_args()

    files = sorted(glob.glob(a.pattern))
    if not files:
        sys.exit("no files match: " + a.pattern)
    runs = [run_metrics(f, a.candidates) for f in files]
    dist = "normal (z)" if a.normal else "Student's t"
    print(f"{a.name}: {len(runs)} runs, 95 % CI of the mean, {dist}")
    print(f"{'metric':18s} {'mean':>9s} {'SD':>9s} {'CI half-width':>14s}   95 % CI")
    for k in ("Q", "coverage", "avg_active_mrsus", "R", "efficiency", "blackout_windows"):
        vals = [r[k] for r in runs if r[k] is not None]
        m, sd, h, n = mean_ci(vals, a.normal)
        note = "" if n == len(runs) else f"   (n={n}: only runs that pass the quality bar)"
        print(f"{k:18s} {m:9.4f} {sd:9.4f} {h:14.4f}   [{m-h:.4f}, {m+h:.4f}]{note}")
    if a.csv:
        with open(a.csv, "w", newline="") as f:
            w = csv.writer(f); w.writerow(["file"] + list(runs[0].keys()))
            for fn, r in zip(files, runs):
                w.writerow([fn] + [("" if v is None else v) for v in r.values()])


if __name__ == "__main__":
    main()
