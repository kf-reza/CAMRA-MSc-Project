import csv
import sys
import os

MIN_QUALITY_THRESHOLD = 0.75
EXCLUDE_COLD_START_WINDOW = True
BLACKOUT_PENALTY = 0.15

def compute_metrics(filepath, total_candidates=9, blackout_penalty=BLACKOUT_PENALTY):
    qos_errors = []
    per_window_r = []
    awareness_values = []
    pdr_values = []
    coverage_values = []
    cbr_values = []
    blackout_windows = 0
    active_counts_nonblackout = []

    with open(filepath, newline='') as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    if EXCLUDE_COLD_START_WINDOW and len(rows) > 0:
        rows = rows[1:]

    for row in rows:
        qos_errors.append(float(row['qos_error']))
        active = int(row['active_mrsus'])
        reporting = int(row.get('reporting_mrsus', 0))
        pdr_values.append(float(row['segment_pdr']))
        cov = float(row['coverage'])
        coverage_values.append(cov)
        if 'segment_avg_cbr' in row and row['segment_avg_cbr'] != '':
            cbr_values.append(float(row['segment_avg_cbr']))
        if 'segment_awareness' in row and row['segment_awareness'] != '':
            awareness_values.append(float(row['segment_awareness']))

        is_blackout = (active == 0 and cov == 0)
        if is_blackout:
            blackout_windows += 1
        else:
            active_counts_nonblackout.append(active)
            if reporting > 0:
                per_window_r.append(active / reporting)

    avg_error = sum(qos_errors) / len(qos_errors)
    avg_active = (sum(active_counts_nonblackout) / len(active_counts_nonblackout)
                  if active_counts_nonblackout else 0)
    avg_pdr = sum(pdr_values) / len(pdr_values)
    avg_coverage = sum(coverage_values) / len(coverage_values)
    avg_cbr = sum(cbr_values) / len(cbr_values) if cbr_values else None
    avg_awareness = sum(awareness_values) / len(awareness_values) if awareness_values else None

    Q_raw = 1 - avg_error
    Q = max(0, Q_raw - (blackout_windows * blackout_penalty))

    R = sum(per_window_r) / len(per_window_r) if per_window_r else (avg_active / total_candidates)
    meets_quality_bar = Q >= MIN_QUALITY_THRESHOLD
    efficiency = (Q / R) if (R > 0 and meets_quality_bar) else None

    return {
        "windows": len(qos_errors), "avg_qos_error": avg_error, "avg_active_mrsus": avg_active,
        "avg_pdr": avg_pdr, "avg_coverage": avg_coverage, "avg_cbr": avg_cbr,
        "avg_awareness": avg_awareness, "blackout_windows": blackout_windows,
        "Q_raw": Q_raw, "Q": Q, "R": R,
        "meets_quality_bar": meets_quality_bar, "efficiency": efficiency,
    }

def comm_summary_stats(comm_summary_path):
    """From a communication_summary.csv: total relay sent (MRSU rows), and average +
    zero-count extended_awareness (V2X rows only, i.e. excluding MRSU)."""
    if not comm_summary_path or not os.path.exists(comm_summary_path):
        return None
    relay_total = 0
    mrsu_count = 0
    ext_awareness = []
    with open(comm_summary_path, newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row.get('role') == 'MRSU':
                relay_total += int(row.get('relay_sent', 0))
                mrsu_count += 1
            elif row.get('role') == 'V2X':
                if row.get('extended_awareness', '') != '':
                    ext_awareness.append(int(row['extended_awareness']))
    avg_ext = sum(ext_awareness) / len(ext_awareness) if ext_awareness else None
    zero_count = sum(1 for x in ext_awareness if x == 0)
    return {
        "relay_total": relay_total, "mrsu_count": mrsu_count,
        "avg_extended_awareness": avg_ext, "n_vehicles": len(ext_awareness),
        "zero_awareness_count": zero_count,
    }

def print_report(name, m, comm=None):
    print(f"\n=== {name} ===")
    print(f"  SCM windows analyzed:     {m['windows']}" + (" (cold-start window excluded)" if EXCLUDE_COLD_START_WINDOW else ""))
    print(f"  Average QoS error (E):    {m['avg_qos_error']:.4f}")
    print(f"  Average active MRSUs (excl. blackout windows): {m['avg_active_mrsus']:.2f}")
    print(f"  Average PDR:              {m['avg_pdr']:.4f}")
    print(f"  Average Coverage:         {m['avg_coverage']:.4f}")
    if m['avg_cbr'] is not None:
        print(f"  Average CBR:              {m['avg_cbr']:.4f}")
    if m['avg_awareness'] is not None:
        print(f"  Average Segment Awareness (per-window, connected only): {m['avg_awareness']:.4f}")
    if comm and comm.get('avg_extended_awareness') is not None:
        print(f"  Average Extended Awareness (per-vehicle, lifetime): {comm['avg_extended_awareness']:.2f}"
              f"  ({comm['zero_awareness_count']}/{comm['n_vehicles']} vehicles got zero relay benefit)")
    print(f"  Blackout windows:         {m['blackout_windows']}/{m['windows']}")
    print(f"  Quality Score (Q, raw):        {m['Q_raw']:.4f}")
    print(f"  Quality Score (Q, blackout-penalized): {m['Q']:.4f}   (minimum required: {MIN_QUALITY_THRESHOLD})")
    print(f"  Resource Usage (R):       {m['R']:.4f}")
    if m['meets_quality_bar']:
        print(f"  Resource Efficiency Q/R:  {m['efficiency']:.4f}")
    else:
        print(f"  Resource Efficiency Q/R:  DISQUALIFIED (penalized Q {m['Q']:.4f} < {MIN_QUALITY_THRESHOLD})")
    if comm:
        print(f"  Total relay messages sent (all MRSUs): {comm['relay_total']}")

if __name__ == "__main__":
    files = {
        "CAMRA": "results/scm_history_CAMRA.csv",
        "All-Active": "results/scm_history_baseline_allactive.csv",
        "Fixed-Single": "results/scm_history_baseline_fixedsingle.csv",
        "Static-Periodic": "results/scm_history_baseline_staticperiodic.csv",
    }
    comm_files = {
        "CAMRA": "results/communication_summary_camra.csv",
        "All-Active": "results/communication_summary_allactive.csv",
        "Fixed-Single": "results/communication_summary_fixedsingle.csv",
        "Static-Periodic": "results/communication_summary_staticperiodic.csv",
    }
    keys = list(files.keys())
    for i, arg in enumerate(sys.argv[1:5]):
        files[keys[i]] = arg
    total_candidates = int(sys.argv[5]) if len(sys.argv) > 5 else 9

    strategies = {}
    comms = {}
    for name, path in files.items():
        try:
            strategies[name] = compute_metrics(path, total_candidates)
        except FileNotFoundError:
            print(f"(Skipping {name}: file not found at {path})")
    for name, path in comm_files.items():
        comms[name] = comm_summary_stats(path)

    for name, m in strategies.items():
        print_report(name, m, comms.get(name))

    qualified = {k: v for k, v in strategies.items() if v['meets_quality_bar']}
    disqualified = {k: v for k, v in strategies.items() if not v['meets_quality_bar']}

    print(f"\n=== Verdict (minimum acceptable quality: Q \u2265 {MIN_QUALITY_THRESHOLD}, after blackout penalty) ===")
    for name, m in disqualified.items():
        print(f"  DISQUALIFIED: {name} \u2014 penalized Q={m['Q']:.4f} (raw Q={m['Q_raw']:.4f}, "
              f"{m['blackout_windows']} blackout window(s) x {BLACKOUT_PENALTY} penalty each)")

    print(f"\n  Resource Efficiency Ranking, among strategies meeting the quality bar (higher is better):")
    ranked = sorted(qualified.items(), key=lambda kv: kv[1]['efficiency'], reverse=True)
    for i, (name, m) in enumerate(ranked, 1):
        print(f"    {i}. {name:16s} Efficiency={m['efficiency']:.4f}  (Q={m['Q']:.4f}, R={m['R']:.4f}, avg active={m['avg_active_mrsus']:.2f})")

    if "CAMRA" in qualified and "All-Active" in qualified:
        camra = qualified["CAMRA"]
        allactive = qualified["All-Active"]
        gain = ((camra['efficiency'] - allactive['efficiency']) / allactive['efficiency']) * 100
        print(f"\n  CAMRA vs. All-Active: {gain:+.1f}% more resource-efficient")
        print(f"    Using {camra['avg_active_mrsus']:.2f} vs {allactive['avg_active_mrsus']:.2f} avg active MRSUs")
        print(f"    Quality gap: {(allactive['Q']-camra['Q'])*100:.2f} points")

    print(f"\n=== Extended Awareness Comparison (per-vehicle, whole-lifetime) ===")
    for name in files.keys():
        c = comms.get(name)
        if c and c.get('avg_extended_awareness') is not None:
            print(f"  {name:16s} avg={c['avg_extended_awareness']:6.2f}  "
                  f"zero-benefit={c['zero_awareness_count']}/{c['n_vehicles']}  "
                  f"total relay sent={c['relay_total']}")

    with open("results/resource_efficiency_report.csv", "w") as f:
        f.write("strategy,windows,avg_qos_error,avg_active_mrsus,avg_pdr,avg_coverage,avg_cbr,avg_segment_awareness,avg_extended_awareness,zero_awareness_vehicles,total_vehicles_measured,total_relay_sent,blackout_windows,Q_raw,Q_penalized,R,meets_quality_bar,efficiency\n")
        for name, m in strategies.items():
            cbr = f"{m['avg_cbr']:.4f}" if m['avg_cbr'] is not None else ""
            aw = f"{m['avg_awareness']:.4f}" if m['avg_awareness'] is not None else ""
            c = comms.get(name)
            ext_aw = f"{c['avg_extended_awareness']:.2f}" if c and c.get('avg_extended_awareness') is not None else ""
            zero_c = c['zero_awareness_count'] if c else ""
            n_v = c['n_vehicles'] if c else ""
            relay_t = c['relay_total'] if c else ""
            eff = f"{m['efficiency']:.4f}" if m['efficiency'] is not None else "DISQUALIFIED"
            f.write(f"{name},{m['windows']},{m['avg_qos_error']:.4f},{m['avg_active_mrsus']:.2f},"
                    f"{m['avg_pdr']:.4f},{m['avg_coverage']:.4f},{cbr},{aw},{ext_aw},{zero_c},{n_v},{relay_t},"
                    f"{m['blackout_windows']},{m['Q_raw']:.4f},{m['Q']:.4f},{m['R']:.4f},{m['meets_quality_bar']},{eff}\n")
    print("\nSaved to results/resource_efficiency_report.csv")