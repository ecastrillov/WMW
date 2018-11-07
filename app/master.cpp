#include "lib/Graph.h"
#include "lib/OverlappingStructure.h"
#include "lib/Modularity.h"
#include "lib/DisjointSet.h"
#include "lib/Clustering.h"
#include "lib/IteratedStructuralSimilarity.h"
#include "lib/WeakMostWeak.h"
#include "lib/AsyncWeakMostWeak.h"
#include <getopt.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <vector>

  // Output format
#define PER_NODE_FORMAT 0
#define PER_COMMUNITY_FORMAT 1
  // WeakMostWeak mode
#define SYNC_WMW  0  // WeakMostWeak Communities
#define ASYNC_WMW 1  // Asynchronous version of WeakMostWeak algortihm

using namespace std;

int quiet = 0;
int file_gml_format = 0;
int output_format = PER_COMMUNITY_FORMAT;

int wmw_mode = SYNC_WMW;
int min_size = 3;
int community_def = NO_DEFINITION;
int overlap = -1;
int iss_iters = 5;
double crisp_threshold = 0.05;
int zero = 0;

fstream file_size_dist;
fstream file_edge_simis;
fstream file_membership_dist;
fstream file_gml;
clock_t begin_time;

void parseArgs(int argc, char **argv) {
  
  struct option long_options[] = {
    {"quiet",              no_argument,        0, 'q'},
    {"gml",                required_argument,  0, 'g'},
    {"input_file",         required_argument,  0, 'i'},
    {"output_file",        required_argument,  0, 'o'},
    {"output_format",      required_argument,  0, 'f'},
    {"node_numbering",     required_argument,  0, 'n'},
    {"size_dist_file",     required_argument,  0, 'z'},
    {"edge_sims_file",     required_argument,  0, 's'},
    {"asynchronous_wmw",   required_argument,  0, 'a'},
    {"memb_dist_file",     required_argument,  0, 'm'},
    {"min_comm_size",      required_argument,  0, 'K'},
    {"community_def",      required_argument,  0, 'C'},
    {"dss_iterations",     required_argument,  0, 'I'},    
    {"overlap",            required_argument,  0, 'O'},
    {"crisp_threshold",    required_argument,  0, 'T'},
    {0, 0, 0, 0}
  };
  int cmd, option_index = 0;

  while ((cmd = getopt_long (argc, argv, "qg:i:o:n:f:z:s:m:aK:C:I:O:T:", long_options, &option_index)) != -1) {
    if (cmd == 'q') {
      quiet = 1;
      freopen("/dev/null", "w", stderr);
    }
    else if (cmd == 'g') {
      file_gml.open(optarg, fstream::out | fstream::trunc);

      if (!file_gml.is_open()) {
        cerr << "Failed to open file_gml file " << optarg << endl;
        exit(-1);
      }
    }
    else if (cmd == 'i') {
      if (freopen(optarg, "r", stdin) == NULL) {
        cerr << "Failed to open input file " << optarg << endl;
        exit(-1);
      }
    }
    else if (cmd == 'o') {
      if (freopen(optarg, "w", stdout) == NULL) {
        cerr << "Failed to open output file " << optarg << endl;
        exit(-1);
      }
    }
    else if (cmd == 'n') {
      zero = atoi(optarg);
      
      if (zero < 0) {
        cerr << "Invalid option value for " << char(cmd) << ": " << optarg << endl;
        exit(-1);
      }
    }
    else if (cmd == 'f') {
      output_format = atoi(optarg);
      
      if (output_format < PER_NODE_FORMAT || output_format > PER_COMMUNITY_FORMAT) {
        cerr << "Invalid option value for " << char(cmd) << ": " << optarg << endl;
        exit(-1);
      }
    }
    else if (cmd == 'a') {
      wmw_mode = ASYNC_WMW;
    }
    else if (cmd == 'z') {
      file_size_dist.open(optarg, fstream::out | fstream::trunc);

      if (!file_size_dist.is_open()) {
        cerr << "Failed to open file_size_dist file " << optarg << endl;
        exit(-1);
      }
    }
    else if (cmd == 's') {
      file_edge_simis.open(optarg, fstream::out | fstream::trunc);

      if (!file_edge_simis.is_open()) {
        cerr << "Failed to open file_edge_simis file " << optarg << endl;
        exit(-1);
      }
    }
    else if (cmd == 'm') {
      file_membership_dist.open(optarg, fstream::out | fstream::trunc);

      if (!file_membership_dist.is_open()) {
        cerr << "Failed to open file_membership_dist file " << optarg << endl;
        exit(-1);
      }
    }
    else if (cmd == 'K') {
      min_size = atoi(optarg);
      
      if (min_size < 1) {
        cerr << "Invalid option value for " << char(cmd) << ": " << optarg << endl;
        exit(-1);
      }
    }
    else if (cmd == 'C') {
      community_def = atoi(optarg);
      
      if (community_def < NO_DEFINITION || community_def > MOST_WEAK_CLUSTER) {
        cerr << "Invalid option value for " << char(cmd) << ": " << optarg << endl;
        exit(-1);
      }
    }
    else if (cmd == 'O') {
      overlap = atoi(optarg);
      
      if (overlap < FUZZY_OVERLAP || overlap > DISJOINT_OVERLAP) {
        cerr << "Invalid option value for " << char(cmd) << ": " << optarg << endl;
        exit(-1);
      }
    }
    else if (cmd == 'I') {
      iss_iters = atoi(optarg);
      
      if (iss_iters < 1) {
        cerr << "Invalid option value for " << char(cmd) << ": " << optarg << endl;
        exit(-1);
      }
    }
    else if (cmd == 'T') {
      crisp_threshold = atof(optarg);
    }
    else if (cmd == '?') {
      cerr << "Please read the manual pages" << endl;
      exit(-1);
    }
  }
}

void startTimer() {

  cerr.flush();
  begin_time = clock();
}

void endTimer() {
  cerr << "Elapsed time: " << double(clock()-begin_time)/CLOCKS_PER_SEC << " sec(s)" << endl;
}

Graph *readGraph() {
 
  int iu, iv, n = 0, i = 0;
  int u, v;
  vector<int> vu, vv;

  while (cin >> u >> v) {
    u -= zero;
    v -= zero;
    vu.push_back(u);
    vv.push_back(v);
    n = max(n, max(u, v));
  }
  Graph *g = new Graph(n+1);
  g->addEdges(vu, vv);
  vu.clear();
  vv.clear();
  return g;
}

void outputGml(int *membership, Graph &input_graph, double **extS) {
 
  if (!file_gml.is_open()) {
    return;
  }
  Graph &g = input_graph;
  
  file_gml << fixed << setprecision(2);
  file_gml << "graph [\n";
  file_gml << "directed 0\n";
    
  for (int i = 0; i < g.num_vertices; i++) {
    file_gml << "node [\n";
    file_gml << "id " << i+zero << "\n";
    file_gml << "co " << membership[i] << "\n";
    file_gml << "]\n";
  }
  for (int i = 0; i < g.num_edges; i++) {
    file_gml << "edge [\n";
    file_gml << "source " << g.src[i]+zero << "\n";
    file_gml << "target " << g.dst[i]+zero << "\n";
    
    if (extS) {
      for (int j = 0; j < g.adj_sz[g.src[i]]; j++) {
        if (g.adj[g.src[i]][j] == g.dst[i]) {
          file_gml << "label " << extS[g.src[i]][j] << "\n";
        }
      }
    }
    file_gml << "]\n";
  }
  file_gml << "]\n";
  file_gml.close();
}

bool fuzzy_cmp(pair<int, double> &a, pair<int, double> &b) {
  if (a.second < b.second) return true;
  if (a.second > b.second) return false;
  return a.first < b.first;
}

void outputFuzzy(Cover *fuzzy, Graph &input_graph) {

  int n = input_graph.num_vertices;
  
  if (output_format == PER_NODE_FORMAT) {
    for (int u = 0; u < n; u++) {
      if (fuzzy[u].size()) {
        sort(fuzzy[u].begin(), fuzzy[u].end(), fuzzy_cmp);
        cout << u+zero;
      
        for (auto &p : fuzzy[u]) {
          cout << " " << p.first+zero << " " << p.second;
        }
        cout << endl;
      }
    }
  } else {
    vector<pair<double, int> > *community = new vector<pair<double, int> >[n];
    
    for (int u = 0; u < n; u++) {
      for (auto &p : fuzzy[u]) {
        community[p.first].push_back(make_pair(p.second, u));
      }
    }
    for (int c = 0; c < n; c++) {
      if (community[c].size()) {
        sort(community[c].begin(), community[c].end());
        
        for (auto &u : community[c]) {
          cout << u.second+zero << " " << u.first << " ";
        }
        cout << endl;
      }
    }
    delete [] community;
  }
}

void outputCrisp(vector<int> *crisp, Graph &input_graph) {

  int n = input_graph.num_vertices;
  
  if (output_format == PER_NODE_FORMAT) {
    for (int u = 0; u < n; u++) {
      if (crisp[u].size()) {
        sort(crisp[u].begin(), crisp[u].end());
        cout << u+zero;
        
        for (int com : crisp[u]) {
          cout << " " << com+zero;
        }
        cout << endl;
      }
    }
  } else {
    vector<int> *community = new vector<int>[n];
    
    for (int u = 0; u < n; u++) {
      for (int c : crisp[u]) {
        community[c].push_back(u);
      }
    }
    for (int c = 0; c < n; c++) {
      if (community[c].size()) {
        sort(community[c].begin(), community[c].end());
        
        for (int u : community[c]) {
          cout << u+zero << " ";
        }
        cout << endl;
      }
    }
    delete [] community;
  }
}

void outputDisjoint(int *membership, Graph &input_graph) {
  
  int n = input_graph.num_vertices;
    
  if (output_format == PER_NODE_FORMAT) {
    for (int i = 0; i < n; i++) {
      cout << i+zero << " " << membership[i]+zero << "\n";
    }
  } else {
    vector<int> *community = new vector<int>[n];
    
    for (int u = 0; u < n; u++) {
      community[membership[u]].push_back(u);
    }
    for (int c = 0; c < n; c++) {
      if (!community[c].empty()) {
        sort(community[c].begin(), community[c].end());
        
        for (int u : community[c]) {
          cout << u+zero << " ";
        }
        cout << "\n";
      }
    }
    delete [] community;
  }
}

void outputSizeDistribution(Cover *fuzzy, vector<int> *crisp, int *membership, Graph &input_graph) {

  int n = input_graph.num_vertices;
  int min_sz = 1e9;
  int max_sz = 0;
  int num_comms = 0;
  int num_comms_min_size = 0;
  int *freq = new int[n+1];
  int *com_size = new int[n+1];
  
  memset(freq, 0, (n+1)*sizeof(int));
  memset(com_size, 0, (n+1)*sizeof(int));
  
  if (overlap == FUZZY_OVERLAP) {
    for (int u = 0; u < n; u++) {
      for (auto &p : fuzzy[u]) {
        com_size[p.first]++;
      }
    }
  }
  else if (overlap == CRISP_OVERLAP) {
    for (int u = 0; u < n; u++) {  
      for (int c : crisp[u]) {
        com_size[c]++;
      }
    }
  }
  else {
    for (int u = 0; u < n; u++) {  
      com_size[membership[u]]++;
    }
  }
  for (int c = 0; c < n; c++) {
    int sz = com_size[c];
    
    if (sz) {
      num_comms++;
      freq[sz]++;

      if (sz >= min_size) {
        min_sz = min(min_sz, sz);
        max_sz = max(max_sz, sz);
        num_comms_min_size++;
      }
    }
  }
  if (file_size_dist.is_open()) {
    file_size_dist << "community_size\tfrequency\n";

    for (int i = 1; i <= n; i++) {
      if (freq[i]) {
        file_size_dist << i << "\t" << freq[i] << "\n";
      }
    }
    file_size_dist.close();
  }
  delete [] freq;
  delete [] com_size;
  
  int w = 18;
  cerr << string(4*w, '-') << endl;
  cerr << setw(w) << "communities size>1" << setw(w) << "fit size" << setw(w) << "min size" << setw(w) << "max size" << endl;
  cerr << setw(w) << num_comms << setw(w) << setw(w) << num_comms_min_size << setw(w) << min_sz << setw(w) << max_sz << endl;
}

void outputMembershipDistribution(Cover *fuzzy, vector<int> *crisp, Graph &input_graph) {
   
  if (overlap < FUZZY_OVERLAP || overlap > CRISP_OVERLAP) {
    return;
  }
  int n = input_graph.num_vertices;
  int min_mem = 1e9;
  int max_mem = 0;
  int num_mem = 0;
  int *freq = new int[n+1];
  int *mem_size = new int[n+1];
  
  memset(freq, 0, (n+1)*sizeof(int));
  memset(mem_size, 0, (n+1)*sizeof(int));
  
  if (overlap == FUZZY_OVERLAP) {
    for (int u = 0; u < n; u++) {
      int sz = fuzzy[u].size();
      
      if (sz) {
        num_mem += freq[sz] == 0;
        freq[sz]++;
        min_mem = min(min_mem, sz);
        max_mem = max(max_mem, sz);
      }
    }
  }
  else if (overlap == CRISP_OVERLAP) {
    for (int u = 0; u < n; u++) {  
      int sz = crisp[u].size();
      
      if (sz) {
        num_mem += freq[sz] == 0;
        freq[sz]++;
        min_mem = min(min_mem, sz);
        max_mem = max(max_mem, sz);
      }
    }
  }
  if (file_membership_dist.is_open()) {
    file_membership_dist << "num_memberships\tfrequency\n";

    for (int i = 1; i <= n; i++) {
      if (freq[i]) {
        file_membership_dist << i << "\t" << freq[i] << "\n";
      }
    }
    file_membership_dist.close();
  }
  delete [] freq;
  delete [] mem_size;
  
  int w = 18;
  cerr << string(3*w, '-') << endl;
  cerr << setw(w) << "memberships>1" << setw(w) << "min memberships" << setw(w) << "max memberships" << endl;
  cerr << setw(w) << num_mem << setw(w) << min_mem << setw(w) << max_mem << endl;
}

void outputEdgeSimilarity(Graph &input_graph, double **extS) {

  if (!file_edge_simis.is_open()) {
    return;
  }
  for (int u = 0; u < input_graph.num_vertices; u++) {
    for (int i = 0; i < input_graph.adj_sz[u]; i++) {
      int v = input_graph.adj[u][i];

      if (u < v) {
        file_edge_simis << u+zero << " " << v+zero << " " << extS[u][i] << endl;
      }
    }
  }
  file_edge_simis.close();
}

int main(int argc, char **argv) {
  ios_base::sync_with_stdio(false); cout.tie(0); cin.tie(0);
  parseArgs(argc, argv);
  //cerr << fixed << setprecision(5);
  cout << fixed << setprecision(3);
  
  Graph *input_graph = 0;
  Clustering *communities = 0;
  Cover *fuzzy = 0;
  vector<int> *crisp = 0;
  int *membership = 0;
    
  startTimer();
    cerr << "*** Loading graph... " << endl;
    input_graph = readGraph();
  endTimer();
  
  cerr << "- Vertices: " << input_graph->num_vertices << endl << "- Edges: " << input_graph->num_edges << endl << endl;
  double **extS = 0;
  double *S = 0;
  pair<Clustering *, double **> clustering;
  startTimer();
  
  if (wmw_mode == SYNC_WMW) {
    cerr << "*** Running Iterated Structural Similarity -> sync Weak Most Weak ***" << endl;  
    clustering = WeakMostWeak::cluster(*input_graph, community_def, min_size, iss_iters);
  }
  else if (wmw_mode == ASYNC_WMW) {
    cerr << "*** Running Iterated Structual Similarity -> async Weak Most Weak ***" << endl;
    clustering = AsyncWeakMostWeak::cluster(*input_graph, community_def, min_size, iss_iters);
  }
  extS = clustering.second;
  communities = clustering.first;
  
  if (overlap != -1) {
    cerr << "@@@ Computing Overlapping Structure @@@" << endl;
    fuzzy = OverlappingStructure::fuzzy(*communities, extS);
    
    if (overlap == CRISP_OVERLAP) {
      crisp = OverlappingStructure::crisp(*communities, fuzzy, crisp_threshold);
    }
    else if (overlap == FUZZY_OVERLAP) {
    }
    else {
      membership = OverlappingStructure::disjoint(*communities, fuzzy);
    }
  } else {
    membership = new int[input_graph->num_vertices];
    
    for (int i = 0; i < input_graph->num_vertices; i++) {
      membership[i] = communities->getMembership(i);
    }
  }
  endTimer();
  delete communities;  
  outputSizeDistribution(fuzzy, crisp, membership, *input_graph);
  outputMembershipDistribution(fuzzy, crisp, *input_graph);
  outputEdgeSimilarity(*input_graph, extS);
  
  if (overlap == FUZZY_OVERLAP) {
    outputFuzzy(fuzzy, *input_graph);
    delete [] fuzzy;
  }
  else if (overlap == CRISP_OVERLAP) {
    outputCrisp(crisp, *input_graph);
    delete [] crisp;
  }
  else {
    cerr << fixed << setprecision(3) << "- Modularty: " << Modularity::compute(*input_graph, membership) << endl;
    outputDisjoint(membership, *input_graph);
    outputGml(membership, *input_graph, extS);
    delete [] membership;
  }
  for (int u = 0; u < input_graph->num_vertices; u++) {
    delete [] extS[u];
  }
  delete [] extS;
  delete input_graph;
  return 0;
}
