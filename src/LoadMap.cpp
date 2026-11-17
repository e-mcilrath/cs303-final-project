#include "Map_generator.hpp"
#include <fstream>
#include <sstream>

using namespace std; 

void Map::LoadMap(string &filename) {

    string line, first; 
    ifstream fin (filename); // open the file
    long long id, lat, lon;
    Node node; 

    while (getline(cin,line)){
       istringstream ss(line);
       
       ss>> first; 

       if(first == "#" ) continue; //ignoring unnessesary lines

       else if(first == "NODE"){ //if it is a node
        
        //read the data in and store it in a node object. Then store it in our map of nodes
        ss >> id >> lat >> lon;
        
        //create a node
        node.id = id; 
        node.latitude = lat;
        node.longitude = lon; 

        //insert into the map
        nodes.insert(make_pair(id, node));
    
       }

       else if( first == "EDGE"){ //repeat the same process we did for nodes 
        

       }

       else if(first == "BUILDING"){ //repeat the same process we did for nodes 

       }

    }
    



    fin.close();
}