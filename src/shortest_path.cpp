#include "Map_generator.hpp"
#include <climits>
#include <set>

void data_structure::shortest_path(long long node_1, long long node_2)
{ // This function will run a dijkstras algorithim that will find the shortest path between two nodes

    vector<double> distances;
    vector<Edge> parents;
    vector<int> visited;
    multimap<double, Edge> possible_paths; // weight, nodes index
    multimap<double, Edge>::iterator till;
    int current,next, start, goal; // current and goal index of nodes vector
    double weight;
    Node tmp;
    Edge edg;
    set <string> final_path;

    visited.resize(nodes.size(), 0);         // 0 = not visited 1 = visited
    parents.resize(nodes.size());        // store the parent of the visited node
    distances.resize(nodes.size(), INT_MAX); // shortest distance from node 1

    start = index_map.find(node_1)->second; // begin from node 1
    distances[start] = 0;
    goal = index_map.find(node_2)->second;    // ending index
    
    current = start;

    while (true)
    {
        visited[current] = 1; // declare node as visited

        if (current == goal) break; // base case if we have reached our final node return to print the final path

        tmp = nodes[current]; // grab the current node

        for (size_t i = 0; i < tmp.edges.size(); i++)
        { // loop through its adjacency list

            edg = tmp.edges[i]; // current edge

            weight = distances[current] + calc_weight(edg, s); // calc_weight takes in an edge and our settings parameters and calculates the weight

            if (weight <= distances[edg.to])
            { // if the weight we found is less then the weight we already have update the distances and parents vectors

                // update parents and distances vector
                distances[edg.to] = weight; // update distance from node 1 to the node we are looking at
                parents[edg.to] = edg;  // update it's parent to be the node we are currently at


                possible_paths.insert(make_pair(weight, edg)); // insert this path to possible paths
            }
        }
        next = current;
        for (till = possible_paths.begin(); till != possible_paths.end(); till++)
        { // loop through the possible paths from least to greatest distance from the starting node

            if (visited[till-> second.to] == 0)
            { // if we have not visited it

                next = till->second.to; // move to this node
                possible_paths.erase(till);
                break;
            }
        }
        if(next == current) break; //could not find a path 
        else current = next; 

    }

    if(current != goal) {
        cout << "No path found";
        return;
    }

    else {

        while(current != start){

            path.push_back(parents[current]); //push back current index
            current = parents[current].from;



        }
    }
}