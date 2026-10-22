/*
Eric McIlrath
graph.cpp

Graph file format, one thing per line. Anything after a # is a comment and
blank lines are skipped.

  NODE id lat lon
  EDGE from_id to_id length capacity free_flow [road name]
  BUILDING node_id building name

Nodes have to be defined before an EDGE or BUILDING line uses them. Edges are
directed, so a two-way road needs two EDGE lines.
*/

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include "graph.h"

using namespace std;

double Edge::congestion() const
{
  return volume / capacity;
}

/* Throws an error that says which line of the file was bad. */
static void bad_line(int line_num, const string &msg)
{
  throw runtime_error("line " + to_string(line_num) + ": " + msg);
}

/* These make sure the whole token is a number, so stuff like "12abc"
   doesn't get read as 12. */
static bool to_double(const string &s, double &d)
{
  char *end;

  errno = 0;
  d = strtod(s.c_str(), &end);
  return !s.empty() && *end == '\0' && errno == 0;
}

static bool to_id(const string &s, long long &id)
{
  char *end;

  errno = 0;
  id = strtoll(s.c_str(), &end, 10);
  return !s.empty() && *end == '\0' && errno == 0;
}

/* Reads whatever is left on the line (for names with spaces in them) and
   trims the whitespace off both ends. */
static string rest_of_line(istringstream &ss)
{
  string s;
  size_t first, last;

  getline(ss, s);
  first = s.find_first_not_of(" \t\r");
  if (first == string::npos) return "";
  last = s.find_last_not_of(" \t\r");
  return s.substr(first, last - first + 1);
}

int Graph::find_node(long long id) const
{
  map <long long, int>::const_iterator it;

  it = index.find(id);
  if (it == index.end()) return -1;
  return it->second;
}

int Graph::add_node(long long id, double lat, double lon)
{
  Node n;

  if (find_node(id) != -1) {
    throw runtime_error("duplicate node id " + to_string(id));
  }

  n.id = id;
  n.lat = lat;
  n.lon = lon;
  nodes.push_back(n);
  index[id] = nodes.size() - 1;
  return nodes.size() - 1;
}

void Graph::add_edge(int from, int to, double length, double capacity,
                     double free_flow, const string &name)
{
  Edge e;

  if (from < 0 || from >= (int) nodes.size() || to < 0 || to >= (int) nodes.size()) {
    throw runtime_error("add_edge: node index out of range");
  }
  if (from == to) throw runtime_error("add_edge: edge goes from a node to itself");

  /* Written as !(x > 0) so NaN gets caught too. */
  if (!(length > 0)) throw runtime_error("add_edge: length must be > 0");
  if (!(capacity > 0)) throw runtime_error("add_edge: capacity must be > 0");
  if (!(free_flow > 0)) throw runtime_error("add_edge: free flow time must be > 0");

  e.to = to;
  e.length = length;
  e.capacity = capacity;
  e.free_flow = free_flow;
  e.volume = 0;
  e.name = name;
  nodes[from].adj.push_back(e);
}

void Graph::add_building(int node, const string &building)
{
  if (node < 0 || node >= (int) nodes.size()) {
    throw runtime_error("add_building: node index out of range");
  }
  nodes[node].buildings.push_back(building);
}

void Graph::read(istream &in)
{
  string line, type, extra;
  string s_id, s_to, s_lat, s_lon, s_len, s_cap, s_ff;
  long long id, to_id_num;
  double lat, lon, length, capacity, free_flow;
  int line_num, from, to;
  size_t hash;

  line_num = 0;
  while (getline(in, line)) {
    line_num++;

    hash = line.find('#');
    if (hash != string::npos) line.erase(hash);

    istringstream ss(line);
    if (!(ss >> type)) continue;   // blank line

    // AI helped here
    if (type == "NODE") {
      if (!(ss >> s_id >> s_lat >> s_lon)) bad_line(line_num, "NODE needs: id lat lon");
      if (ss >> extra) bad_line(line_num, "extra stuff at the end of NODE line");
      if (!to_id(s_id, id)) bad_line(line_num, "bad node id \"" + s_id + "\"");
      if (!to_double(s_lat, lat) || !(lat >= -90 && lat <= 90)) {
        bad_line(line_num, "bad latitude \"" + s_lat + "\"");
      }
      if (!to_double(s_lon, lon) || !(lon >= -180 && lon <= 180)) {
        bad_line(line_num, "bad longitude \"" + s_lon + "\"");
      }
      if (find_node(id) != -1) bad_line(line_num, "duplicate node id " + s_id);
      add_node(id, lat, lon);

    } else if (type == "EDGE") {
      if (!(ss >> s_id >> s_to >> s_len >> s_cap >> s_ff)) {
        bad_line(line_num, "EDGE needs: from to length capacity free_flow [name]");
      }
      if (!to_id(s_id, id)) bad_line(line_num, "bad node id \"" + s_id + "\"");
      if (!to_id(s_to, to_id_num)) bad_line(line_num, "bad node id \"" + s_to + "\"");
      from = find_node(id);
      to = find_node(to_id_num);
      if (from == -1) bad_line(line_num, "edge uses unknown node " + s_id);
      if (to == -1) bad_line(line_num, "edge uses unknown node " + s_to);
      if (from == to) bad_line(line_num, "edge goes from a node to itself");
      if (!to_double(s_len, length) || !(length > 0)) {
        bad_line(line_num, "bad length \"" + s_len + "\" (must be > 0)");
      }
      if (!to_double(s_cap, capacity) || !(capacity > 0)) {
        bad_line(line_num, "bad capacity \"" + s_cap + "\" (must be > 0)");
      }
      if (!to_double(s_ff, free_flow) || !(free_flow > 0)) {
        bad_line(line_num, "bad free flow time \"" + s_ff + "\" (must be > 0)");
      }
      add_edge(from, to, length, capacity, free_flow, rest_of_line(ss));

    } else if (type == "BUILDING") {
      if (!(ss >> s_id)) bad_line(line_num, "BUILDING needs: node_id name");
      if (!to_id(s_id, id)) bad_line(line_num, "bad node id \"" + s_id + "\"");
      from = find_node(id);
      if (from == -1) bad_line(line_num, "building uses unknown node " + s_id);
      extra = rest_of_line(ss);
      if (extra.empty()) bad_line(line_num, "BUILDING is missing a name");
      add_building(from, extra);

    } else {
      bad_line(line_num, "unknown line type \"" + type + "\"");
    }
  }

  if (in.bad()) throw runtime_error("error reading the graph file");
}

int Graph::num_edges() const
{
  size_t i;
  int total;

  total = 0;
  for (i = 0; i < nodes.size(); i++) total += nodes[i].adj.size();
  return total;
}

void Graph::print() const
{
  size_t i, j;
  const Node *n;
  const Edge *e;

  for (i = 0; i < nodes.size(); i++) {
    n = &nodes[i];
    printf("Node %lld (%.5f, %.5f)\n", n->id, n->lat, n->lon);

    for (j = 0; j < n->buildings.size(); j++) {
      printf("  building: %s\n", n->buildings[j].c_str());
    }

    if (n->adj.empty()) printf("  (no outgoing edges)\n");
    for (j = 0; j < n->adj.size(); j++) {
      e = &n->adj[j];
      printf("  -> %-6lld %-24s %7.1f m  cap %6.0f/hr  free flow %5.1f s  v/c %.2f\n",
             nodes[e->to].id, e->name.empty() ? "(unnamed)" : e->name.c_str(),
             e->length, e->capacity, e->free_flow, e->congestion());
    }
  }
}
