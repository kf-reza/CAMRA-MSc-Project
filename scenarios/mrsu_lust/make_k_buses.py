import re, random, sys
import xml.etree.ElementTree as ET
SRC = "lust_roi15_buses.rou.xml"
DST = "lust_roi15_buses_k21.rou.xml"
AREA_KM2 = (10396.53 - 6505.05) * (10327.51 - 6464.77) / 1e6
K = round(14 / 10.25 * AREA_KM2)
txt = open(SRC).read()
if "nvx_" in txt:
    sys.exit("ABORT: source already has nvx_ ids")
ids = re.findall(r'<vehicle id="(mrsu_bus_[^"]+)"', txt)
if len(ids) != 39:
    sys.exit("ABORT: expected 39 buses, found %d" % len(ids))
random.seed(42)
keep = set(random.sample(ids, K))
def sub(m):
    if m.group(1) in keep:
        return m.group(0)
    return '<vehicle id="nvx_bus_' + m.group(1)[len("mrsu_bus_"):] + '"'
new = re.sub(r'<vehicle id="(mrsu_bus_[^"]+)"', sub, txt)
open(DST, "w").write(new)
vs = ET.parse(DST).getroot().findall("vehicle")
n_m = sum(1 for v in vs if "mrsu" in v.get("id"))
n_x = sum(1 for v in vs if v.get("id").startswith("nvx_"))
print("area %.2f km2, K=%d | buses: %d  candidates(mrsu): %d  silent(nvx_): %d  unique ids: %s" % (AREA_KM2, K, len(vs), n_m, n_x, len({v.get("id") for v in vs}) == len(vs)))
