/*
Eric McIlrath
osm_convert.cpp

Converts the raw OpenStreetMap export (data/map.osm) into our graph file
format (NODE / EDGE / BUILDING lines, read by src/LoadMap.cpp) for the UTK
campus, with both roads and walkways.

usage: osm_convert map.osm [graph-file]

With no graph-file the graph goes to standard output, so this works:
  ./bin/osm_convert data/map.osm > data/map.txt
Progress and the check summary always go to standard error.

The skeleton and its direction comments were drafted with AI help, and the
steps were filled in with AI help (Claude) too.

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
  - plaza crossing edges through a building:  0
*/

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
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
static const double PI = 3.14159265358979323846;

/* Latitude used for the flat x/y projection (middle of campus). */
static const double CAMPUS_LAT = 35.95;

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
    string bounds;                       // from <bounds>, for the header
    string newest;                       // newest element timestamp
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
    double capacity;                // vehicles/hour in each car direction
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
    bool plaza;                     // straight-across plaza edge
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
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

/* Returns the value of tag k, or "" if the way/relation doesn't have it. */
static string tag(const map <string, string> &tags, const string &k)
{
  map <string, string>::const_iterator it;

  it = tags.find(k);
  return (it == tags.end()) ? "" : it->second;
}

static bool starts_with(const string &s, const string &prefix)
{
  return s.compare(0, prefix.size(), prefix) == 0;
}

static string trim(const string &s)
{
  size_t b, e;

  b = s.find_first_not_of(" \t\r\n");
  if (b == string::npos) return "";
  e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

/* Position of an OSM node. Every node a way uses must be in the file. */
static const OsmNode &node_at(const OsmData &osm, long long id)
{
  map <long long, OsmNode>::const_iterator it;

  it = osm.nodes.find(id);
  if (it == osm.nodes.end()) {
    throw runtime_error("node " + to_string(id) + " is used but not in the file");
  }
  return it->second;
}

/* Throws an error that names the line of map.osm it came from. */
static void bad_line(int line_num, const string &what)
{
  throw runtime_error("line " + to_string(line_num) + ": " + what);
}

static long long to_ll(const string &s, int line_num)
{
  char *end;
  long long v;

  v = strtoll(s.c_str(), &end, 10);
  if (s.empty() || *end != '\0') bad_line(line_num, "bad number \"" + s + "\"");
  return v;
}

static double to_double(const string &s, int line_num)
{
  char *end;
  double v;

  v = strtod(s.c_str(), &end);
  if (s.empty() || *end != '\0' || !std::isfinite(v)) {
    bad_line(line_num, "bad number \"" + s + "\"");
  }
  return v;
}

/* ------------------------------------------------------------------ */
/* Step 1: read the OSM XML                                            */
/* ------------------------------------------------------------------ */

static void replace_all(string &s, const string &from, const string &to)
{
  size_t pos;

  pos = 0;
  while ((pos = s.find(from, pos)) != string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
}

/* Turns XML escapes back into characters. &amp; goes LAST, or "&amp;lt;"
   would turn into "<" instead of "&lt;". map.osm has 44 &amp; and 6 &quot;
   and no numeric (&#..;) escapes. */
static string unescape_xml(const string &s)
{
  string r;

  r = s;
  replace_all(r, "&quot;", "\"");
  replace_all(r, "&apos;", "'");
  replace_all(r, "&lt;", "<");
  replace_all(r, "&gt;", ">");
  replace_all(r, "&amp;", "&");
  return r;
}

/* Finds key="value" in one line of XML and stores the unescaped value in
   val. Returns false if the key isn't on the line. The space in front of
   the key keeps "id" from matching "uid", and the =" keeps "k" from
   matching inside other names. Values never contain a raw " because XML
   escapes it as &quot;. */
static bool get_attr(const string &line, const string &key, string &val)
{
  string pat;
  size_t start, end;

  pat = " " + key + "=\"";
  start = line.find(pat);
  if (start == string::npos) return false;
  start += pat.size();
  end = line.find('"', start);
  if (end == string::npos) return false;
  val = unescape_xml(line.substr(start, end - start));
  return true;
}

/* Like get_attr, but the attribute has to be there. */
static string need_attr(const string &line, const string &key, int line_num)
{
  string val;

  if (!get_attr(line, key, val)) bad_line(line_num, "missing " + key + "=\"...\"");
  return val;
}

/* Reads the whole file into osm. The export puts one XML element per line:

     <node id=".." lat=".." lon=".." .../>     node with no tags
     <node id=".." lat=".." lon=".." ...>      node with tags, ends at </node>
     <way id=".." ...>                         start a new OsmWay
       <nd ref=".."/>                          add to the current way's refs
       <tag k=".." v=".."/>                    add to the current way/relation
     </way>
     <relation id=".." ...>
       <member type=".." ref=".." role=".."/>
     </relation>

   Node tags are skipped (we don't use them). */
static void read_osm(istream &in, OsmData &osm)
{
  enum State { NONE, IN_NODE, IN_WAY, IN_RELATION };
  State state;
  string raw, s, k, v, ts;
  int line_num;
  bool closed;
  OsmNode node;
  OsmWay way;
  OsmRelation rel;
  OsmMember mem;
  long long id;

  state = NONE;
  line_num = 0;
  while (getline(in, raw)) {
    line_num++;
    s = trim(raw);
    if (s.empty()) continue;
    closed = (s.size() >= 2 && s.compare(s.size() - 2, 2, "/>") == 0);

    if (starts_with(s, "<node ")) {
      if (state != NONE) bad_line(line_num, "<node> inside another element");
      id = to_ll(need_attr(s, "id", line_num), line_num);
      node.lat = to_double(need_attr(s, "lat", line_num), line_num);
      node.lon = to_double(need_attr(s, "lon", line_num), line_num);
      osm.nodes[id] = node;
      if (!closed) state = IN_NODE;

    } else if (s == "</node>") {
      if (state != IN_NODE) bad_line(line_num, "</node> without <node>");
      state = NONE;

    } else if (starts_with(s, "<way ")) {
      if (state != NONE) bad_line(line_num, "<way> inside another element");
      way = OsmWay();
      way.id = to_ll(need_attr(s, "id", line_num), line_num);
      state = IN_WAY;

    } else if (starts_with(s, "<nd ")) {
      if (state != IN_WAY) bad_line(line_num, "<nd> outside a way");
      way.refs.push_back(to_ll(need_attr(s, "ref", line_num), line_num));

    } else if (starts_with(s, "<tag ")) {
      k = need_attr(s, "k", line_num);
      v = need_attr(s, "v", line_num);
      if (state == IN_WAY) way.tags[k] = v;
      else if (state == IN_RELATION) rel.tags[k] = v;
      else if (state != IN_NODE) bad_line(line_num, "<tag> outside an element");

    } else if (s == "</way>") {
      if (state != IN_WAY) bad_line(line_num, "</way> without <way>");
      osm.way_index[way.id] = (int) osm.ways.size();
      osm.ways.push_back(way);
      state = NONE;

    } else if (starts_with(s, "<relation ")) {
      if (state != NONE) bad_line(line_num, "<relation> inside another element");
      rel = OsmRelation();
      rel.id = to_ll(need_attr(s, "id", line_num), line_num);
      state = IN_RELATION;

    } else if (starts_with(s, "<member ")) {
      if (state != IN_RELATION) bad_line(line_num, "<member> outside a relation");
      mem.type = need_attr(s, "type", line_num);
      mem.ref = to_ll(need_attr(s, "ref", line_num), line_num);
      mem.role = need_attr(s, "role", line_num);
      rel.members.push_back(mem);

    } else if (s == "</relation>") {
      if (state != IN_RELATION) bad_line(line_num, "</relation> without <relation>");
      osm.relations.push_back(rel);
      state = NONE;

    } else if (starts_with(s, "<bounds ")) {
      osm.bounds = "minlat " + need_attr(s, "minlat", line_num) +
                   " minlon " + need_attr(s, "minlon", line_num) +
                   " maxlat " + need_attr(s, "maxlat", line_num) +
                   " maxlon " + need_attr(s, "maxlon", line_num);
    }
    /* Anything else (<?xml, <osm>, </osm>) carries nothing we need. */

    /* A self-closing <way .../> or <relation .../> has no children. */
    if (closed && state == IN_WAY && starts_with(s, "<way ")) {
      osm.way_index[way.id] = (int) osm.ways.size();
      osm.ways.push_back(way);
      state = NONE;
    } else if (closed && state == IN_RELATION && starts_with(s, "<relation ")) {
      osm.relations.push_back(rel);
      state = NONE;
    }

    /* ISO 8601 timestamps sort as text, so the largest is the newest. */
    if (get_attr(s, "timestamp", ts) && ts > osm.newest) osm.newest = ts;
  }
  if (state != NONE) throw runtime_error("file ends inside an element");
}

/* ------------------------------------------------------------------ */
/* Step 2: geometry helpers                                            */
/* ------------------------------------------------------------------ */

static double to_rad(double degrees)
{
  return degrees * PI / 180.0;
}

/* Great-circle distance in meters between two lat/lon points (haversine):
     h = sin^2(dlat/2) + cos(lat1) * cos(lat2) * sin^2(dlon/2)
     d = 2 * R * asin(sqrt(h)) */
static double haversine(double lat1, double lon1, double lat2, double lon2)
{
  double dlat, dlon, h;

  dlat = to_rad(lat2 - lat1);
  dlon = to_rad(lon2 - lon1);
  h = sin(dlat / 2) * sin(dlat / 2) +
      cos(to_rad(lat1)) * cos(to_rad(lat2)) * sin(dlon / 2) * sin(dlon / 2);
  return 2 * EARTH_RADIUS * asin(sqrt(min(1.0, h)));
}

static double node_distance(const OsmData &osm, long long a, long long b)
{
  const OsmNode &na = node_at(osm, a);
  const OsmNode &nb = node_at(osm, b);

  return haversine(na.lat, na.lon, nb.lat, nb.lon);
}

/* Flat x/y in meters, good enough for geometry tests on a campus this
   small. Used by the plaza and point-in-polygon code, NOT for lengths. */
static void to_xy(const OsmNode &n, double &x, double &y)
{
  x = to_rad(n.lon) * EARTH_RADIUS * cos(to_rad(CAMPUS_LAT));
  y = to_rad(n.lat) * EARTH_RADIUS;
}

/* True if (x, y) is inside the shape made by these closed rings, using
   even-odd ray casting: flip a bool for every ring edge that a ray going
   right from the point crosses. Testing all rings at once (outer + holes)
   makes points inside a hole come out as outside. */
static bool inside_rings(double x, double y, const vector < vector <long long> > &rings,
                         const OsmData &osm)
{
  bool inside;
  size_t r, j;
  double ax, ay, bx, by;

  inside = false;
  for (r = 0; r < rings.size(); r++) {
    for (j = 0; j + 1 < rings[r].size(); j++) {
      to_xy(node_at(osm, rings[r][j]), ax, ay);
      to_xy(node_at(osm, rings[r][j + 1]), bx, by);
      if ((ay > y) != (by > y) && x < ax + (y - ay) * (bx - ax) / (by - ay)) {
        inside = !inside;
      }
    }
  }
  return inside;
}

/* (b - a) x (c - a): > 0 if c is left of a->b, < 0 if right, 0 if on the line. */
static double cross(double ax, double ay, double bx, double by, double cx, double cy)
{
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

/* For a point p already known to be on the line through a and b: is it
   between them (inside the bounding box)? */
static bool in_box(double ax, double ay, double bx, double by, double px, double py)
{
  const double eps = 1e-6;    // meters

  return px >= min(ax, bx) - eps && px <= max(ax, bx) + eps &&
         py >= min(ay, by) - eps && py <= max(ay, by) + eps;
}

/* True if segment p1-p2 touches segment p3-p4 anywhere, ends included.
   Touching counts as blocked, which keeps plaza edges from slipping
   through a corner. */
static bool segments_touch(double x1, double y1, double x2, double y2,
                           double x3, double y3, double x4, double y4)
{
  const double eps = 1e-6;    // square meters: "on the line"
  double d1, d2, d3, d4;

  d1 = cross(x3, y3, x4, y4, x1, y1);
  d2 = cross(x3, y3, x4, y4, x2, y2);
  d3 = cross(x1, y1, x2, y2, x3, y3);
  d4 = cross(x1, y1, x2, y2, x4, y4);

  /* Proper crossing: each segment's ends are on opposite sides of the other. */
  if (((d1 > eps && d2 < -eps) || (d1 < -eps && d2 > eps)) &&
      ((d3 > eps && d4 < -eps) || (d3 < -eps && d4 > eps))) return true;

  /* An end lying on the other segment. */
  if (fabs(d1) <= eps && in_box(x3, y3, x4, y4, x1, y1)) return true;
  if (fabs(d2) <= eps && in_box(x3, y3, x4, y4, x2, y2)) return true;
  if (fabs(d3) <= eps && in_box(x1, y1, x2, y2, x3, y3)) return true;
  if (fabs(d4) <= eps && in_box(x1, y1, x2, y2, x4, y4)) return true;
  return false;
}

/* ------------------------------------------------------------------ */
/* Step 4: speeds and capacities (rubric 2.2)                          */
/* ------------------------------------------------------------------ */

/* Returns the speed in m/s from a maxspeed tag, or -1 if there isn't a
   usable one. Every maxspeed in map.osm looks like "35 mph" (80 ways have
   one). The OSM wiki (Key:maxspeed) says values are km/h by default, so a
   bare number like "50" is 50 km/h. Anything else ("none", "walk",
   "30;40", ...) is not usable and falls back to the road type default. */
static double parse_maxspeed(const map <string, string> &tags)
{
  string v, unit;
  const char *p;
  char *end;
  double num;

  v = tag(tags, "maxspeed");
  if (v.empty()) return -1;
  p = v.c_str();
  num = strtod(p, &end);
  if (end == p || !std::isfinite(num) || num <= 0) return -1;
  unit = trim(string(end));
  if (unit.empty() || unit == "km/h") return num / 3.6;
  if (unit == "mph") return num * MPH_TO_MS;
  return -1;
}

/* Default car speed (m/s) and capacity per lane (vehicles/hour) for a road
   type, used when there's no maxspeed tag. Walk-only types get 0 and 0.

   Speeds:
   - service, living_street: 20 mph. UTK Parking and Transportation,
     Traffic and Parking Regulations: "the maximum speed limit on campus
     streets is 20 mph and 30 mph on the city streets or as posted"
     (https://parking.utk.edu/citations/traffic-and-parking-regulations/).
     Service roads here are campus drives and parking aisles.
   - motorway 55 mph, trunk 35 mph, primary 45 mph, secondary 35 mph: the
     most common posted maxspeed for that type in map.osm itself (motorway
     13 of 23 tagged ways, trunk 8 of 9, primary 28 of 28, secondary 10 of
     10).
   - everything else (tertiary, residential, unclassified, links): 30 mph,
     the city street limit from the same UTK page. The tags don't say which
     residential streets are campus streets, so these use the city value.

   Capacity per lane:
   - 1,900 for primary, secondary, tertiary, trunk, motorway and links:
     FHWA Traffic Signal Timing Manual, ch. 3: "the ideal saturation flow
     rate, which is typically assumed to be 1,900 passenger cars per hour
     per lane" (https://ops.fhwa.dot.gov/publications/fhwahop08024/chapter3.htm).
   - 1,500 for residential, unclassified, living_street, service: the low
     end of the "1,500 to 2,000 passenger cars per hour per lane" range on
     the same page. This one is our own pick for small streets, not a
     published number for them. Capacity only matters for traffic later. */
static void road_defaults(const string &highway, double &speed, double &capacity)
{
  const double campus = 20 * MPH_TO_MS;
  const double city = 30 * MPH_TO_MS;

  if (highway == "motorway") {
    speed = 55 * MPH_TO_MS; capacity = 1900;
  } else if (highway == "trunk") {
    speed = 35 * MPH_TO_MS; capacity = 1900;
  } else if (highway == "primary") {
    speed = 45 * MPH_TO_MS; capacity = 1900;
  } else if (highway == "secondary") {
    speed = 35 * MPH_TO_MS; capacity = 1900;
  } else if (highway == "tertiary" || highway == "motorway_link" || highway == "trunk_link" ||
             highway == "primary_link" || highway == "secondary_link" ||
             highway == "tertiary_link") {
    speed = city; capacity = 1900;
  } else if (highway == "residential" || highway == "unclassified") {
    speed = city; capacity = 1500;
  } else if (highway == "living_street" || highway == "service") {
    speed = campus; capacity = 1500;
  } else {
    speed = 0; capacity = 0;
  }
}

/* ------------------------------------------------------------------ */
/* Step 3: which ways do we keep, and who can use them                 */
/* ------------------------------------------------------------------ */

static bool is_one_of(const string &s, const char *const *list)
{
  int i;

  for (i = 0; list[i] != NULL; i++) {
    if (s == list[i]) return true;
  }
  return false;
}

/* Fills in line from way w and returns true if the way should be kept.

   Highway values in map.osm (counts are whole file):
     car + walk: primary, secondary, tertiary (+ _link), residential,
                 unclassified, living_street, service
     walk only:  footway (1,410), pedestrian (153), steps (137),
                 cycleway (61), path (8)
     car only:   motorway, motorway_link, trunk, trunk_link
     skip:       construction, and any highway value not listed

   Access (OSM wiki, Key:access):
   - access=private or access=no: skipped, unless foot=yes or
     foot=designated, which keeps it as walk-only.
   - foot=yes / designated makes a way walkable, foot=no makes it not.
   - motor_vehicle or vehicle = no / private: no cars (6 + 5 ways).

   oneway (OSM wiki, Key:oneway):
   - oneway=yes (and the deprecated 1, and true): one-way in refs order.
   - oneway=-1: one-way against refs order, so refs get reversed.
   - junction=roundabout and highway=motorway are one-way with no oneway
     tag. The wiki does NOT say this for motorway_link.
   - oneway only limits cars here. Walkers can go both ways on a one-way
     street (the wiki: on roads it "only applies to vehicles"). The only
     walk-only ways with oneway=yes in map.osm are 5 cycleways. */
static bool classify_way(const OsmWay &w, Line &line)
{
  static const char *const both[] = {
    "primary", "primary_link", "secondary", "secondary_link", "tertiary",
    "tertiary_link", "residential", "unclassified", "living_street", "service", NULL
  };
  static const char *const walk_only[] = {
    "footway", "pedestrian", "steps", "cycleway", "path", NULL
  };
  static const char *const car_only[] = {
    "motorway", "motorway_link", "trunk", "trunk_link", NULL
  };
  string hw, access, foot, ow, mv, veh;
  bool foot_ok;
  double default_speed, lane_capacity, posted, lanes, lanes_each_way;

  hw = tag(w.tags, "highway");
  access = tag(w.tags, "access");
  foot = tag(w.tags, "foot");
  ow = tag(w.tags, "oneway");
  mv = tag(w.tags, "motor_vehicle");
  veh = tag(w.tags, "vehicle");
  foot_ok = (foot == "yes" || foot == "designated");

  line.refs = w.refs;
  line.highway = hw;
  line.name = tag(w.tags, "name");
  line.car = false;
  line.walk = false;
  line.oneway = false;
  line.speed = 0;
  line.capacity = 0;

  if (is_one_of(hw, both)) {
    line.car = true;
    line.walk = true;
  } else if (is_one_of(hw, walk_only)) {
    line.walk = true;
  } else if (is_one_of(hw, car_only)) {
    line.car = true;
  } else {
    return false;
  }

  if (foot_ok) line.walk = true;
  if (foot == "no") line.walk = false;
  if (access == "private" || access == "no") {
    if (!foot_ok) return false;
    line.car = false;
    line.walk = true;
  }
  if (mv == "no" || mv == "private" || veh == "no" || veh == "private") line.car = false;
  if (!line.car && !line.walk) return false;
  if (line.refs.size() < 2) return false;

  if (line.car) {
    if (ow == "yes" || ow == "true" || ow == "1") {
      line.oneway = true;
    } else if (ow == "-1") {
      line.oneway = true;
      reverse(line.refs.begin(), line.refs.end());
    } else if (ow.empty() && (tag(w.tags, "junction") == "roundabout" || hw == "motorway")) {
      line.oneway = true;
    }

    road_defaults(hw, default_speed, lane_capacity);
    posted = parse_maxspeed(w.tags);
    line.speed = (posted > 0) ? posted : default_speed;

    /* lanes counts every motor traffic lane, both directions together
       (OSM wiki, Key:lanes). With no usable tag: one lane each way. */
    lanes = atof(tag(w.tags, "lanes").c_str());
    if (lanes >= 1) {
      lanes_each_way = line.oneway ? lanes : max(1.0, lanes / 2);
    } else {
      lanes_each_way = 1;
    }
    line.capacity = lane_capacity * lanes_each_way;
  }
  return true;
}

/* True if any node of the way is inside the campus outline. The WHOLE way
   is kept if any node is inside, instead of cutting it at the boundary.
   Cutting splits the walk graph into 9 pieces; keeping whole ways gives 4,
   with almost everything in one piece. */
static bool touches_campus(const vector <long long> &refs, const OsmData &osm,
                           const vector < vector <long long> > &campus)
{
  size_t i;
  double x, y;

  for (i = 0; i < refs.size(); i++) {
    to_xy(node_at(osm, refs[i]), x, y);
    if (inside_rings(x, y, campus, osm)) return true;
  }
  return false;
}

static bool rings_touch_campus(const vector < vector <long long> > &rings, const OsmData &osm,
                               const vector < vector <long long> > &campus)
{
  size_t r;

  for (r = 0; r < rings.size(); r++) {
    if (touches_campus(rings[r], osm, campus)) return true;
  }
  return false;
}

/* ------------------------------------------------------------------ */
/* Step 5: plazas                                                      */
/* ------------------------------------------------------------------ */

/* Joins open ways end to end into closed rings (first id == last id).
   Needed for multipolygon relations, whose outline is split over several
   member ways. Takes one piece, then keeps appending a piece whose first
   or last node matches the current end (flipped if needed) until the ring
   closes. Throws runtime_error if it never closes. */
static vector < vector <long long> > stitch_rings(const vector < vector <long long> > &parts)
{
  vector < vector <long long> > rings;
  vector <bool> used;
  vector <long long> ring, piece;
  size_t i, start;
  bool found;

  used.assign(parts.size(), false);
  for (start = 0; start < parts.size(); start++) {
    if (used[start] || parts[start].empty()) continue;
    used[start] = true;
    ring = parts[start];

    while (ring.front() != ring.back()) {
      found = false;
      for (i = 0; i < parts.size() && !found; i++) {
        if (used[i] || parts[i].empty()) continue;
        piece = parts[i];
        if (piece.back() == ring.back()) reverse(piece.begin(), piece.end());
        if (piece.front() == ring.back()) {
          ring.insert(ring.end(), piece.begin() + 1, piece.end());
          used[i] = true;
          found = true;
        }
      }
      if (!found) {
        throw runtime_error("ring starting at node " + to_string(ring.front()) +
                            " never closes");
      }
    }
    if (ring.size() >= 4) rings.push_back(ring);
  }
  return rings;
}

/* Stitches the member ways of a multipolygon with the given role into
   rings. Returns false if a member way is missing from the export. */
static bool member_rings(const OsmData &osm, const OsmRelation &rel, const string &role,
                         vector < vector <long long> > &rings)
{
  vector < vector <long long> > parts;
  map <long long, int>::const_iterator it;
  size_t i;

  for (i = 0; i < rel.members.size(); i++) {
    if (rel.members[i].type != "way" || rel.members[i].role != role) continue;
    it = osm.way_index.find(rel.members[i].ref);
    if (it == osm.way_index.end()) return false;
    parts.push_back(osm.ways[it->second].refs);
  }
  rings = stitch_rings(parts);
  return true;
}

static void add_plaza(const vector < vector <long long> > &rings, vector <Plaza> &plazas)
{
  Plaza p;
  size_t r;

  p.rings = rings;
  for (r = 0; r < rings.size(); r++) p.outline.insert(rings[r].begin(), rings[r].end());
  plazas.push_back(p);
}

/* Collects every walkable area that touches campus. Two kinds in map.osm:
   - Closed ways with highway=pedestrian + area=yes (150). One ring each.
     classify_way() already kept their outlines as walk-only lines.
   - Relations with type=multipolygon + highway=pedestrian (2, one is the
     big plaza around McClung Tower). The "outer" members and the "inner"
     members (holes: the tower, a fountain, planters) are stitched into
     rings. Those member ways have no tags, so classify_way() never saw
     them: each ring is added to lines here as a walk-only Line. */
static void collect_plazas(const OsmData &osm, const vector < vector <long long> > &campus,
                           vector <Plaza> &plazas, vector <Line> &lines)
{
  vector < vector <long long> > rings, outer, inner;
  size_t i, r;
  Line line;
  const OsmWay *w;
  const OsmRelation *rel;

  for (i = 0; i < osm.ways.size(); i++) {
    w = &osm.ways[i];
    if (tag(w->tags, "highway") != "pedestrian" || tag(w->tags, "area") != "yes") continue;
    if (w->refs.size() < 4 || w->refs.front() != w->refs.back()) continue;
    if (!classify_way(*w, line) || !line.walk) continue;    // e.g. access=private
    rings.assign(1, w->refs);
    if (rings_touch_campus(rings, osm, campus)) add_plaza(rings, plazas);
  }

  for (i = 0; i < osm.relations.size(); i++) {
    rel = &osm.relations[i];
    if (tag(rel->tags, "type") != "multipolygon" ||
        tag(rel->tags, "highway") != "pedestrian") continue;
    if (!member_rings(osm, *rel, "outer", outer) || !member_rings(osm, *rel, "inner", inner)) {
      fprintf(stderr, "plaza relation %lld: a member way is missing, skipped\n", rel->id);
      continue;
    }
    if (outer.empty() || !rings_touch_campus(outer, osm, campus)) continue;
    rings = outer;
    rings.insert(rings.end(), inner.begin(), inner.end());
    add_plaza(rings, plazas);

    for (r = 0; r < rings.size(); r++) {
      line.refs = rings[r];
      line.highway = "pedestrian";
      line.name = tag(rel->tags, "name");
      line.car = false;
      line.walk = true;
      line.oneway = false;
      line.speed = 0;
      line.capacity = 0;
      lines.push_back(line);
    }
  }
}

/* ------------------------------------------------------------------ */
/* Step 6: find graph nodes and cut lines into edges (rubric 2.1)      */
/* ------------------------------------------------------------------ */

/* A node becomes a graph node if it is the first or last node of a line
   (dead ends need to be nodes), or it shows up 2+ times across all lines
   (shared by two paths, or a loop). Endpoints count 2, other uses 1, and
   every node with a count of 2 or more is kept. */
static void find_graph_nodes(const vector <Line> &lines, OutGraph &g)
{
  map <long long, int> uses;
  map <long long, int>::const_iterator it;
  size_t i, j;

  for (i = 0; i < lines.size(); i++) {
    for (j = 0; j < lines[i].refs.size(); j++) {
      uses[lines[i].refs[j]] += (j == 0 || j + 1 == lines[i].refs.size()) ? 2 : 1;
    }
  }
  for (it = uses.begin(); it != uses.end(); it++) {
    if (it->second >= 2) g.nodes.insert(it->first);
  }
}

/* Adds the edge(s) for one piece of a line, from graph node a to graph
   node b, length meters long.
   - The forward edge carries the line's car and walk flags.
   - The backward edge is the same, except that cars can't go back on a
     one-way line. It's dropped if nobody can use it.
   - Car edges get the line's capacity and free_flow = length / speed.
     Walk-only edges get capacity 0 and free_flow 0 (the file format
     requires it); walking time comes from length in the routing code. */
static void add_line_edges(const Line &line, long long a, long long b, double length,
                           OutGraph &g)
{
  OutEdge e;

  e.length = length;
  e.plaza = false;
  e.name = line.name;
  e.walk = line.walk;

  e.from = a;
  e.to = b;
  e.car = line.car;
  e.capacity = e.car ? line.capacity : 0;
  e.free_flow = e.car ? length / line.speed : 0;
  g.edges.push_back(e);

  e.from = b;
  e.to = a;
  e.car = line.car && !line.oneway;
  e.capacity = e.car ? line.capacity : 0;
  e.free_flow = e.car ? length / line.speed : 0;
  if (e.car || e.walk) g.edges.push_back(e);
}

/* Walks each line's refs, adding up haversine() between neighbors. Each
   time it reaches a graph node, the piece since the last graph node
   becomes an edge (each way) and the length starts over. A piece that
   starts and ends at the same node (a closed loop with nothing else
   attached) can't help any route, so it's skipped. */
static void split_lines(const vector <Line> &lines, const OsmData &osm, OutGraph &g)
{
  size_t i, j;
  long long last;
  double length;

  for (i = 0; i < lines.size(); i++) {
    const Line &line = lines[i];
    last = line.refs[0];
    length = 0;
    for (j = 1; j < line.refs.size(); j++) {
      length += node_distance(osm, line.refs[j - 1], line.refs[j]);
      if (g.nodes.count(line.refs[j]) == 0) continue;
      if (line.refs[j] != last) add_line_edges(line, last, line.refs[j], length, g);
      last = line.refs[j];
      length = 0;
    }
  }
}

/* Outline of every building, for the plaza crossing check and step 7. */
class BuildingShape {
  public:
    string name;                            // "" if the building has none
    vector < vector <long long> > rings;    // outer rings only
};

/* Ways with a building tag, and multipolygon relations with a building
   tag (their outer rings; e.g. Neyland Stadium is mapped that way). */
static vector <BuildingShape> building_shapes(const OsmData &osm)
{
  vector <BuildingShape> shapes;
  BuildingShape b;
  size_t i;

  for (i = 0; i < osm.ways.size(); i++) {
    if (tag(osm.ways[i].tags, "building").empty()) continue;
    if (osm.ways[i].refs.size() < 4 || osm.ways[i].refs.front() != osm.ways[i].refs.back()) continue;
    b.name = tag(osm.ways[i].tags, "name");
    b.rings.assign(1, osm.ways[i].refs);
    shapes.push_back(b);
  }
  for (i = 0; i < osm.relations.size(); i++) {
    const OsmRelation &rel = osm.relations[i];
    if (tag(rel.tags, "type") != "multipolygon" || tag(rel.tags, "building").empty()) continue;
    b.name = tag(rel.tags, "name");
    try {
      if (!member_rings(osm, rel, "outer", b.rings)) {
        fprintf(stderr, "building relation %lld: a member way is missing, skipped\n", rel.id);
        continue;
      }
    } catch (const runtime_error &e) {
      fprintf(stderr, "building relation %lld: %s, skipped\n", rel.id, e.what());
      continue;
    }
    if (!b.rings.empty()) shapes.push_back(b);
  }
  return shapes;
}

/* A segment in flat x/y, with the OSM ids of its ends. */
class Seg {
  public:
    long long a, b;
    double ax, ay, bx, by;
};

static vector <Seg> ring_segments(const vector < vector <long long> > &rings, const OsmData &osm)
{
  vector <Seg> segs;
  Seg s;
  size_t r, j;

  for (r = 0; r < rings.size(); r++) {
    for (j = 0; j + 1 < rings[r].size(); j++) {
      s.a = rings[r][j];
      s.b = rings[r][j + 1];
      to_xy(node_at(osm, s.a), s.ax, s.ay);
      to_xy(node_at(osm, s.b), s.bx, s.by);
      segs.push_back(s);
    }
  }
  return segs;
}

/* True if the straight line a-b touches any of segs, not counting
   segments that have a or b as an end. */
static bool blocked(long long a, long long b, double ax, double ay, double bx, double by,
                    const vector <Seg> &segs)
{
  size_t i;

  for (i = 0; i < segs.size(); i++) {
    const Seg &s = segs[i];
    if (s.a == a || s.a == b || s.b == a || s.b == b) continue;
    if (segments_touch(ax, ay, bx, by, s.ax, s.ay, s.bx, s.by)) return true;
  }
  return false;
}

/* Bounding box of some segments, in flat x/y. */
static void seg_box(const vector <Seg> &segs, double box[4])
{
  size_t i;

  box[0] = box[1] = 1e18;
  box[2] = box[3] = -1e18;
  for (i = 0; i < segs.size(); i++) {
    box[0] = min(box[0], min(segs[i].ax, segs[i].bx));
    box[1] = min(box[1], min(segs[i].ay, segs[i].by));
    box[2] = max(box[2], max(segs[i].ax, segs[i].bx));
    box[3] = max(box[3], max(segs[i].ay, segs[i].by));
  }
}

static bool boxes_overlap(const double a[4], const double b[4])
{
  return a[0] <= b[2] && b[0] <= a[2] && a[1] <= b[3] && b[1] <= a[3];
}

/* Straight-across walk edges inside each plaza.
   1. Entry points = outline nodes that some OTHER line also uses (a
      footway reaching the plaza edge, or a neighboring plaza), plus graph
      nodes that sit inside the plaza (3 footways dead-end inside the
      McClung plaza without touching its edge). A line is part of this
      plaza's own outline if all of its nodes are on the outline.
   2. Every pair of entry points (a, b) gets a two-way walk-only edge if
      the straight line a-b
        - doesn't touch any ring edge, except edges that have a or b as an
          end, and
        - has its midpoint inside the plaza. This catches lines across the
          open part of an L-shaped plaza.
   3. Length = haversine(a, b). The outline edges stay too.
   As a check, a crossing that touches a building outline (other than at
   a or b) is dropped and counted; there should be none. */
static void add_plaza_edges(const vector <Plaza> &plazas, const vector <Line> &lines,
                            const OsmData &osm, OutGraph &g)
{
  map <long long, vector <int> > node_lines;
  vector <BuildingShape> shapes;
  vector < vector <Seg> > building_segs;
  vector < vector <double> > building_boxes;
  set <long long> entry_set;
  set <long long>::const_iterator nit;
  vector <long long> entries;
  map <int, bool> own_line;
  vector <Seg> segs;
  size_t p, i, j, k, m;
  int li, crossings, through_building, plazas_with_crossings;
  double ax, ay, bx, by, x, y, pbox[4], box[4], ebox[4];
  bool own, hit;
  OutEdge e;

  for (i = 0; i < lines.size(); i++) {
    for (j = 0; j < lines[i].refs.size(); j++) node_lines[lines[i].refs[j]].push_back((int) i);
  }

  shapes = building_shapes(osm);
  for (i = 0; i < shapes.size(); i++) {
    building_segs.push_back(ring_segments(shapes[i].rings, osm));
    seg_box(building_segs.back(), box);
    building_boxes.push_back(vector <double>(box, box + 4));
  }

  crossings = 0;
  through_building = 0;
  plazas_with_crossings = 0;
  for (p = 0; p < plazas.size(); p++) {
    const Plaza &pl = plazas[p];

    /* 1. entry points */
    entry_set.clear();
    own_line.clear();
    for (nit = pl.outline.begin(); nit != pl.outline.end(); nit++) {
      const vector <int> &users = node_lines[*nit];
      for (k = 0; k < users.size(); k++) {
        li = users[k];
        if (own_line.count(li) == 0) {
          own = true;
          for (m = 0; m < lines[li].refs.size() && own; m++) {
            if (pl.outline.count(lines[li].refs[m]) == 0) own = false;
          }
          own_line[li] = own;
        }
        if (!own_line[li]) entry_set.insert(*nit);
      }
    }
    segs = ring_segments(pl.rings, osm);
    seg_box(segs, pbox);
    for (nit = g.nodes.begin(); nit != g.nodes.end(); nit++) {
      if (pl.outline.count(*nit) != 0) continue;
      to_xy(node_at(osm, *nit), x, y);
      if (x < pbox[0] || x > pbox[2] || y < pbox[1] || y > pbox[3]) continue;
      if (inside_rings(x, y, pl.rings, osm)) entry_set.insert(*nit);
    }
    entries.assign(entry_set.begin(), entry_set.end());

    /* 2. pairs */
    k = 0;
    for (i = 0; i < entries.size(); i++) {
      for (j = i + 1; j < entries.size(); j++) {
        to_xy(node_at(osm, entries[i]), ax, ay);
        to_xy(node_at(osm, entries[j]), bx, by);
        if (blocked(entries[i], entries[j], ax, ay, bx, by, segs)) continue;
        if (!inside_rings((ax + bx) / 2, (ay + by) / 2, pl.rings, osm)) continue;

        ebox[0] = min(ax, bx); ebox[1] = min(ay, by);
        ebox[2] = max(ax, bx); ebox[3] = max(ay, by);
        hit = false;
        for (m = 0; m < shapes.size() && !hit; m++) {
          if (!boxes_overlap(ebox, &building_boxes[m][0])) continue;
          if (blocked(entries[i], entries[j], ax, ay, bx, by, building_segs[m])) hit = true;
        }
        if (hit) {
          through_building++;
          continue;
        }

        /* 3. the edge, both ways */
        e.length = node_distance(osm, entries[i], entries[j]);
        e.capacity = 0;
        e.free_flow = 0;
        e.car = false;
        e.walk = true;
        e.plaza = true;
        e.name = "";
        e.from = entries[i];
        e.to = entries[j];
        g.edges.push_back(e);
        e.from = entries[j];
        e.to = entries[i];
        g.edges.push_back(e);
        k++;
      }
    }
    crossings += (int) k;
    if (k > 0) plazas_with_crossings++;
  }
  fprintf(stderr, "plazas: %d, straight-across edges: %d (each two-way) in %d plazas, "
          "%d dropped because they touch a building\n",
          (int) plazas.size(), crossings, plazas_with_crossings, through_building);
}

/* ------------------------------------------------------------------ */
/* Step 7: buildings (rubric 2.3)                                      */
/* ------------------------------------------------------------------ */

/* Buildings are their own list (name + center), NOT attached to nodes. The
   routing code finds the nearest node to a building when it needs one.
   - Ways with a "building" tag AND a "name" tag that touch campus, plus
     named multipolygon buildings (Neyland Stadium, Humanities and Social
     Sciences, ...), which the scaffold's directions left out.
   - Center = average lat/lon of the outline points (the repeated last
     point of a closed ring is skipped).
   - OSM sometimes packs two names into one value with ";" (e.g. "Berry
     Hall;Thompson-Boling Assembly Center and Arena"). Each name is added
     with the same center.
   - Some names repeat (one building split into several outlines). Each
     name gets one entry, the average of all of its outline points. */
static void collect_buildings(const OsmData &osm, const vector < vector <long long> > &campus,
                              OutGraph &g)
{
  vector <BuildingShape> shapes;
  map <string, vector <double> > sums;       // name -> lat sum, lon sum, count
  map <string, vector <double> >::const_iterator it;
  vector <string> names;
  string rest, one;
  size_t i, r, j, semi;
  OutBuilding b;

  shapes = building_shapes(osm);
  for (i = 0; i < shapes.size(); i++) {
    if (shapes[i].name.empty() || !rings_touch_campus(shapes[i].rings, osm, campus)) continue;

    names.clear();
    rest = shapes[i].name;
    do {
      semi = rest.find(';');
      one = trim(rest.substr(0, semi));
      if (!one.empty()) names.push_back(one);
      rest = (semi == string::npos) ? "" : rest.substr(semi + 1);
    } while (semi != string::npos);

    for (j = 0; j < names.size(); j++) {
      vector <double> &s = sums[names[j]];
      if (s.empty()) s.assign(3, 0.0);
      for (r = 0; r < shapes[i].rings.size(); r++) {
        const vector <long long> &ring = shapes[i].rings[r];
        for (size_t n = 0; n + 1 < ring.size(); n++) {
          s[0] += node_at(osm, ring[n]).lat;
          s[1] += node_at(osm, ring[n]).lon;
          s[2] += 1;
        }
      }
    }
  }

  for (it = sums.begin(); it != sums.end(); it++) {
    b.name = it->first;
    b.lat = it->second[0] / it->second[2];
    b.lon = it->second[1] / it->second[2];
    g.buildings.push_back(b);
  }
  fprintf(stderr, "buildings: %d named\n", (int) g.buildings.size());
}

/* ------------------------------------------------------------------ */
/* Step 8: checks (rubric 2.4)                                         */
/* ------------------------------------------------------------------ */

/* Connected pieces, as a piece number for every node (-1 = not in any)
   and the size of every piece. */
class Pieces {
  public:
    vector <int> piece;
    vector <int> sizes;
    int largest;                    // piece number of the biggest, or -1
};

static void find_largest(Pieces &pc)
{
  size_t i;

  pc.largest = -1;
  for (i = 0; i < pc.sizes.size(); i++) {
    if (pc.largest < 0 || pc.sizes[i] > pc.sizes[pc.largest]) pc.largest = (int) i;
  }
}

/* Pieces of the walk graph, ignoring direction (walk edges come in pairs). */
static Pieces walk_pieces(int n, const vector < vector <int> > &adj, const vector <bool> &use)
{
  Pieces pc;
  vector <int> todo;
  int s, u;
  size_t k;

  pc.piece.assign(n, -1);
  for (s = 0; s < n; s++) {
    if (!use[s] || pc.piece[s] >= 0) continue;
    pc.sizes.push_back(0);
    pc.piece[s] = (int) pc.sizes.size() - 1;
    todo.assign(1, s);
    while (!todo.empty()) {
      u = todo.back();
      todo.pop_back();
      pc.sizes.back()++;
      for (k = 0; k < adj[u].size(); k++) {
        if (pc.piece[adj[u][k]] < 0) {
          pc.piece[adj[u][k]] = pc.piece[s];
          todo.push_back(adj[u][k]);
        }
      }
    }
  }
  find_largest(pc);
  return pc;
}

/* Strongly connected pieces of the car graph (Kosaraju, without
   recursion): nodes in one piece can all reach each other by car. */
static Pieces car_pieces(int n, const vector < vector <int> > &out,
                         const vector < vector <int> > &in, const vector <bool> &use)
{
  Pieces pc;
  vector <bool> seen;
  vector <int> order, todo;
  vector < pair <int, size_t> > stack;
  int s, u, v, i;

  /* Pass 1: nodes in the order their depth-first search finishes. */
  seen.assign(n, false);
  for (s = 0; s < n; s++) {
    if (!use[s] || seen[s]) continue;
    seen[s] = true;
    stack.assign(1, make_pair(s, (size_t) 0));
    while (!stack.empty()) {
      u = stack.back().first;
      if (stack.back().second < out[u].size()) {
        v = out[u][stack.back().second];
        stack.back().second++;
        if (!seen[v]) {
          seen[v] = true;
          stack.push_back(make_pair(v, (size_t) 0));
        }
      } else {
        order.push_back(u);
        stack.pop_back();
      }
    }
  }

  /* Pass 2: latest finisher first, search the reversed edges. */
  pc.piece.assign(n, -1);
  for (i = (int) order.size() - 1; i >= 0; i--) {
    s = order[i];
    if (pc.piece[s] >= 0) continue;
    pc.sizes.push_back(0);
    pc.piece[s] = (int) pc.sizes.size() - 1;
    todo.assign(1, s);
    while (!todo.empty()) {
      u = todo.back();
      todo.pop_back();
      pc.sizes.back()++;
      for (size_t k = 0; k < in[u].size(); k++) {
        v = in[u][k];
        if (pc.piece[v] < 0) {
          pc.piece[v] = pc.piece[s];
          todo.push_back(v);
        }
      }
    }
  }
  find_largest(pc);
  return pc;
}

static string piece_summary(const Pieces &pc)
{
  vector <int> sizes;
  ostringstream ss;
  size_t i;

  sizes = pc.sizes;
  sort(sizes.rbegin(), sizes.rend());
  ss << sizes.size() << " (sizes";
  for (i = 0; i < sizes.size() && i < 10; i++) ss << " " << sizes[i];
  if (sizes.size() > 10) ss << " ...";
  ss << ")";
  return ss.str();
}

static int mode_key(const OutEdge &e)
{
  return (e.car ? 2 : 0) + (e.walk ? 1 : 0);
}

/* - Edges with length <= 0 are reported and removed (there shouldn't be
     any).
   - Only the largest connected piece is kept, separately for each mode:
     for walking, a search ignoring direction; for cars, the largest
     strongly connected piece (nodes you can both reach and leave). An
     edge outside the car piece loses its car flag (and its capacity and
     free_flow); an edge outside the walk piece loses its walk flag; an
     edge with neither is removed.
   - Duplicate edges (same from, to, car, walk): the shortest is kept.
   - Nodes with no edges left are removed. Buildings aren't tied to nodes,
     so they all stay.
   - A summary goes to stderr. */
static void check_graph(OutGraph &g)
{
  map <long long, int> index;
  vector <long long> ids;
  vector < vector <int> > walk_adj, car_out, car_in;
  vector <bool> has_walk, has_car;
  vector <OutEdge> edges;
  map < pair < pair <long long, long long>, int >, size_t > best;
  map < pair < pair <long long, long long>, int >, size_t >::iterator bit;
  pair < pair <long long, long long>, int > key;
  set <long long>::const_iterator nit;
  Pieces walk, car;
  size_t i;
  int a, b, bad_length, car_lost, walk_lost, removed, dups, n_car, n_walk, n_plaza;
  int nodes_before;
  bool car_ok, walk_ok;

  nodes_before = (int) g.nodes.size();

  /* lengths */
  bad_length = 0;
  for (i = 0; i < g.edges.size(); i++) {
    if (g.edges[i].length > 0) edges.push_back(g.edges[i]);
    else bad_length++;
  }
  g.edges.swap(edges);

  /* pieces */
  for (nit = g.nodes.begin(); nit != g.nodes.end(); nit++) {
    index[*nit] = (int) ids.size();
    ids.push_back(*nit);
  }
  walk_adj.resize(ids.size());
  car_out.resize(ids.size());
  car_in.resize(ids.size());
  has_walk.assign(ids.size(), false);
  has_car.assign(ids.size(), false);
  for (i = 0; i < g.edges.size(); i++) {
    a = index.at(g.edges[i].from);
    b = index.at(g.edges[i].to);
    if (g.edges[i].walk) {
      walk_adj[a].push_back(b);
      walk_adj[b].push_back(a);
      has_walk[a] = has_walk[b] = true;
    }
    if (g.edges[i].car) {
      car_out[a].push_back(b);
      car_in[b].push_back(a);
      has_car[a] = has_car[b] = true;
    }
  }
  walk = walk_pieces((int) ids.size(), walk_adj, has_walk);
  car = car_pieces((int) ids.size(), car_out, car_in, has_car);

  car_lost = walk_lost = removed = 0;
  edges.clear();
  for (i = 0; i < g.edges.size(); i++) {
    OutEdge e = g.edges[i];
    a = index.at(e.from);
    b = index.at(e.to);
    car_ok = e.car && car.piece[a] == car.largest && car.piece[b] == car.largest;
    walk_ok = e.walk && walk.piece[a] == walk.largest && walk.piece[b] == walk.largest;
    if (e.car && !car_ok) car_lost++;
    if (e.walk && !walk_ok) walk_lost++;
    if (!car_ok && !walk_ok) {
      removed++;
      continue;
    }
    e.car = car_ok;
    e.walk = walk_ok;
    if (!e.car) {
      e.capacity = 0;
      e.free_flow = 0;
    }
    edges.push_back(e);
  }

  /* duplicates (the map also sorts the edges by from, to, mode) */
  dups = 0;
  for (i = 0; i < edges.size(); i++) {
    key = make_pair(make_pair(edges[i].from, edges[i].to), mode_key(edges[i]));
    bit = best.find(key);
    if (bit == best.end()) {
      best[key] = i;
    } else {
      dups++;
      if (edges[i].length < edges[bit->second].length) bit->second = i;
    }
  }
  g.edges.clear();
  g.nodes.clear();
  n_car = n_walk = n_plaza = 0;
  for (bit = best.begin(); bit != best.end(); bit++) {
    const OutEdge &e = edges[bit->second];
    g.edges.push_back(e);
    g.nodes.insert(e.from);
    g.nodes.insert(e.to);
    if (e.car) n_car++;
    if (e.walk) n_walk++;
    if (e.plaza) n_plaza++;
  }

  fprintf(stderr, "check: %d edges with length <= 0 removed, %d duplicate edges removed\n",
          bad_length, dups);
  fprintf(stderr, "check: walk pieces %s\n", piece_summary(walk).c_str());
  fprintf(stderr, "check: car pieces (strongly connected) %s\n", piece_summary(car).c_str());
  fprintf(stderr, "check: %d of %d nodes with car edges are in the largest car piece\n",
          car.largest < 0 ? 0 : car.sizes[car.largest],
          (int) count(has_car.begin(), has_car.end(), true));
  fprintf(stderr, "check: outside the largest pieces: %d edges lost car use, %d lost walk use, "
          "%d removed; %d of %d nodes left\n",
          car_lost, walk_lost, removed, (int) g.nodes.size(), nodes_before);
  fprintf(stderr, "check: %d nodes, %d directed edges: %d car, %d walk (%d plaza crossings)\n",
          (int) g.nodes.size(), (int) g.edges.size(), n_car, n_walk, n_plaza);
}

/* ------------------------------------------------------------------ */
/* Step 9: write the file                                              */
/* ------------------------------------------------------------------ */

/* NODE lines first (the loader needs nodes defined before edges), then
   EDGE lines, then BUILDING lines. lat/lon get %.7f (OSM's own
   precision), lengths and times %.2f. W edges have capacity 0 and
   free_flow 0; C/CW edges have both > 0. Comment lines start with "# "
   because the loader skips a line when its first word is "#". */
static void write_graph(ostream &out, const OutGraph &g, const OsmData &osm,
                        const string &source)
{
  char buf[256], date[32];
  time_t now;
  set <long long>::const_iterator nit;
  size_t i;
  int n_car, n_walk;

  now = time(NULL);
  strftime(date, sizeof(date), "%Y-%m-%d", gmtime(&now));
  n_car = n_walk = 0;
  for (i = 0; i < g.edges.size(); i++) {
    if (g.edges[i].car) n_car++;
    if (g.edges[i].walk) n_walk++;
  }

  out << "# UTK campus graph (roads, walkways and plazas) for the CS 303 route planner.\n";
  out << "# Made by bin/osm_convert from " << source << " on " << date << " (UTC).\n";
  out << "# Source: OpenStreetMap export from openstreetmap.org, " << osm.bounds << ".\n";
  out << "# Newest edit in the export: " << osm.newest << ".\n";
  out << "# Map data (c) OpenStreetMap contributors, ODbL (https://www.openstreetmap.org/copyright)\n";
  out << "#\n";
  out << "# Format:\n";
  out << "#   NODE id lat lon\n";
  out << "#   EDGE from_id to_id length_m capacity_veh_per_hr free_flow_s mode [name]\n";
  out << "#   BUILDING lat lon name\n";
  out << "# mode: C = cars only, W = walkers only, CW = both. W edges have capacity 0\n";
  out << "# and free_flow 0. Every EDGE line is one direction.\n";
  out << "#\n";
  out << "# " << g.nodes.size() << " nodes, " << g.edges.size() << " edges (" << n_car
      << " car, " << n_walk << " walk), " << g.buildings.size() << " buildings\n";

  for (nit = g.nodes.begin(); nit != g.nodes.end(); nit++) {
    const OsmNode &n = node_at(osm, *nit);
    snprintf(buf, sizeof(buf), "NODE %lld %.7f %.7f\n", *nit, n.lat, n.lon);
    out << buf;
  }
  for (i = 0; i < g.edges.size(); i++) {
    const OutEdge &e = g.edges[i];
    snprintf(buf, sizeof(buf), "EDGE %lld %lld %.2f %.0f %.2f %s", e.from, e.to, e.length,
             e.capacity, e.free_flow, e.car ? (e.walk ? "CW" : "C") : "W");
    out << buf;
    if (!e.name.empty()) out << " " << e.name;
    out << "\n";
  }
  for (i = 0; i < g.buildings.size(); i++) {
    const OutBuilding &b = g.buildings[i];
    snprintf(buf, sizeof(buf), "BUILDING %.7f %.7f ", b.lat, b.lon);
    out << buf << b.name << "\n";
  }
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
  map <long long, int>::const_iterator it;
  int car_lines, oneway_lines;

  if (argc != 2 && argc != 3) {
    fprintf(stderr, "usage: osm_convert map.osm [graph-file]\n");
    fprintf(stderr, "       (with no graph-file the graph goes to standard output)\n");
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

    it = osm.way_index.find(CAMPUS_WAY_ID);
    if (it == osm.way_index.end()) {
      throw runtime_error("campus outline (way " + to_string(CAMPUS_WAY_ID) + ") is missing");
    }
    campus.push_back(osm.ways[it->second].refs);
    if (campus[0].size() < 4 || campus[0].front() != campus[0].back()) {
      throw runtime_error("campus outline (way " + to_string(CAMPUS_WAY_ID) + ") isn't closed");
    }

    car_lines = oneway_lines = 0;
    for (i = 0; i < osm.ways.size(); i++) {                     // step 3
      if (classify_way(osm.ways[i], line) && touches_campus(line.refs, osm, campus)) {
        lines.push_back(line);
        if (line.car) car_lines++;
        if (line.oneway) oneway_lines++;
      }
    }
    fprintf(stderr, "ways kept: %d (%d usable by cars, %d of them one-way)\n",
            (int) lines.size(), car_lines, oneway_lines);
    collect_plazas(osm, campus, plazas, lines);                 // step 5

    find_graph_nodes(lines, g);                                 // step 6
    split_lines(lines, osm, g);
    fprintf(stderr, "graph nodes: %d, directed edges from ways: %d\n",
            (int) g.nodes.size(), (int) g.edges.size());
    add_plaza_edges(plazas, lines, osm, g);
    collect_buildings(osm, campus, g);                          // step 7
    check_graph(g);                                             // step 8
  } catch (const runtime_error &e) {
    fprintf(stderr, "%s: %s\n", argv[1], e.what());
    return 1;
  }

  if (argc == 3) {                                              // step 9
    fout.open(argv[2]);
    if (!fout.is_open()) {
      fprintf(stderr, "couldn't write %s\n", argv[2]);
      return 1;
    }
    write_graph(fout, g, osm, argv[1]);
    fout.close();
    if (fout.fail()) {
      fprintf(stderr, "error writing %s\n", argv[2]);
      return 1;
    }
  } else {
    write_graph(cout, g, osm, argv[1]);
    cout.flush();
    if (cout.fail()) {
      fprintf(stderr, "error writing standard output\n");
      return 1;
    }
  }
  fprintf(stderr, "wrote %d nodes, %d edges, %d buildings to %s\n",
          (int) g.nodes.size(), (int) g.edges.size(), (int) g.buildings.size(),
          (argc == 3) ? argv[2] : "standard output");
  return 0;
}
