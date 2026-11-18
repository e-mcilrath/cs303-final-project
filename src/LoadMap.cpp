#include "Map_generator.hpp"
#include <fstream>
#include <sstream>

using namespace std;

void data_structure::LoadMap(string &filename)
{

    string line, first, direction1,direction2 ,name, CW;
    ifstream fin(filename); // open the file
    long long id,  from, to;
    double lat, lon,length, capacity, free_flow; 
     
    Node node;
    Edge edge;
    Building building;

    while (getline(fin, line))
    {
        istringstream ss(line);

        ss >> first;

        if (first == "#")
            continue; // ignoring unnessesary lines

        else if (first == "NODE")
        { // if it is a node

            // read the data in and store it in a node object. Then store it in our map of nodes
            ss >> id >> lat >> lon;

            // create a node
            node.id = id;
            node.latitude = lat;
            node.longitude = lon;

            // insert into the map
            nodes.push_back(node);
            index_map.insert(make_pair(id, nodes.size()-1)); //nodes.size()-1 == current nodes index in the vector
        }

        else if (first == "EDGE")
        { // repeat the same process we did for nodes
            //from_id to_id length_m capacity_veh_per_hr free_flow_s mode [name]
            ss >> from >> to >> length >> capacity >> free_flow >> CW >> direction1 >> direction2 >> name;

            edge.from = index_map.find(from) -> second;
            edge.to = index_map.find(to) -> second; 
            edge.length = length; 
            edge.capacity = capacity; 
            edge.free_flow = free_flow;
            
            //determine if the path is driveable or walkable
            if(CW.size() == 2) {
                edge.car = true; 
                edge.walk = true; 
            }
            else if(CW == "W"){
                edge.car = false;
                edge.walk = true;
            }
            else if(CW == "C"){
                edge.car = true; 
                edge.walk = false;
            }

            //TODO read in the name
            name = direction + name;
            edge.name = name; 
            nodes[edge.from].edges.push_back(edge);


           
        }

        else if (first == "BUILDING")
        { // repeat the same process we did for nodes
            //lat lon name
            ss >> lat >> lon >> direction >> name; 

            building.latitude = lat; 
            building.longitude = lon;
            building.name = direction + name; //TODO read in the name properly
            


        }
    }

    fin.close();
}