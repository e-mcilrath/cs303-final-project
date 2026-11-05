/*
Eric McIlrath
graph.h

Road and walkway network for the UTK campus route planner. Intersections are
nodes and road/path segments are directed edges, stored as adjacency lists.
A two-way road is just two edges, one in each direction.

Each edge says who can use it: cars, walkers, or both. A one-way road is a
car+walk edge one way and a walk-only edge back, since people can walk
either direction on it.

Units: length in meters, capacity in vehicles per hour, free flow time in
seconds, volume in vehicles per hour. Walk-only edges have capacity and
free flow time 0, since they carry no cars.
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
    double capacity;      // 0 on walk-only edges
    double free_flow;     // car travel time with no traffic, 0 on walk-only edges
    double volume;        // current traffic, set by the traffic simulation
    bool car;             // cars may use this edge
    bool walk;            // people may walk this edge
    string name;          // road name, can be empty

    double congestion() const;   // volume / capacity, 0 on walk-only edges
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
    /* car edges need capacity > 0 and free_flow > 0; walk-only edges need
       both to be 0. At least one of car / walk has to be true. */
    void add_edge(int from, int to, double length, double capacity,
                  double free_flow, bool car, bool walk, const string &name);
    void add_building(int node, const string &building);

    int num_edges() const;
    void print() const;

    vector <Node> nodes;

  protected:
    map <long long, int> index;   // node id -> index in nodes
};
