import sumolib
import random

random.seed(42)

net = sumolib.net.readNet("mrsu_bremen3.net.xml")

# tram route we already found
tram_edges = "1226344159#0 1226344159#1 251068032#3 251068032#4 251068032#6 251068032#7 251068032#8 251239165#0 251239165#2 251239165#4 251239165#5 251239165#6 251068051 1226344160#1 251068065 830297980#0 830297980#4 251068036#0 251068036#1 251068036#2 251068036#3 251068095#1 251068095#3 251068039 251068018#0 251068018#2 251068018#3 251068018#4 251068018#5 251068018#6 830297979 720059985#0 720059985#2 720059985#3 720059985#4 720059985#7 251068048#3 251068048#5 251068048#7 251068048#8 251068048#11 251068048#12 830298367#0 830298367#3 830298367#4 250700642#0 250700642#1 250700642#2 251068009#0 251068009#1 251068009#2 251068009#3 250700674#0 250700674#1 250700674#2 1347432181#1 250700652#0 250700652#2 250700652#3 250700652#7 469794462#0 469794462#1 469794462#2"
tram2_edges = "1336285075#3 251068033#0 251068033#1 251068033#2 251068033#3 251068033#6 251068033#7 251068033#8 251068056#0 251068056#1 251068056#4 251068056#6 839598348 251091503#0 251091503#4 687758002#0 687758002#1 839600171 251091479#0 251091479#3 251091479#4 251091501#0 251091501#1 251091501#2 251091501#3 251091501#4 251091501#5 251068035#0 251068035#1 251068035#4 251068035#5 251068035#6 251068035#7 251068035#8 251068035#10 251068035#11 251068035#12 1334803887#0 1334803887#1 251068043#1 251068043#3 251068040 251068093#0 251068093#1 1334803883 252091828#0 250700640#0 250700640#1 250700668 250700672#0 250700672#1 250700672#3 250700672#5 250700672#6 250700672#7 250700672#9 250700672#10 250700672#12 250700672#14 250700672#15 250700672#17 250700672#19 250700672#24 250700672#25 250700672#28 250700672#30 250700672#31 250700672#33 251068011#0 251068011#1 251068011#4 251068052 251068104#0 251068104#1 1334823187#1 251068082#0 251068082#1 251068082#2 251068082#3 251068082#4 251068082#5 251068082#6 251068082#7 251068082#8 251068082#9 251068082#10 251068010#0 251068010#2 251068010#3 251068010#4 251068010#5 252091839#0 252091839#3 252091839#4 251068106#0 251068106#1 251068106#2 251239174 251239196#0 251239196#2 251239196#3 4610623#0 4610623#1 4610623#2 4610623#5 4610623#6 4610623#7"
tram3_edges = "391173763#0 391173763#1 391173763#3 391173763#4 391173763#5 391173763#6 391173763#7 391173763#9 391173763#10 391173763#11 391173763#13 1334823184#0 1334823184#1 1334823184#5 1334823184#6 1334823184#7 1334823184#8 252091817#0 252091817#2 252091817#3 252091817#4 252091817#5 252091817#8 251068052 251068008#0 251068008#3 251068008#4 251068008#5 420642991#0 420642991#1 307507969 251068041 251068060 1336637279#0 1336637279#3 251068058 251068068#0 251068068#1 251068054 1349208964 251068123#3 251239181#0 251239181#1 251239181#2 251239181#4 251239181#5 251239181#6 23489068#3 420642989 251239197#2 251239177 251239173 1336285076#0 1336285076#1 251239193#0 251239193#1 251068070#0 251068070#3 251068061#0 251068061#1 251068031#7 251068031#8 251068031#9 472153576#0 472153576#1 472153576#2 251068077#0 251068077#1 251068077#2 251068077#3 251068077#4 251068077#5 251068077#6 251068077#7 251068077#8 251068077#9 251068077#10 472153575 251068119#0"
tram4_edges = "250700643#0 250700643#3 250700643#4 250700643#6 250700671#0 250700671#1 250700671#2 1226344162#1 469771907 250700644#0 250700644#1 250700644#2 295426302#3 295426302#4 251068053#0 251068053#1 251068053#2 1334803882 251068067#0 251068067#1 251068038 251068044#0 251068044#2 251068044#3 1334803885 251068025#0 251068025#1 251068025#3 251068025#4 251068025#5 251068025#6 251068025#7 251091504#0 251091504#1 251091504#2 251091504#3 251091504#4 251091504#5 890244160#0 890244160#1 890244160#2 251091502#0 251091502#1 251091502#2 251091502#3 251091502#6 251091502#7 1335561165 251091494 1335561166 251091481#0 251091481#1 251091481#4 839600172"
tram5_edges = "251068057#3 251068057#6 251091493#0 251091493#2 251091493#5 251091493#6 251091493#15 251091493#19 251068031#0 251068031#1 251068031#2 251068031#4 251068031#5 251068031#6 251068061#2 251068061#3 251068061#4 251068061#5 251068061#6 251068061#9 251068061#10 251068061#11 251068061#12 1334803887#0"
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

TOTAL_CARS = 130
TOTAL_BUSES = 10
TOTAL_NONV2X = 30
SIM_SPAN = 300

vehicles = []


# trams - five real routes, two trams per direction, staggered departures
vehicles.append((0.0, "tram_1", "tramType", "route_tram1", tram_edges))
vehicles.append((50.0, "tram_2", "tramType", "route_tram2", tram2_edges))
vehicles.append((100.0, "tram_3", "tramType", "route_tram3", tram3_edges))
vehicles.append((150.0, "tram_4", "tramType", "route_tram4", tram4_edges))
vehicles.append((200.0, "tram_5", "tramType", "route_tram5", tram5_edges))
vehicles.append((250.0, "tram_6", "tramType", "route_tram1", tram_edges))

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

with open("mrsu_bremen3.rou.xml", "w") as f:
    f.write('<routes>\n')
    f.write('    <vType id="tramType" vClass="tram" accel="1.0" decel="4.5" length="15" maxSpeed="10" color="1,0,0"/>\n')
    f.write('    <vType id="busType" vClass="bus" accel="1.2" decel="4.5" length="12" maxSpeed="13.9" color="1,1,0"/>\n')
    f.write('    <vType id="carType" vClass="passenger" accel="2.6" decel="4.5" length="5" maxSpeed="16.7" color="0,0,1"/>\n\n')

    f.write(f'    <route id="route_tram1" edges="{tram_edges}"/>\n')
    f.write(f'    <route id="route_tram2" edges="{tram2_edges}"/>\n')
    f.write(f'    <route id="route_tram3" edges="{tram3_edges}"/>\n')
    f.write(f'    <route id="route_tram4" edges="{tram4_edges}"/>\n')
    f.write(f'    <route id="route_tram5" edges="{tram5_edges}"/>\n')
    for route_id, edges in route_cache.items():
        f.write(f'    <route id="{route_id}" edges="{edges}"/>\n')
    f.write('\n')

    for entry in vehicles:
        depart, vid, vtype, route_id = entry[0], entry[1], entry[2], entry[3]
        f.write(f'    <vehicle id="{vid}" type="{vtype}" route="{route_id}" depart="{depart}"/>\n')

    f.write('</routes>\n')

print("Wrote mrsu_bremen3.rou.xml")