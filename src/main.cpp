#include "Map_generator.hpp"
#include "LoadMap.cpp"
#include "PrintMap.cpp"

int main(){
    data_structure s;
    int node1, node2;
    string filename = "map.txt";


   s.LoadMap(filename); //read the data in and adjust our nodes vector

   

   s.shortest_path(node1, node2); //runs dijkstras algorithim to find the shortest path

    return 1; 
}