#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
using namespace std;

class Node {
public:
    long long id; // Node name
    double latitude;// coordinates
    double longitude;
};

class Edges {
public:
    long long id; // pathway name
    long long from; // current node
    long long to;  // next node 
    double length; // length of the edge
    double capacity;  // how much cars it can support
    double free_flow; // how long it takes for a car 
    string mode; // determines if it is a walking path or a driving path
    string name; // name of the road
    
};

class Map {
public:
    unordered_map<long long, Node> nodes; 
    unordered_map<long long, Edges> ways;

    void osm_convert (string &filename);// function reads the osm file and creates a txt file to process
    void LoadMap (string &filename); // function reads the text file from OSM_convert and creates our map
    void shortest_path (long long node_1, long long node_2 ); // This function will run a dijkstras algorithim that will find the shortest path between two nodes

};