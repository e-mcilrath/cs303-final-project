#include "Map_generator.hpp"
#include "LoadMap.cpp"
#include "PrintMap.cpp"
#include <set>

int main(){

    data_structure s;
    int node1, node2;
    string filename = "map.txt";
    set <string> path_names; 

    cout << "Enter two nodes: " << endl;
    cin >>  node1 >> node2; 


   s.LoadMap(filename); //read the data in and adjust our nodes vector
   s.shortest_path(node1, node2); //runs dijkstras algorithim to find the shortest path

   //Loop through path vector and print out all the different paths 

   
   for(size_t i =0; i< s.path.size(); i++) path_names.insert(s.path[i].name); //loop through the path vector and insert into our path names set




   

    return 1; 
}