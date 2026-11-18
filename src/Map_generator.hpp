#include <iostream>
#include <vector>
#include <map>
#include <string>
using namespace std;

class Node {
public:
    long long id; // Node name
    double latitude;// coordinates
    double longitude;
    vector <Edge> edges;//all the edges leaving the node
     
};

class Edge {
public:

    int from; // index
    int to;   // index

    double length; // length of the edge
    double capacity;  // how much cars it can support
    double free_flow; // how long it takes for a car
    double volume; 
    bool mode; //true = car and walk false = just walk
    string name; // name of the road
};

class Building {
    public:
        string name;
        long long latitude; 
        long long longitude;

};

class data_structure {
public:

    vector < Node > nodes;
    vector < Building > buildings;
    map <long long, int> index_map;
    string mode; 

    void osm_convert (string &filename);// function reads the osm file and creates a txt file to process
    void LoadMap (string &filename); // function reads the text file from OSM_convert and creates our map
    void shortest_path (long long node_1, long long node_2 ); // This function will run a dijkstras algorithim that will find the shortest path between two nodes
    double  calc_weight(Edge, string CW);

};