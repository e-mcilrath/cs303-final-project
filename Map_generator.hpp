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
    unordered_map<string, string> tags; // tag type , tag name
};

class Way {
public:
    long long id; // pathway name
    vector<long long> nodes; // vector of all nodes in order
    unordered_map<string, string> tags; // tag type, tag name
};

class Map {
public:
    unordered_map<long long, Node> nodes; 
    unordered_map<long long, Way> ways;

    void OSM_convert (string &filename);// function reads the osm file and creates a txt file to process
    void LoadMap (string &filename); // function reads the text file from OSM_convert and creates our map
    void shortes_path (long long node_1, long long node_2 ); // This function will run a dijkstras algorithim that will find the shortest path between two nodes
};