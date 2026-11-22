#include "Map_generator.hpp"
#include "LoadMap.cpp"
#include "PrintMap.cpp"

int main(){
    data_structure s;
    int node1, node2;
    string filename = "example_campus.txt";

   s.LoadMap(filename); //read the data in and adjust our nodes vector

   //TODO find a way to read in the two nodes we want from our website
   node1 = 1; 
   node2 = 9;

   s.shortest_path(node1, node2); //runs dijkstras algorithim to find the shortest path

    return 1; 
}