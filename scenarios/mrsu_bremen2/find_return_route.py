import sumolib
import itertools

net = sumolib.net.readNet("mrsu_bremen2.net.xml")

tram_edges = [e for e in net.getEdges() if e.allows("tram")]
print(f"Total tram edges available: {len(tram_edges)}")

# try several candidate pairs, looking for the longest valid route
# that's meaningfully different from the original tram_1 route
original_route = set("1305459766#0 1305459766#1 1305459766#2 1334228333#0 1334228333#1 1334228333#2 1305459767#1 1305459767#2 1334228331#1 251393901#0 251393901#1 251393901#4 251393901#6 251393901#9 251393901#10 251393901#14 251393901#15".split())

best = None
best_len = 0

# sample a reasonable number of pairs rather than all combinations (could be huge)
import random
random.seed(1)
sample_edges = random.sample(tram_edges, min(15, len(tram_edges)))

for start, end in itertools.permutations(sample_edges, 2):
    path, cost = net.getShortestPath(start, end, vClass="tram")
    if path and len(path) >= 5:
        edge_ids = set(e.getID() for e in path)
        overlap = len(edge_ids & original_route) / len(edge_ids)
        if overlap < 0.5 and len(path) > best_len:
            best = (start.getID(), end.getID(), path, cost)
            best_len = len(path)

if best:
    start_id, end_id, path, cost = best
    print(f"\nBest alternate route: {start_id} -> {end_id}")
    print(f"{len(path)} edges, cost={cost:.0f}")
    print("Edge sequence:")
    print(" ".join(e.getID() for e in path))
else:
    print("\nNo good alternate route found with low overlap")