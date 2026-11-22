#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <set>
using namespace std;

const double walk_speed = 1.4; 

class Edge {
public:

    int from; // index
    int to;   // index

    double length; // length of the edge
    double capacity;  // how much cars it can support
    double free_flow; // how long it takes for a car
    double volume;
    bool car;  // cars may use this edge
    bool walk; // people may walk this edge
    string name; // name of the road
};

class Node {
public:
    long long id; // Node name
    double latitude;// coordinates
    double longitude;
    vector <Edge> edges;//all the edges leaving the node

};

class Building {
    public:
        string name;
        double latitude;
        double longitude;

};


enum class Mode { WALK, CAR };

struct Settings {
    Mode mode;   // walking or driving, exactly one
    bool use_traffic;
    double traffic_multiplier;
    set<string> avoid;
};

class data_structure {
public:

    vector<Edge> path;
    vector < Node > nodes;
    vector < Building > buildings;
    map <long long, int> index_map;

    void osm_convert (string &filename);// function reads the osm file and creates a txt file to process
    void LoadMap (string &filename); // function reads the text file from OSM_convert and creates our map
    void shortest_path (long long node_1, long long node_2 ); // This function will run a dijkstras algorithim that will find the shortest path between two nodes
    double calc_weight(const Edge &edge, const Settings &s); // calculates the weight of an edge based on user settigns and traffic
    void PrintMap() const; // Print the created map to terminal
};