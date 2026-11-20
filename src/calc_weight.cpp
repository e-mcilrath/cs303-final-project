#include "Map_generator.hpp"
#include <cmath>

double data_structure:: calc_weight(const Edge &edge, const Settings &s){

    if (s.mode == Mode::CAR && !edge.car) return INFINITY; // if Settings chooses car, check if edge allows
    if (!edge.name.empty() && s.avoid.count(edge.name)) return INFINITY; // check if edge is in Settings avoid set

    if (s.mode == Mode::WALK) return edge.length / walk_speed; // regular walking
    if (s.use_traffic) {
        return edge.free_flow * (1 + 0.15 * pow(edge.volume / edge.capacity, 4)); // car, with traffic
    }
    return edge.free_flow; // car, without traffic
}