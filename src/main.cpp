#include "Map_generator.hpp"
#include <cmath>
#include <limits>
#include <set>
using namespace std; 



long long find_closest_node(string building_name, data_structure& campus_map)
{
    size_t building_index = 0;

    for (; building_index < campus_map.buildings.size(); building_index++) {
        if (campus_map.buildings[building_index].name == building_name) {
            break;
        }
    }

    if (building_index == campus_map.buildings.size()) {
        cout << "Building not found: " << building_name << '\n';
        return -1;
    }

    if (campus_map.nodes.empty()) {
        cout << "No map nodes found.\n";
        return -1;
    }

    Building& building = campus_map.buildings[building_index];
    long long closest_id = -1;
    double best_distance = numeric_limits<double>::infinity();
    double longitude_scale = cos(building.latitude * 3.141592653589793 / 180.0);

    for (size_t i = 0; i < campus_map.nodes.size(); i++) {
        Node& node = campus_map.nodes[i];
        double dlat = node.latitude - building.latitude;
        double dlon = (node.longitude - building.longitude) * longitude_scale;
        double distance_squared = dlat * dlat + dlon * dlon;

        if (distance_squared < best_distance) {
            best_distance = distance_squared;
            closest_id = node.id;
        }
    }

    return closest_id;
}

int main()
{
    data_structure campus_map;
    string filename = "data/map.txt";
    campus_map.LoadMap(filename);
    campus_map.s = Settings{Mode::WALK, false, 1.0, {}};

    string building1, building2;

    cout << "Enter starting building: ";
    if (!getline(cin, building1)) {
        return 1;
    }

    cout << "Enter ending building: ";
    if (!getline(cin, building2)) {
        return 1;
    }

    long long node1 = find_closest_node(building1, campus_map);
    long long node2 = find_closest_node(building2, campus_map);

    if (node1 == -1 || node2 == -1) {
        return 1;
    }

    cout << '\n' << building1 << " -> " << building2 << '\n';

    if (node1 == node2) {
        cout << "Both buildings use the same map node.\n";
        return 0;
    }

    campus_map.shortest_path(node1, node2);

    if (campus_map.path.empty()) {
        cout << "No walking route found.\n";
        return 0;
    }

    string previous_name;
    size_t step = 0;

    for (size_t i = 0; i < campus_map.path.size(); i++) {
        string name = campus_map.path[i].name;

        if (name.empty()) {
            name = "Unnamed walkway";
        }

        if (name != previous_name) {
            step++;
            cout << "  " << step << ". " << name << '\n';
            previous_name = name;
        }
    }

    return 0;
}