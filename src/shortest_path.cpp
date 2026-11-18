#include "Map_generator.hpp"
#include <climits> 

    void data_structure :: shortest_path (long long node_1, long long node_2 ){// This function will run a dijkstras algorithim that will find the shortest path between two nodes

        vector <int> distances; 
        vector <int> parents; 
        vector <int> visited; 
        int current, goal; //current index of nodes vector
        double weight;
        Node tmp;
        Edge edg;
        

        
        visited.resize(nodes.size(),0); // 0 = not visited 1 = visited
        parents.resize(nodes.size(),-1); // store the parent of the visited node
        distances.resize(nodes.size(),INT_MAX); // shortest distance from node 1


        current = index_map.find(node_1)->second;// begin from node 1
        goal = index_map.find(node_2)->second;//ending index

        while(true){

            if (current == goal) break; //base case if we have reached our final node return to print the final path

            tmp = nodes[current];//grab the current node

            for(size_t i = 0; i< tmp.edges.size(); i++){ // loop through its adjacency list
                
                edg = tmp.edges[i]; // current edge

                weight = calc_weight(edg, s); // calc_weight takes in an edge and our settings parameters and calculates the weight

                if (weight <= distances[edg.to]){ //if the weight we found is less then the weight we already have update the weight and parent

                    //update parents and distances vector
                    distances[edg.to] = weight; 
                    parents[edg.to] = weight; 

                    






                }






            }



            










        }


        
    } 