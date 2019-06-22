/**
 * basic usage of the library
 * by R. Falque
 * 22/06/2019
 **/

#include "graph_lib/graphStructure.hpp"
#include "graph_lib/readGraphOBJ.hpp"
#include "graph_lib/plotGraph.hpp"
#include "graph_lib/options.hpp"

#include <yaml-cpp/yaml.h>

int main(int argc, char* argv[])
{
  options opts;
  opts.loadYAML("../config.yaml");

  Graph graph("../data/graphs_test/6_disconnected.obj", opts.verbose);
  graph.init();

  bool is_connected, is_biconnected, is_triconnected;
  is_connected = graph.is_connected();
  is_biconnected = graph.is_biconnected();
  is_triconnected = graph.is_triconnected();
  
  graph.plot();

  return 0;
}
