import csv
import sys

MIN_QUALITY_THRESHOLD = 0.75
EXCLUDE_COLD_START_WINDOW = True  # exclude only the very first SCM window (unavoidable startup cycle),
                                   # applied uniformly to every strategy for fairness

def compute_metrics(filepath, total_candidates=9):
    qos_errors = []
    per_window_r = []
    awareness_values = []
    pdr_values = []
    coverage_values = []
    blackout_windows = 0
    active_counts = []

    with open(filepath, newline='') as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    if EXCLUDE_COLD_START_WINDOW and len(rows) > 0:
        rows = rows[1:]

    for row in rows:
        qos_errors.append(float(row['qos_error']))
        active = int(row['active_mrsus'])
        active_counts.append(active)
        reporting = int(row.get('reporting_mrsus', 0))
        if reporting > 0:
            per_window_r.append(active / reporting)
        pdr_values.append(float(row['segment_pdr']))
        cov = float(row['coverage'])
        coverage_values.append(cov)
        if 'segment_awareness' in row and row['segment_awareness'] != '':
            awareness_values.append(float(row['segment_awareness']))
        if active == 0 and cov == 0:
            blackout_windows += 1

    avg_error = sum(qos_errors) / len(qos_errors)
    avg_active = sum(active_counts) / len(active_counts)
    avg_pdr = sum(pdr_values) / len(pdr_values)
    avg_coverage = sum(coverage_values) / len(coverage_values)
    avg_awareness = sum(awareness_values) / len(awareness_values) if awareness_values else None

    Q = 1 - avg_error
    R = sum(per_window_r) / len(per_window_r) if per_window_r else (avg_active / total_candidates)
    meets_quality_bar = Q >= MIN_QUALITY_THRESHOLD
    efficiency = (Q / R) if (R > 0 and meets_quality_bar) else None

    return {
        "windows": len(qos_errors),
        "avg_qos_error": avg_error,
        "avg_active_mrsus": avg_active,
        "avg_pdr": avg_pdr,
        "avg_coverage": avg_coverage,
        "avg_awareness": avg_awareness,
        "blackout_windows": blackout_windows,
        "Q": Q,
        "R": R,
        "meets_quality_bar": meets_quality_bar,
        "efficiency": efficiency,
    }

def print_report(name, m):
    print(f"\n=== {name} ===")
    print(f"  SCM windows analyzed:     {m['windows']}" + (" (cold-start window excluded)" if EXCLUDE_COLD_START_WINDOW else ""))
    print(f"  Average QoS error (E):    {m['avg_qos_error']:.4f}")
    print(f"  Average active MRSUs:     {m['avg_active_mrsus']:.2f}")
    print(f"  Average PDR:              {m['avg_pdr']:.4f}")
    print(f"  Average Coverage:         {m['avg_coverage']:.4f}")
    if m['avg_awareness'] is not None:
        print(f"  Average Segment Awareness:{m['avg_awareness']:.4f}")
    print(f"  Blackout windows (0 active, 0% coverage): {m['blackout_windows']} / {m['windows']}")
    print(f"  Quality Score (Q):        {m['Q']:.4f}   (minimum required: {MIN_QUALITY_THRESHOLD})")
    print(f"  Resource Usage (R):       {m['R']:.4f}   (avg of active/reporting per window)")
    if m['meets_quality_bar']:
        print(f"  Resource Efficiency Q/R:  {m['efficiency']:.4f}")
    else:
        print(f"  Resource Efficiency Q/R:  DISQUALIFIED \u2014 quality below minimum threshold ({m['Q']:.4f} < {MIN_QUALITY_THRESHOLD})")

if __name__ == "__main__":
    files = {
        "CAMRA": "results/scm_history_CAMRA.csv",
        "All-Active": "results/scm_history_baseline_allactive.csv",
        "Fixed-Single": "results/scm_history_baseline_fixedsingle.csv",
        "Static-Periodic": "results/scm_history_baseline_staticperiodic.csv",
    }
    keys = list(files.keys())
    for i, arg in enumerate(sys.argv[1:5]):
        files[keys[i]] = arg
    total_candidates = int(sys.argv[5]) if len(sys.argv) > 5 else 9

    if EXCLUDE_COLD_START_WINDOW:
        print("NOTE: The first SCM window (unavoidable cold-start cycle before any reactive")
        print("strategy can make its first decision) is excluded from all strategies below,")
        print("for a fair, uniform comparison. Mid-run blackouts are still counted normally.")
        print("Resource Usage (R) is now computed per-window as active/reporting, then averaged.\n")

    strategies = {}
    for name, path in files.items():
        try:
            strategies[name] = compute_metrics(path, total_candidates)
        except FileNotFoundError:
            print(f"(Skipping {name}: file not found at {path})")

    for name, m in strategies.items():
        print_report(name, m)

    qualified = {k: v for k, v in strategies.items() if v['meets_quality_bar']}
    disqualified = {k: v for k, v in strategies.items() if not v['meets_quality_bar']}

    print(f"\n=== Verdict (minimum acceptable quality: Q \u2265 {MIN_QUALITY_THRESHOLD}) ===")
    for name, m in disqualified.items():
        print(f"  DISQUALIFIED: {name} \u2014 Q={m['Q']:.4f} fails minimum service requirement "
              f"({m['blackout_windows']} full service blackout window(s) observed)")

    print(f"\n  Resource Efficiency Ranking, among strategies meeting the quality bar (higher is better):")
    ranked = sorted(qualified.items(), key=lambda kv: kv[1]['efficiency'], reverse=True)
    for i, (name, m) in enumerate(ranked, 1):
        print(f"    {i}. {name:16s} Efficiency={m['efficiency']:.4f}  (Q={m['Q']:.4f}, R={m['R']:.4f}, avg active={m['avg_active_mrsus']:.2f})")

    if "CAMRA" in qualified and "All-Active" in qualified:
        camra = qualified["CAMRA"]
        allactive = qualified["All-Active"]
        gain = ((camra['efficiency'] - allactive['efficiency']) / allactive['efficiency']) * 100
        print(f"\n  CAMRA vs. All-Active (both meet quality bar): {gain:+.1f}% more resource-efficient, "
              f"using {camra['avg_active_mrsus']:.2f} vs {allactive['avg_active_mrsus']:.2f} avg active MRSUs, "
              f"for only {(allactive['Q']-camra['Q'])*100:.1f} points less quality.")

    for disq_name in ["Fixed-Single", "Static-Periodic"]:
        if disq_name in disqualified:
            m = disqualified[disq_name]
            print(f"\n  {disq_name}'s low resource usage (R={m['R']:.4f}) does NOT represent genuine efficiency:")
            print(f"    it failed to maintain acceptable service (Q={m['Q']:.4f}), including {m['blackout_windows']} "
                  f"complete service blackout(s) where coverage dropped to 0%.")

    if "Fixed-Single" in strategies and "Static-Periodic" in strategies:
        fs = strategies["Fixed-Single"]
        sp = strategies["Static-Periodic"]
        print(f"\n  Fixed-Single vs. Static-Periodic (both cap at ~1 active MRSU, isolating candidate SELECTION):")
        print(f"    Fixed-Single (smart, S_i-based selection):  Q={fs['Q']:.4f}, blackouts={fs['blackout_windows']}")
        print(f"    Static-Periodic (arrival-order, no selection logic): Q={sp['Q']:.4f}, blackouts={sp['blackout_windows']}")
        if fs['Q'] > sp['Q']:
            print(f"    -> Smart candidate selection alone improves quality by {(fs['Q']-sp['Q'])*100:.1f} points, "
                  f"even with equal resource constraints.")

    with open("results/resource_efficiency_report.csv", "w") as f:
        f.write("strategy,windows,avg_qos_error,avg_active_mrsus,avg_pdr,avg_coverage,avg_awareness,blackout_windows,Q,R,meets_quality_bar,efficiency\n")
        for name, m in strategies.items():
            aw = f"{m['avg_awareness']:.4f}" if m['avg_awareness'] is not None else ""
            eff = f"{m['efficiency']:.4f}" if m['efficiency'] is not None else "DISQUALIFIED"
            f.write(f"{name},{m['windows']},{m['avg_qos_error']:.4f},{m['avg_active_mrsus']:.2f},"
                    f"{m['avg_pdr']:.4f},{m['avg_coverage']:.4f},{aw},{m['blackout_windows']},"
                    f"{m['Q']:.4f},{m['R']:.4f},{m['meets_quality_bar']},{eff}\n")
    print("\nSaved to results/resource_efficiency_report.csv")