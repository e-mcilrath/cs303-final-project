#include "Map_generator.hpp"
#include <fstream>
#include <sstream>

using namespace std;

void data_structure::LoadMap(string &filename)
{

    string line, first, name, CW;
    ifstream fin(filename); // open the file
    long long id, from, to;
    double lat, lon, length, capacity, free_flow; // lat/lon have decimals, so double
     
    Node node;
    Edge edge;
    Building building;

    while (getline(fin, line))
    {
        istringstream ss(line);

        // skip blank lines. Without this, first keeps the last line's type
        // and the last node or edge gets added again
        if (!(ss >> first))
            continue;

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
            ss >> from >> to >> length >> capacity >> free_flow >> CW;


            edge.from = index_map.find(from) -> second; //find the index for the from node
            edge.to = index_map.find(to) -> second;  // find the index it goes to
            edge.length = length; 
            edge.capacity = capacity; 
            edge.free_flow = free_flow;
            edge.volume = 0; // no traffic until the traffic simulation sets it
            
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

            // the name is the rest of the line and can have spaces.
            // clear it first so an unnamed edge doesn't keep the last name
            name.clear();
            getline(ss >> ws, name);
            edge.name = name;

            nodes[edge.from].edges.push_back(edge);


           
        }

        else if (first == "BUILDING")
        { // repeat the same process we did for nodes
            //lat lon name
            ss >> lat >> lon;

            // the name is the rest of the line and can have spaces
            name.clear();
            getline(ss >> ws, name);

            building.latitude = lat;
            building.longitude = lon;
            building.name = name;

            buildings.push_back(building);
            


        }
    }

    fin.close();
}