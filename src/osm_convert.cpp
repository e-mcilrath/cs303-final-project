/*
Eric McIlrath
osm_convert.cpp

Converts the raw OpenStreetMap export (data/map.osm) into our graph file
format (NODE / EDGE / BUILDING lines, see graph.cpp) for the UTK campus,
with both roads and walkways.

usage: osm_convert map.osm graph-file

DRAFT: this is a skeleton with direction comments. Every function compiles
but does nothing yet. Fill in the TODOs roughly in order (Step 1, 2, ...);
main() already calls everything in the right order. The skeleton and the
comments were drafted with AI help.

Sanity numbers from a Python prototype on the same map.osm, so you can
check your output as you go (yours may differ a little depending on your
choices):
  - nodes inside the campus outline:          ~15,240
  - road/walkway ways kept:                   ~1,390
  - graph nodes (intersections + dead ends):  ~2,480 (about 2,500 with the
                                              McClung plaza outline)
  - segments between graph nodes:             ~3,640 (about 1,000 usable
                                              by cars, all walkable)
  - straight-across plaza edges:              ~2,580 in 63 plazas
  - plaza crossing edges through a building:  0 (if not, step 8 is wrong)
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

/* ------------------------------------------------------------------ */
/* Settings                                                            */
/* ------------------------------------------------------------------ */

/* The closed way tagged amenity=university, name="University of Tennessee,
   Knoxville". Anything inside it counts as UTK. */
static const long long CAMPUS_WAY_ID = 376167113;

static const double EARTH_RADIUS = 6371000.0;   // meters
static const double MPH_TO_MS = 0.44704;        // 1 mph in meters/second

/* ------------------------------------------------------------------ */
/* What we read out of the OSM file                                    */
/* ------------------------------------------------------------------ */

class OsmNode {
  public:
    double lat;
    double lon;
};

class OsmWay {
  public:
    long long id;
    vector <long long> refs;        // node ids, in order along the way
    map <string, string> tags;      // k="..." v="..."
};

class OsmMember {
  public:
    string type;                    // "node", "way" or "relation"
    long long ref;
    string role;                    // for multipolygons: "outer" or "inner"
};

class OsmRelation {
  public:
    long long id;
    vector <OsmMember> members;
    map <string, string> tags;
};

class OsmData {
  public:
    map <long long, OsmNode> nodes;      // node id -> position
    vector <OsmWay> ways;
    map <long long, int> way_index;      // way id -> index in ways
    vector <OsmRelation> relations;
};

/* ------------------------------------------------------------------ */
/* What we build and write out                                         */
/* ------------------------------------------------------------------ */

/* A path we route on: a road or walkway, or the outline of a plaza. */
class Line {
  public:
    vector <long long> refs;
    string highway;                 // value of the highway tag
    string name;
    bool car;                       // cars may use it
    bool walk;                      // people may walk it
    bool oneway;                    // one-way for cars, in refs order
    double speed;                   // car speed in m/s (free flow)
};

/* A walkable area (highway=pedestrian + area=yes, or a multipolygon). */
class Plaza {
  public:
    vector < vector <long long> > rings;   // closed rings: outer and holes
    set <long long> outline;               // every node on any ring
};

class OutEdge {
  public:
    long long from;
    long long to;
    double length;                  // meters
    double capacity;                // vehicles/hour (0 for walk-only)
    double free_flow;               // seconds at the car speed
    bool car;
    bool walk;
    string name;
};

class OutBuilding {
  public:
    string name;
    double lat;                     // center of the building
    double lon;
};

class OutGraph {
  public:
    set <long long> nodes;          // graph node ids
    vector <OutEdge> edges;
    vector <OutBuilding> buildings; // not tied to nodes
};

/* ------------------------------------------------------------------ */
/* Step 1: read the OSM XML                                            */
/* ------------------------------------------------------------------ */

/* Finds key="value" in one line of XML and stores value in val.
   Returns false if the key isn't on the line.

   TODO:
   - Search for  key + "=\""  (include the =" so k= doesn't match in
     "track=" etc., and put a space in front so "id" doesn't match "uid").
   - The value runs up to the next ". Values never contain a raw " because
     XML escapes it as &quot;.
   - Run the value through unescape_xml() before storing it. */
static bool get_attr(const string &line, const string &key, string &val)
{
  (void) line; (void) key; (void) val;    // remove these once implemented
  return false;
}

/* Turns XML escapes back into characters.

   TODO: replace &amp; &quot; &apos; &lt; &gt; with & " ' < >.
   Do &amp; LAST, or "&amp;lt;" turns into "<" instead of "&lt;".
   map.osm has 44 lines with escapes, e.g. "Science &amp; Engineering". */
static string unescape_xml(const string &s)
{
  return s;
}

/* Reads the whole file into osm.

   TODO: the export puts one XML element per line, so read it with
   getline() and look at how each line starts (after leading spaces):

     <node id=".." lat=".." lon=".." .../>     node with no tags (ends "/>")
     <node id=".." lat=".." lon=".." ...>      node with tags, ends at </node>
     <way id=".." ...>                         start a new OsmWay
       <nd ref=".."/>                          add to the current way's refs
       <tag k=".." v=".."/>                    add to the current way/relation
     </way>                                    push the way, set way_index
     <relation id=".." ...>                    start a new OsmRelation
       <member type=".." ref=".." role=".."/>
     </relation>

   - Keep a variable for "what am I inside right now" (nothing / node /
     way / relation) so <tag> lines go to the right place. Node tags can
     be skipped, we don't use them.
   - Use strtoll / strtod like graph.cpp does, and throw runtime_error with
     the line number if a number is bad (same style as bad_line()).
   - Check at the end: about 37,214 nodes, 4,726 ways, 105 relations. */
static void read_osm(istream &in, OsmData &osm)
{
  (void) in; (void) osm;
  (void) get_attr; (void) unescape_xml;
}

/* ------------------------------------------------------------------ */
/* Step 2: geometry helpers                                            */
/* ------------------------------------------------------------------ */

/* Great-circle distance in meters between two lat/lon points.

   TODO: haversine formula. Convert degrees to radians first.
     h = sin^2(dlat/2) + cos(lat1) * cos(lat2) * sin^2(dlon/2)
     d = 2 * R * asin(sqrt(h)) */
static double haversine(double lat1, double lon1, double lat2, double lon2)
{
  (void) lat1; (void) lon1; (void) lat2; (void) lon2;
  (void) EARTH_RADIUS;
  return 0;
}

/* Flat x/y in meters, good enough for geometry tests on a campus this
   small. Used by the plaza and point-in-polygon code, NOT for lengths.

   TODO: x = lon_radians * R * cos(35.95 degrees), y = lat_radians * R */
static void to_xy(const OsmNode &n, double &x, double &y)
{
  (void) n;
  x = 0;
  y = 0;
}

/* True if (x, y) is inside the shape made by these rings.

   TODO: even-odd ray casting. For every edge (a, b) of every ring, flip a
   bool if a horizontal ray going right from the point crosses that edge:
     (ay > y) != (by > y)  &&  x < ax + (y - ay) * (bx - ax) / (by - ay)
   Testing against all rings at once (outer + holes) means points inside a
   hole come out as outside, which is what we want. */
static bool inside_rings(double x, double y, const vector < vector <long long> > &rings,
                         const OsmData &osm)
{
  (void) x; (void) y; (void) rings; (void) osm;
  return false;
}

/* True if segment p1-p2 touches segment p3-p4 anywhere, ends included.

   TODO: use the cross product  cross(a, b, c) = (b-a) x (c-a).
   - The segments properly cross if p1 and p2 are on opposite sides of
     p3-p4 AND p3 and p4 are on opposite sides of p1-p2.
   - Also return true if any endpoint lies ON the other segment (cross
     product ~0 and inside the bounding box). Treat touching as blocked,
     that keeps plaza edges from slipping through a corner. */
static bool segments_touch(double x1, double y1, double x2, double y2,
                           double x3, double y3, double x4, double y4)
{
  (void) x1; (void) y1; (void) x2; (void) y2;
  (void) x3; (void) y3; (void) x4; (void) y4;
  return false;
}

/* ------------------------------------------------------------------ */
/* Step 3: which ways do we keep, and who can use them                 */
/* ------------------------------------------------------------------ */

/* Highway values in map.osm (counts are whole file):
     car + walk: primary, secondary, tertiary (+ _link), residential,
                 unclassified, living_street, service
     walk only:  footway (1,410), pedestrian (153), steps (137),
                 cycleway (61), path (8)
     car only:   motorway, motorway_link, trunk, trunk_link
                 (none of these end up on campus, but handle them)
     skip:       construction, and any highway value not listed

   Fills in car / walk / oneway / name / highway on line and returns true
   if the way should be kept.

   TODO:
   - Skip access=private or access=no (40 ways), unless foot=yes or
     foot=designated, in which case keep it as walk-only.
   - foot=no means not walkable.
   - oneway (verified on the OSM wiki, Key:oneway):
       oneway=yes / true / 1     -> one-way in refs order
       oneway=-1                 -> one-way AGAINST refs order (easiest:
                                    reverse refs and set oneway = true)
       junction=roundabout and highway=motorway are one-way even with no
       oneway tag. The wiki does NOT confirm this for motorway_link.
       map.osm only has oneway=yes (348) and oneway=no (35).
   - oneway only applies to vehicles on roads. Walkers can go both ways
     on a one-way street. */
static bool classify_way(const OsmWay &w, Line &line)
{
  (void) w; (void) line;
  return false;
}

/* True if any node of the way is inside the campus outline.

   TODO: keep the WHOLE way if any node is inside, instead of cutting it
   at the boundary. Cutting splits the walk graph into 9 pieces; keeping
   whole ways gives 4, with almost everything in one piece. */
static bool touches_campus(const vector <long long> &refs, const OsmData &osm,
                           const vector < vector <long long> > &campus)
{
  (void) refs; (void) osm; (void) campus;
  (void) inside_rings;
  return false;
}

/* ------------------------------------------------------------------ */
/* Step 4: speeds and capacities (rubric 2.2)                          */
/* ------------------------------------------------------------------ */

/* Returns the speed in m/s from a maxspeed tag, or -1 if there isn't a
   usable one.

   TODO:
   - Every maxspeed in map.osm looks like "35 mph" (only ~80 ways have one).
   - The OSM wiki (Key:maxspeed) says a bare number with no unit means km/h,
     so "50" is 50 km/h. Handle that too, even though this map has none. */
static double parse_maxspeed(const map <string, string> &tags)
{
  (void) tags;
  (void) MPH_TO_MS;
  return -1;
}

/* Default car speed (m/s) and capacity (vehicles/hour) for a road type,
   used when there's no maxspeed tag.

   TODO: pick your own numbers per highway value and write down where they
   came from for the report (e.g. posted campus limits, Highway Capacity
   Manual style per-lane values). Walk-only types get capacity 0. */
static void road_defaults(const string &highway, double &speed, double &capacity)
{
  (void) highway;
  speed = 0;
  capacity = 0;
}

/* ------------------------------------------------------------------ */
/* Step 5: plazas                                                      */
/* ------------------------------------------------------------------ */

/* Joins open ways end to end into closed rings (first id == last id).

   TODO: needed for multipolygon relations, whose outline is split over
   several member ways. Take one piece, then keep looking for a piece
   whose first or last node matches the current end (flip the piece if
   needed) and append it, until the ring closes. Throw runtime_error if
   it never closes. */
static vector < vector <long long> > stitch_rings(const vector < vector <long long> > &parts)
{
  (void) parts;
  return vector < vector <long long> >();
}

/* Collects every walkable area that touches campus.

   TODO: two kinds in map.osm:
   - 150 closed ways with highway=pedestrian + area=yes. One ring each.
   - 2 relations with type=multipolygon + highway=pedestrian (one is the
     big plaza around McClung Tower). Stitch the "outer" members and the
     "inner" members (holes: the tower, a fountain, planters) into rings.
     IMPORTANT: those member ways have no tags, so classify_way() never
     sees them. Add each ring to lines as a walk-only Line here, or the
     McClung plaza outline is missing from the graph.
   - Expect ~67 plazas on campus. */
static void collect_plazas(const OsmData &osm, const vector < vector <long long> > &campus,
                           vector <Plaza> &plazas, vector <Line> &lines)
{
  (void) osm; (void) campus; (void) plazas; (void) lines;
  (void) stitch_rings;
}

/* ------------------------------------------------------------------ */
/* Step 6: find graph nodes and cut lines into edges (rubric 2.1)      */
/* ------------------------------------------------------------------ */

/* TODO: a node becomes a graph node if
   - it is the first or last node of a line (dead ends need to be nodes), or
   - it shows up 2+ times across all lines (shared by two paths, or a loop).
   Easiest: count uses in a map<long long, int>, add 2 for endpoints and
   1 otherwise, then keep every node with count >= 2. */
static void find_graph_nodes(const vector <Line> &lines, OutGraph &g)
{
  (void) lines; (void) g;
}

/* TODO: walk each line's refs, adding haversine() between neighbors. Each
   time you reach a graph node, make an edge from the last graph node to
   this one with the length so far, then reset the length to 0.
   - Two-way line: push an edge each way.
   - One-way line: forward edge gets car = true; the backward edge still
     exists for walkers (car = false, walk = line.walk). Drop it if the
     line isn't walkable.
   - free_flow = length / speed for car edges. Walk-only edges get
     capacity 0 and free_flow 0 (Graph::read requires it); walking time
     is worked out by the routing code from length. */
static void split_lines(const vector <Line> &lines, const OsmData &osm, OutGraph &g)
{
  (void) lines; (void) osm; (void) g;
  (void) haversine; (void) parse_maxspeed; (void) road_defaults;
}

/* TODO: straight-across walk edges inside each plaza.
   1. Entry points = outline nodes that some OTHER line also uses (a
      footway reaching the plaza edge, or a neighboring plaza), PLUS graph
      nodes that sit inside the plaza (3 footways dead-end inside the
      McClung plaza without touching its edge).
   2. For every pair of entry points (a, b), add a two-way walk-only edge
      if the straight line a-b
        - doesn't touch any ring edge, except edges that have a or b as
          an end (segments_touch), and
        - has its midpoint inside the plaza (inside_rings). This catches
          lines across the open part of an L-shaped plaza.
   3. Length = haversine(a, b). Keep the outline edges too.
   Check: no crossing edge should pass through a building outline. */
static void add_plaza_edges(const vector <Plaza> &plazas, const vector <Line> &lines,
                            const OsmData &osm, OutGraph &g)
{
  (void) plazas; (void) lines; (void) osm; (void) g;
  (void) to_xy; (void) segments_touch;
}

/* ------------------------------------------------------------------ */
/* Step 7: buildings (rubric 2.3)                                      */
/* ------------------------------------------------------------------ */

/* Buildings are their own list (name + center), NOT attached to nodes. The
   routing code finds the nearest node to a building when it needs one.

   TODO:
   - Ways with a "building" tag AND a "name" tag, that touch campus
     (about 112 different names).
   - Center = average lat/lon of the outline points (skip the repeated
     last point of a closed way).
   - OSM sometimes packs two names into one value with ";" (e.g. "Berry
     Hall;Thompson-Boling Assembly Center and Arena"). Split it and add
     each name with the same center.
   - Some names repeat (one building split into several outlines). Keep one
     entry per name: average all of its outline points into one center. */
static void collect_buildings(const OsmData &osm, const vector < vector <long long> > &campus,
                              OutGraph &g)
{
  (void) osm; (void) campus; (void) g;
}

/* ------------------------------------------------------------------ */
/* Step 8: checks (rubric 2.4)                                         */
/* ------------------------------------------------------------------ */

/* TODO:
   - Duplicate edges (same from, to, car, walk): keep the shortest.
   - Edges with length <= 0: shouldn't happen, report them if they do.
   - Keep only the largest connected piece. For walking, a BFS ignoring
     direction is fine (walk edges always come in pairs). For cars, one-way
     roads matter: you want nodes you can both reach and leave, which is
     a strongly connected component (or BFS forward + BFS on reversed
     edges from one big node, keep nodes in both). Expect about 66
     car-only nodes to fall off (parking lot stubs off Neyland Drive).
   - Removing nodes means removing their edges too. Buildings aren't tied
     to nodes, so they stay.
   - Print a summary to stderr: node count, edge counts (car / walk /
     plaza), pieces found and how big each was. */
static void check_graph(OutGraph &g)
{
  (void) g;
}

/* ------------------------------------------------------------------ */
/* Step 9: write the file                                              */
/* ------------------------------------------------------------------ */

/* TODO:
   - NODE lines first (graph.cpp needs nodes defined before edges), then
     EDGE lines, then BUILDING lines. Use %.7f for lat/lon (OSM's own
     precision) and %.1f or so for length/time.
   - BUILDING lines are  BUILDING lat lon name  (the building's center).
   - Put a header comment in the file: where it came from, the date, and
     "Map data (c) OpenStreetMap contributors, ODbL".
   - EDGE lines carry a mode before the name (Graph::read checks it):
       EDGE from to length capacity free_flow C|W|CW [name]
     W edges must have capacity 0 and free_flow 0, C/CW edges need both
     > 0. data/example_campus.txt shows every case. */
static void write_graph(ostream &out, const OutGraph &g, const OsmData &osm)
{
  (void) out; (void) g; (void) osm;
}

/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
  OsmData osm;
  vector < vector <long long> > campus;
  vector <Line> lines;
  vector <Plaza> plazas;
  OutGraph g;
  ifstream fin;
  ofstream fout;
  size_t i;
  Line line;

  if (argc != 3) {
    fprintf(stderr, "usage: osm_convert map.osm graph-file\n");
    return 1;
  }

  fin.open(argv[1]);
  if (!fin.is_open()) {
    fprintf(stderr, "couldn't open %s\n", argv[1]);
    return 1;
  }

  try {
    read_osm(fin, osm);                                         // step 1
    fprintf(stderr, "read %d nodes, %d ways, %d relations\n",
            (int) osm.nodes.size(), (int) osm.ways.size(), (int) osm.relations.size());

    /* TODO: look up CAMPUS_WAY_ID in osm.way_index, throw if it's missing,
       and put its refs in campus as the only ring. */
    (void) CAMPUS_WAY_ID;

    for (i = 0; i < osm.ways.size(); i++) {                     // step 3
      if (classify_way(osm.ways[i], line) && touches_campus(line.refs, osm, campus)) {
        lines.push_back(line);
      }
    }
    collect_plazas(osm, campus, plazas, lines);                 // step 5

    find_graph_nodes(lines, g);                                 // step 6
    split_lines(lines, osm, g);
    add_plaza_edges(plazas, lines, osm, g);
    collect_buildings(osm, campus, g);                          // step 7
    check_graph(g);                                             // step 8
  } catch (const runtime_error &e) {
    fprintf(stderr, "%s: %s\n", argv[1], e.what());
    return 1;
  }

  fout.open(argv[2]);
  if (!fout.is_open()) {
    fprintf(stderr, "couldn't write %s\n", argv[2]);
    return 1;
  }
  write_graph(fout, g, osm);                                    // step 9
  fprintf(stderr, "wrote %d nodes, %d edges, %d buildings to %s\n",
          (int) g.nodes.size(), (int) g.edges.size(), (int) g.buildings.size(), argv[2]);
  return 0;
}
