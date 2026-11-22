#include "Map_generator.hpp"

void data_structure::PrintMap() const
{
    const auto original_precision = cout.precision(17);

    for (const Node& node : nodes)
    {
        cout << "NODE " << node.id << ' '
             << node.latitude << ' ' << node.longitude << '\n';
    }

    for (const Node& node : nodes)
    {
        for (const Edge& edge : node.edges)
        {
            cout << "EDGE " << nodes[edge.from].id << ' '
                 << nodes[edge.to].id << ' '
                 << edge.length << ' '
                 << edge.capacity << ' '
                 << edge.free_flow << ' ';

            if (edge.car)
                cout << 'C';
            if (edge.walk)
                cout << 'W';

            if (!edge.name.empty())
                cout << ' ' << edge.name;

            cout << '\n';
        }
    }

    for (const Building& building : buildings)
    {
        cout << "BUILDING " << building.latitude << ' '
             << building.longitude << ' ' << building.name << '\n';
    }

    cout.precision(original_precision);
}