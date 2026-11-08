"""
CAMRA vs. Always-Active — Active MRSU Position Heatmap over Real Bremen Map
============================================================================
Run this on your own machine (needs internet access for map tiles).

Setup (one-time):
    pip install pandas numpy pyproj matplotlib contextily --break-system-packages

Usage:
    Place this script in the same folder as your two CSV files:
        active_mrsu_positions_camra.csv
        active_mrsu_positions_allactive.csv
    Then run:
        python3 make_heatmap_basemap.py
"""

import pandas as pd
import numpy as np
from pyproj import Transformer
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import contextily as cx

# ---- SUMO network projection info (from mrsu_bremen3.net.xml) ----
NET_OFFSET_X = -485009.54
NET_OFFSET_Y = -5879288.38
UTM_ZONE = 32

transformer = Transformer.from_crs(f"EPSG:326{UTM_ZONE}", "EPSG:4326", always_xy=True)
to_webmercator = Transformer.from_crs("EPSG:4326", "EPSG:3857", always_xy=True)

def sumo_xy_to_latlon(df):
    utm_x = df["x"] - NET_OFFSET_X
    utm_y = df["y"] - NET_OFFSET_Y
    lon, lat = transformer.transform(utm_x.values, utm_y.values)
    df["lon"] = lon
    df["lat"] = lat
    x3857, y3857 = to_webmercator.transform(lon, lat)
    df["x3857"] = x3857
    df["y3857"] = y3857
    return df

def load(path):
    df = pd.read_csv(path)
    df.columns = [c.strip() for c in df.columns]
    return sumo_xy_to_latlon(df)

print("Loading CSVs...")
camra = load("active_mrsu_positions_camra.csv")
allactive = load("active_mrsu_positions_allactive.csv")
print(f"  CAMRA: {len(camra)} samples")
print(f"  All-Active: {len(allactive)} samples")

# shared bounds so both maps use the identical zoom/extent for fair comparison
x_min = min(camra.x3857.min(), allactive.x3857.min())
x_max = max(camra.x3857.max(), allactive.x3857.max())
y_min = min(camra.y3857.min(), allactive.y3857.min())
y_max = max(camra.y3857.max(), allactive.y3857.max())
pad_x = (x_max - x_min) * 0.08
pad_y = (y_max - y_min) * 0.08

print("Rendering heatmaps with basemap (requires internet)...")
fig, axes = plt.subplots(1, 2, figsize=(16, 8))

for ax, df, title in [(axes[0], camra, "CAMRA"), (axes[1], allactive, "Always-Active")]:
    hb = ax.hexbin(df.x3857, df.y3857, gridsize=40, cmap="hot", mincnt=1, alpha=0.65,
                    extent=[x_min - pad_x, x_max + pad_x, y_min - pad_y, y_max + pad_y])
    ax.set_xlim(x_min - pad_x, x_max + pad_x)
    ax.set_ylim(y_min - pad_y, y_max + pad_y)
    cx.add_basemap(ax, source=cx.providers.CartoDB.Positron, zoom=15)
    ax.set_title(f"{title}\n(active MRSU position density, {len(df)} samples)")
    ax.set_axis_off()
    fig.colorbar(hb, ax=ax, label="Active MRSU samples", shrink=0.7)

plt.tight_layout()
plt.savefig("heatmap_comparison_basemap.png", dpi=170, bbox_inches="tight")
print("Saved heatmap_comparison_basemap.png")

# Also save individual full-size versions, useful for presentation slides
for df, title, fname in [(camra, "CAMRA", "heatmap_camra_basemap.png"),
                           (allactive, "Always-Active", "heatmap_allactive_basemap.png")]:
    fig2, ax2 = plt.subplots(figsize=(9, 9))
    hb2 = ax2.hexbin(df.x3857, df.y3857, gridsize=45, cmap="hot", mincnt=1, alpha=0.65,
                       extent=[x_min - pad_x, x_max + pad_x, y_min - pad_y, y_max + pad_y])
    ax2.set_xlim(x_min - pad_x, x_max + pad_x)
    ax2.set_ylim(y_min - pad_y, y_max + pad_y)
    cx.add_basemap(ax2, source=cx.providers.CartoDB.Positron, zoom=15)
    ax2.set_title(f"{title} — Active MRSU Position Density\n({len(df)} samples, mrsu_bremen3)")
    ax2.set_axis_off()
    fig2.colorbar(hb2, ax=ax2, label="Active MRSU samples", shrink=0.7)
    plt.tight_layout()
    plt.savefig(fname, dpi=170, bbox_inches="tight")
    print(f"Saved {fname}")

# Export lat/lon CSVs too, useful for Google My Maps / QGIS if you want an interactive version
camra[["time", "mrsu_id", "lat", "lon"]].to_csv("active_mrsu_positions_camra_latlon.csv", index=False)
allactive[["time", "mrsu_id", "lat", "lon"]].to_csv("active_mrsu_positions_allactive_latlon.csv", index=False)
print("Saved lat/lon CSVs for optional use in QGIS / Google My Maps")

print("\nDone.")
