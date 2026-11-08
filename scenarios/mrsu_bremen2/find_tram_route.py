import sumolib

net = sumolib.net.readNet("osm.net.xml")

# collect all edges that allow tram
tram_edges = [e for e in net.getEdges() if e.allows("tram")]
print(f"Total tram-allowed edges: {len(tram_edges)}")

# build a simple graph: edge_id -> edge object, and connectivity via from/to nodes
tram_edge_ids = set(e.getID() for e in tram_edges)

# find connected components using node-based traversal
visited = set()
components = []

for edge in tram_edges:
    if edge.getID() in visited:
        continue
    # BFS from this edge's start node, following only tram edges
    component = []
    stack = [edge]
    while stack:
        e = stack.pop()
        if e.getID() in visited:
            continue
        visited.add(e.getID())
        component.append(e)
        # explore outgoing edges from e's target node that are also tram edges
        for out_edge in e.getToNode().getOutgoing():
            if out_edge.getID() in tram_edge_ids and out_edge.getID() not in visited:
                stack.append(out_edge)
        for in_edge in e.getFromNode().getIncoming():
            if in_edge.getID() in tram_edge_ids and in_edge.getID() not in visited:
                stack.append(in_edge)
    components.append(component)

components.sort(key=len, reverse=True)
print(f"Found {len(components)} separate tram line segments")
for i, comp in enumerate(components[:5]):
    total_length = sum(e.getLength() for e in comp)
    print(f"  Component {i}: {len(comp)} edges, ~{total_length:.0f}m total length")

# print the edge IDs of the largest component, in a rough traversal order
largest = components[0]
print("\nLargest component edge IDs:")
for e in largest:
    print(f"  {e.getID()}  ({e.getFromNode().getID()} -> {e.getToNode().getID()}, {e.getLength():.0f}m)")

# Pick the two most "extreme" edges (by shape) to get a long end-to-end route
start_edge = largest[0]
end_edge = None
max_dist = 0

start_pos = start_edge.getShape()[0]
for e in largest:
    end_pos = e.getShape()[-1]
    dist = ((end_pos[0]-start_pos[0])**2 + (end_pos[1]-start_pos[1])**2) ** 0.5
    if dist > max_dist:
        max_dist = dist
        end_edge = e

print(f"\nAttempting route from {start_edge.getID()} to {end_edge.getID()} (approx {max_dist:.0f}m apart)")

path, cost = net.getShortestPath(start_edge, end_edge, vClass="tram")

if path:
    print(f"Route found: {len(path)} edges, cost={cost:.0f}")
    print("Edge sequence:")
    print(" ".join(e.getID() for e in path))
else:
    print("No route found between these two edges")