/*
Eric McIlrath
graph_test.cpp

Reads a graph file, builds the adjacency lists, and prints every node with
its outgoing edges so I can check the graph got loaded right.

usage: graph_test graph-file
*/

#include <cstdio>
#include <fstream>
#include <stdexcept>

#include "graph.h"

using namespace std;

int main(int argc, char **argv)
{
  Graph g;
  ifstream fin;

  if (argc != 2) {
    fprintf(stderr, "usage: graph_test graph-file\n");
    return 1;
  }

  fin.open(argv[1]);
  if (!fin.is_open()) {
    fprintf(stderr, "couldn't open %s\n", argv[1]);
    return 1;
  }

  try {
    g.read(fin);
  } catch (const runtime_error &e) {
    fprintf(stderr, "%s: %s\n", argv[1], e.what());
    return 1;
  }

  g.print();
  printf("\n%d nodes, %d edges\n", (int) g.nodes.size(), g.num_edges());
  return 0;
}
