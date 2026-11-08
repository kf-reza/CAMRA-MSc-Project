import sumolib
import random

random.seed(42)

net = sumolib.net.readNet("mrsu_bremen2.net.xml")

# tram route we already found
tram_edges = "1305459766#0 1305459766#1 1305459766#2 1334228333#0 1334228333#1 1334228333#2 1305459767#1 1305459767#2 1334228331#1 251393901#0 251393901#1 251393901#4 251393901#6 251393901#9 251393901#10 251393901#14 251393901#15"
tram2_edges = "251393900#6 251393900#9 251393900#11 251393900#13 251393900#14 251393900#15 1334228330#0 1334228330#1 1305459769#2 1334228335#1 1334228335#2 1305459768#0 1305459768#1"

# collect drivable edges (cars/buses) - normal roads, reasonable length, not internal junction edges
car_edges = [e for e in net.getEdges() if e.allows("passenger") and not e.isSpecial() and e.getLength() > 20]
print(f"Usable car/bus edges: {len(car_edges)}")

def find_valid_route(max_attempts=20):
    for _ in range(max_attempts):
        start = random.choice(car_edges)
        end = random.choice(car_edges)
        if start == end:
            continue
        path, cost = net.getShortestPath(start, end, vClass="passenger")
        if path and 3 <= len(path) <= 30:  # reasonable route length, not too short/long
            return " ".join(e.getID() for e in path)
    return None

TOTAL_CARS = 80
TOTAL_BUSES = 16
TOTAL_NONV2X = 20
SIM_SPAN = 300

vehicles = []


# trams - two real routes, two trams per direction, staggered departures
vehicles.append((0.0, "tram_1", "tramType", "route_tram1", tram_edges))
vehicles.append((30.0, "tram_2", "tramType", "route_tram2", tram2_edges))
vehicles.append((150.0, "tram_3", "tramType", "route_tram1", tram_edges))
vehicles.append((180.0, "tram_4", "tramType", "route_tram2", tram2_edges))

route_cache = {}
def get_or_make_route(prefix, index):
    route_str = find_valid_route()
    if route_str is None:
        return None
    route_id = f"route_{prefix}_{index}"
    route_cache[route_id] = route_str
    return route_id

count_ok = 0
for i in range(TOTAL_CARS):
    depart = round(random.uniform(1, SIM_SPAN), 2)
    route_id = get_or_make_route("car", i)
    if route_id:
        vehicles.append((depart, f"car_{i+1}", "carType", route_id, None))
        count_ok += 1

for i in range(TOTAL_NONV2X):
    depart = round(random.uniform(1, SIM_SPAN), 2)
    route_id = get_or_make_route("nvx", i)
    if route_id:
        vehicles.append((depart, f"nvx_{i+1}", "carType", route_id, None))
        count_ok += 1

for i in range(TOTAL_BUSES):
    depart = round(random.uniform(1, SIM_SPAN), 2)
    route_id = get_or_make_route("bus", i)
    if route_id:
        vehicles.append((depart, f"bus_{i+1}", "busType", route_id, None))
        count_ok += 1

print(f"Successfully generated {count_ok} car/bus routes")

vehicles.sort(key=lambda v: v[0])

with open("mrsu_bremen2.rou.xml", "w") as f:
    f.write('<routes>\n')
    f.write('    <vType id="tramType" vClass="tram" accel="1.0" decel="4.5" length="15" maxSpeed="10" color="1,0,0"/>\n')
    f.write('    <vType id="busType" vClass="bus" accel="1.2" decel="4.5" length="12" maxSpeed="13.9" color="1,1,0"/>\n')
    f.write('    <vType id="carType" vClass="passenger" accel="2.6" decel="4.5" length="5" maxSpeed="16.7" color="0,0,1"/>\n\n')

    f.write(f'    <route id="route_tram1" edges="{tram_edges}"/>\n')
    f.write(f'    <route id="route_tram2" edges="{tram2_edges}"/>\n')
    for route_id, edges in route_cache.items():
        f.write(f'    <route id="{route_id}" edges="{edges}"/>\n')
    f.write('\n')

    for entry in vehicles:
        depart, vid, vtype, route_id = entry[0], entry[1], entry[2], entry[3]
        f.write(f'    <vehicle id="{vid}" type="{vtype}" route="{route_id}" depart="{depart}"/>\n')

    f.write('</routes>\n')

print("Wrote mrsu_bremen2.rou.xml")