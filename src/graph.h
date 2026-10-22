/*
Eric McIlrath
graph.h

Road network for the UTK campus route planner. Intersections are nodes and
road segments are directed edges, stored as adjacency lists. A two-way road
is just two edges, one in each direction.

Units: length in meters, capacity in vehicles per hour, free flow time in
seconds, volume in vehicles per hour.
*/

#pragma once

#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace std;

class Edge {
  public:
    int to;               // index into Graph::nodes (not the node id)
    double length;
    double capacity;
    double free_flow;     // travel time with no traffic
    double volume;        // current traffic, set by the traffic simulation
    string name;          // road name, can be empty

    double congestion() const;   // volume / capacity
};

class Node {
  public:
    long long id;         // long long because OSM node ids get really big
    double lat;
    double lon;
    vector <string> buildings;
    vector <Edge> adj;    // outgoing edges
};

class Graph {
  public:
    /* Reads a graph in our text format (see data/test_small.txt). Throws a
       runtime_error with the line number if anything in the file is bad. */
    void read(istream &in);

    /* Returns the index of the node with this id, or -1 if there isn't one. */
    int find_node(long long id) const;

    int add_node(long long id, double lat, double lon);
    void add_edge(int from, int to, double length, double capacity,
                  double free_flow, const string &name);
    void add_building(int node, const string &building);

    int num_edges() const;
    void print() const;

    vector <Node> nodes;

  protected:
    map <long long, int> index;   // node id -> index in nodes
};
