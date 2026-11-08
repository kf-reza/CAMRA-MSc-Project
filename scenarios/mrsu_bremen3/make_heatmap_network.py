import xml.etree.ElementTree as ET
import pandas as pd
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.collections import LineCollection

NET_FILE = "mrsu_bremen3.net.xml"
CAMRA_CSV = "active_mrsu_positions_camra.csv"
ALLACTIVE_CSV = "active_mrsu_positions_allactive.csv"

print("Parsing network geometry (this may take a moment for a large file)...")
tree = ET.parse(NET_FILE)
root = tree.getroot()

segments = []       # regular road edges
tram_segments = []  # edges that allow tram (drawn distinctly)

for edge in root.iter("edge"):
    edge_id = edge.get("id", "")
    if edge_id.startswith(":"):
        continue  # skip internal junction edges
    for lane in edge.findall("lane"):
        shape = lane.get("shape")
        allow = lane.get("allow", "")
        disallow = lane.get("disallow", "")
        if not shape:
            continue
        pts = [tuple(map(float, p.split(","))) for p in shape.split(" ") if p]
        if len(pts) < 2:
            continue
        is_tram = "tram" in allow or ("tram" not in disallow and "rail" in edge.get("type", ""))
        if "tram" in allow:
            tram_segments.append(pts)
        else:
            segments.append(pts)
        break  # one lane's shape per edge is enough for a clean visual

print(f"  {len(segments)} road segments, {len(tram_segments)} tram segments")

def load_positions(path):
    df = pd.read_csv(path)
    df.columns = [c.strip() for c in df.columns]
    return df

camra = load_positions(CAMRA_CSV)
allactive = load_positions(ALLACTIVE_CSV)
print(f"CAMRA: {len(camra)} samples | All-Active: {len(allactive)} samples")

x_min = min(camra.x.min(), allactive.x.min()) - 100
x_max = max(camra.x.max(), allactive.x.max()) + 100
y_min = min(camra.y.min(), allactive.y.min()) - 100
y_max = max(camra.y.max(), allactive.y.max()) + 100

def draw_network(ax):
    road_lc = LineCollection(segments, colors="#cccccc", linewidths=0.4, zorder=1)
    ax.add_collection(road_lc)
    if tram_segments:
        tram_lc = LineCollection(tram_segments, colors="#888888", linewidths=1.1, zorder=2)
        ax.add_collection(tram_lc)

fig, axes = plt.subplots(1, 2, figsize=(16, 8))

for ax, df, title in [(axes[0], camra, "CAMRA"), (axes[1], allactive, "Always-Active")]:
    draw_network(ax)
    hb = ax.hexbin(df.x, df.y, gridsize=45, cmap="hot", mincnt=1, alpha=0.85,
                    extent=[x_min, x_max, y_min, y_max], zorder=3)
    ax.set_xlim(x_min, x_max)
    ax.set_ylim(y_min, y_max)
    ax.set_aspect("equal")
    ax.set_title(f"{title}\n(active MRSU position density, {len(df)} samples)")
    ax.set_xlabel("SUMO x (m)")
    ax.set_ylabel("SUMO y (m)")
    fig.colorbar(hb, ax=ax, label="Active MRSU samples", shrink=0.75)

plt.suptitle("Active MRSU Coverage over Real Bremen Road/Tram Network (mrsu_bremen3)", fontsize=13)
plt.tight_layout()
plt.savefig("heatmap_comparison_network.png", dpi=170, bbox_inches="tight")
print("Saved heatmap_comparison_network.png")

# Individual full-size versions for slides
for df, title, fname in [(camra, "CAMRA", "heatmap_camra_network.png"),
                           (allactive, "Always-Active", "heatmap_allactive_network.png")]:
    fig2, ax2 = plt.subplots(figsize=(9, 9))
    draw_network(ax2)
    hb2 = ax2.hexbin(df.x, df.y, gridsize=50, cmap="hot", mincnt=1, alpha=0.85,
                       extent=[x_min, x_max, y_min, y_max], zorder=3)
    ax2.set_xlim(x_min, x_max)
    ax2.set_ylim(y_min, y_max)
    ax2.set_aspect("equal")
    ax2.set_title(f"{title} — Active MRSU Position Density\n({len(df)} samples, mrsu_bremen3, real road network)")
    ax2.set_xlabel("SUMO x (m)")
    ax2.set_ylabel("SUMO y (m)")
    fig2.colorbar(hb2, ax=ax2, label="Active MRSU samples", shrink=0.75)
    plt.tight_layout()
    plt.savefig(fname, dpi=170, bbox_inches="tight")
    print(f"Saved {fname}")

print("\nDone.")
