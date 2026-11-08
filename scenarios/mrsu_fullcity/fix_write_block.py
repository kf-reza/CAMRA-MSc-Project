with open("generate_fullcity_routes.py", "r") as f:
    lines = f.readlines()

# Find the anchor points: right after the carType vType line, and right before the route_cache loop
start_idx = None
end_idx = None
for i, line in enumerate(lines):
    if 'carType' in line and 'vType' in line:
        start_idx = i + 1
    if 'for route_id, edges in route_cache.items():' in line:
        end_idx = i
        break

if start_idx is None or end_idx is None:
    print("ERROR: anchors not found, aborting.")
else:
    new_block = ["\n"]
    for i in range(1, 9):
        suffix = "" if i == 1 else str(i)
        new_block.append(f'    f.write(f\'    <route id="route_tram{i}" edges="{{tram{suffix}_edges}}"/>\\n\')\n')

    lines[start_idx:end_idx] = new_block

    with open("generate_fullcity_routes.py", "w") as f:
        f.writelines(lines)
    print(f"Replaced lines {start_idx} to {end_idx} with clean 8-line block.")