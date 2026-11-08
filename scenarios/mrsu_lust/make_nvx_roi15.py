import re, random, sys
import xml.etree.ElementTree as ET
SRC = "lust_roi15_cars.rou.xml"
DST = "lust_roi15_cars_nvx60.rou.xml"
NVX_SHARE = 0.40
txt = open(SRC).read()
if "nvx_" in txt:
    sys.exit("ABORT: source already contains nvx_ ids")
ids = re.findall(r'<vehicle id="([^"]+)"', txt)
k = round(NVX_SHARE * len(ids))
random.seed(42)
pick = set(random.sample(ids, k))
def sub(m):
    return '<vehicle id="nvx_' + m.group(1) + '"' if m.group(1) in pick else m.group(0)
new = re.sub(r'<vehicle id="([^"]+)"', sub, txt)
open(DST, "w").write(new)
root = ET.parse(DST).getroot()
vs = root.findall("vehicle")
nv = sum(1 for v in vs if v.get("id").startswith("nvx_"))
print("vehicles:", len(vs), " nvx_:", nv, " V2X:", len(vs) - nv, " V2X share: %.1f%%" % (100 * (len(vs) - nv) / len(vs)))
print("unique ids:", len({v.get("id") for v in vs}) == len(vs))
print("wrote", DST, "(original untouched)")
