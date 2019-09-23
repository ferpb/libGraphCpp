#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <algorithm>           // std::all_of
#include <Eigen/Dense>
#include <limits>

#include "readGraphOBJ.hpp"
#include "writeGraphOBJ.hpp"
#include "plotGraph.hpp"
#include "graphOptions.hpp"


/* TODO: check adjacency_list
 *       add linear triconnectivity implementation
 * 
 * 		- add cycle detection https://www.geeksforgeeks.org/print-all-the-cycles-in-an-undirected-graph/
 * 
 * 		- merge 	Graph(Eigen::MatrixXd nodes, Eigen::MatrixXi adjacency_matrix)
 * 					Graph(Eigen::MatrixXd nodes, Eigen::Matrix<int, Eigen::Dynamic, 2> edges)
 */
namespace libgraphcpp
{

	class Graph
	{
	private:
		graphOptions opts_;                                                      // Store all visualization infos

		// basic structure for edges and nodes
		Eigen::MatrixXd nodes_;                                             // should be a N by 3 matrix of doubles
		Eigen::MatrixXi edges_;                                             // should be a M by 2 matrix of integers
		int num_nodes_;                                                     // set to N
		int num_edges_;                                                     // set to M
		double scale_;                                                      // used to trim the tree in simplify_tree 
		
		// structures used for fast circulation through data
		std::vector< std::vector<int> > adjacency_list_;                    // contains for each nodes, its nodes neighbors
		std::vector< std::vector<int> > adjacency_edge_list_;               // contains for each nodes, its edges neighbors
		Eigen::VectorXd edges_length_;                                      // used only for Dijkstra
		Eigen::MatrixXi adjacency_matrix_;

		// connectivity properties
		int is_connected_ = -1;                                             // 0 no, 1 yes, -1 undefined
		int is_biconnected_ = -1;                                           // 0 no, 1 yes, -1 undefined
		int is_triconnected_ = -1;                                          // 0 no, 1 yes, -1 undefined
		int has_bridges_ = -1;                                              // 0 no, 1 yes, -1 undefined
		int has_cycle_ = -1;                                                // 0 no, 1 yes, -1 undefined

		// storing of the cut sets
		std::vector< int > one_cut_vertices_;                               // set of articulation points : vector of nodes ids
		std::vector< std::pair<int, int> > two_cut_vertices_;               // set of two-cut vertices    : vector of nodes ids pair
		std::vector<int> bridges_;                                          // set of briges              : vector of edges ids
		std::vector< std::vector<int> > cycles_;                            // set of cycles
		

		// internal functions: tools (defined at the bottom of the file)
		inline void removeRow(Eigen::MatrixXd& matrix, unsigned int rowToRemove);
		inline void removeRow(Eigen::MatrixXi& matrix, unsigned int rowToRemove);
		inline void removeDuplicates(std::vector<std::pair<int, int>>& v);
		inline bool is_element_in_vector(int a, std::vector<int> & A);
		inline bool return_colors_highlight(std::vector<int> element_to_highlight, int size_array, Eigen::MatrixXd &color);

		// internal functions: iterative functions (defined at the bottom of the file)
		inline void DFSUtil(int u, std::vector< std::vector<int> > adj, std::vector<bool> &visited);
		inline void APUtil(int u, std::vector<bool> & visited, int disc[], int low[], std::vector<int> & parent, std::vector<bool> & ap);
		inline void bridgeUtil(int u, std::vector<bool> & visited, int disc[], int low[], std::vector<int> & parent, std::vector<int> & bridges);
		inline void dfs_cycle(int u, int p, std::vector<int>& color, std::vector<int>& mark, std::vector<int>& par, int& cyclenumber) ;

	public:

		Graph(std::string file_name)
		{
			readGraphOBJ(file_name, nodes_, edges_);

			init();
		}

		// overload with options
		Graph(std::string file_name, graphOptions opts) : Graph(file_name) {
			opts_ = opts;
		}

		Graph(Eigen::MatrixXd nodes, Eigen::MatrixXi edges_input)
		{
			// test if edges is m by 2 (explicit edges) or n by n (adjacency matrix)
			if ( edges_input.cols() == 2 )
			{
				nodes_ = nodes;
				edges_ = edges_input;
			}
			else
			{
				Eigen::MatrixXi adjacency_matrix;
				adjacency_matrix = edges_input;

				if (adjacency_matrix.rows() != adjacency_matrix.cols() || adjacency_matrix.rows() != nodes.rows()) {
					std::cout << "size of the nodes matrix: " << nodes.rows() << ", " << nodes.cols() << "\n";
					std::cout << "size of the edges matrix: " << edges_input.rows() << ", " << edges_input.cols() << "\n";
					std::cout << "Error: wrong input size when assessing adjacency_matrix.rows() != adjacency_matrix.cols() || adjacency_matrix.rows() != nodes.rows()\n ";
					std::exit(0);
				}
				if (adjacency_matrix.transpose() != adjacency_matrix)
				{
					std::cout << "Error: the adjacency_matrix should be symmetric\n ";
					std::exit(0);
				}

				nodes_ = nodes;
				Eigen::MatrixXi edges((adjacency_matrix.array() != 0).count(), 2);

				// explore the upper triangle of the adjacency_matrix (without the diagonal)
				int num_edges = 0;
				for (int i=0; i<adjacency_matrix.rows(); i++)
					for (int j=i+1; j<adjacency_matrix.cols(); j++) {
						if (adjacency_matrix(i,j) != 0)
						{
							edges(num_edges, 0) = i;
							edges(num_edges, 1) = j;
							num_edges ++;
						}
					}
				edges.conservativeResize(num_edges, 2);

				edges_ = edges;
				adjacency_matrix_  = adjacency_matrix;
			}

			init();
		}

		// overload with options
		Graph(Eigen::MatrixXd nodes, Eigen::MatrixXi edges, graphOptions opts) : Graph(nodes, edges) {
			opts_ = opts;
		}

		// destructor
		~Graph()
		{
		}

		// save graph as OBJ file
		void save(std::string output_file)
		{
			writeGraphOBJ(nodes_, edges_, output_file);
		}

		// initialisation of the private variables
		bool init()
		{
			if (nodes_.cols()!=3 || edges_.cols()!=2) {
				std::cout << "Error: wrong graph dimensions" << std::endl;
				std::exit(1);
			}

			one_cut_vertices_.clear();
			two_cut_vertices_.clear();
			bridges_.clear();

			// set up properties
			num_nodes_ = nodes_.rows();
			num_edges_ = edges_.rows();

			// get scale
			Eigen::MatrixXd max_point = nodes_.colwise().maxCoeff();
			Eigen::MatrixXd min_point = nodes_.colwise().minCoeff();
			scale_ = (max_point - min_point).norm();

			// set up edges_length
			edges_length_ = Eigen::VectorXd::Zero(num_edges_);
			for (int i=0; i<num_edges_; i++)
				edges_length_(i) = (nodes_.row(edges_(i,0)) - nodes_.row(edges_(i,1)) ).norm();
			
			// set up the adjacency list
			set_adjacency_lists();

			return true;
		}

		void set_adjacency_lists()
		{
			// TODO: - the if statement should be removed for undirected graphs as they are not needed
			//       - is the adjacency_edge_list_ needed?

			// make sure the lists are empty to start with
			adjacency_list_.clear();
			adjacency_edge_list_.clear();
			
			adjacency_list_.resize(num_nodes_);
			adjacency_edge_list_.resize(num_nodes_);
			
			for (int i=0; i<num_edges_; i++)
			{
				// the if statements check if the node has already been inserted
				if (std::find (adjacency_list_[edges_(i, 0)].begin(), adjacency_list_[edges_(i, 0)].end(), edges_(i, 1))==adjacency_list_[edges_(i, 0)].end())
					adjacency_list_[edges_(i, 0)].push_back(edges_(i, 1));
				if (std::find (adjacency_list_[edges_(i, 1)].begin(), adjacency_list_[edges_(i, 1)].end(), edges_(i, 0))==adjacency_list_[edges_(i, 1)].end())
					adjacency_list_[edges_(i, 1)].push_back(edges_(i, 0));
				adjacency_edge_list_[edges_(i, 0)].push_back(i);
				adjacency_edge_list_[edges_(i, 1)].push_back(i);
			}
		}

		bool plot() {
			return polyscope_plot(nodes_, edges_);
		}


		bool plot_connectivity() 
		{
			connectivity_tests();
			Eigen::MatrixXd empty; 			// used to pass empty content
			Eigen::MatrixXd highlight;

			GraphVisualization viz;
			viz.add_graph(nodes_, edges_);

			// highlight one-cut-vertices
			return_colors_highlight(one_cut_vertices_, nodes_.rows(), highlight);
			viz.add_color(highlight, empty, "one-cut-vertices");

			// highlight two-cut-vertices
			std::vector <int> temp;
			for (std::pair<int, int> p: two_cut_vertices_)
			{
				temp.push_back(p.first);
				temp.push_back(p.second);
			}
			return_colors_highlight(temp, nodes_.rows(), highlight);
			viz.add_color(highlight, empty, "two-cut-vertices");

			// highlight bridges
			return_colors_highlight(bridges_, edges_.rows(), highlight);
			viz.add_color(empty, highlight, "bridges");

			viz.show();
			return true;
		}


		bool plot_and_highlight(std::vector<int> node_list, std::vector<int> edge_list)
		{
			// set default_values 
			Eigen::MatrixXd nodes_colors = Eigen::MatrixXd::Constant(nodes_.rows(),3,0.1);
			nodes_colors.col(0) = Eigen::MatrixXd::Constant(nodes_.rows(),1, 1.0);
			nodes_colors.col(1) = Eigen::MatrixXd::Constant(nodes_.rows(),1, 0.1);
			nodes_colors.col(2) = Eigen::MatrixXd::Constant(nodes_.rows(),1, 0.1);

			Eigen::MatrixXd edges_colors = Eigen::MatrixXd::Constant(edges_.rows(),3,0.1);
			edges_colors.col(0) = Eigen::MatrixXd::Constant(edges_.rows(),1, 0.1);
			edges_colors.col(1) = Eigen::MatrixXd::Constant(edges_.rows(),1, 0.1);
			edges_colors.col(2) = Eigen::MatrixXd::Constant(edges_.rows(),1, 0.1);

			for (int node: node_list) {
				nodes_colors(node, 0) = 0.1;
				nodes_colors(node, 2) = 1.0;
			}

			for (int edge: edge_list) {
				edges_colors(edge, 0) = 0.1;
				edges_colors(edge, 2) = 1.0;
			}

			polyscope_plot_with_color (nodes_, edges_, nodes_colors, edges_colors);

			return true;
		}


		bool print_isolated_vertices()
		{
			for (int i=0; i<num_nodes_; i++)
				if (adjacency_list_[i].size() == 0)
					std::cout << "Isolated vertices at: " << i << std::endl;
		}

		bool add_node(Eigen::Vector3d node, std::vector<int> neighbours)
		{
			// add node
			Eigen::MatrixXd temp_nodes = nodes_;
			nodes_.resize(nodes_.rows()+1, 3);
			nodes_ << temp_nodes, node.transpose();
			
			// add related edges
			Eigen::MatrixXi temp_edges = edges_;
			Eigen::MatrixXi edges_to_add(neighbours.size(), 2);
			for (int edge_iterator=0; edge_iterator<neighbours.size(); edge_iterator++)
				edges_to_add.row(edge_iterator) << nodes_.rows()-1, neighbours[edge_iterator];
			
			edges_.resize(temp_edges.rows()+edges_to_add.rows(), 2);
			edges_ << temp_edges, edges_to_add;

			init();

			return true;
		}

		bool remove_node(int nodeToRemove) 
		{
			// remove node
			removeRow(nodes_, nodeToRemove);

			// remove related edges
			std::vector<int> edges_to_remove = adjacency_edge_list_[nodeToRemove];
			std::sort(edges_to_remove.begin(), edges_to_remove.end(), std::greater<int>());
			for (int edge_iterator=0; edge_iterator<edges_to_remove.size(); edge_iterator++)
				removeRow(edges_, edges_to_remove[edge_iterator]);

			// update the edge nodes index (we have one less node now)
			for (int i=0; i<edges_.rows(); i++)
				for (int j=0; j<edges_.cols(); j++)
					if (edges_(i,j) >= nodeToRemove)
						edges_(i,j)--;

			init();

			return true;
		}

		bool merge_nodes(std::vector<int> nodes)
		{
			// store connected elements first
			std::vector<int> neighbours;
			for (int node : nodes)
				for (int neighour : adjacency_list_[node])
					neighbours.push_back(neighour);

			std::sort(neighbours.begin(), neighbours.end());
			neighbours.erase( std::unique( neighbours.begin(), neighbours.end() ), neighbours.end() );

			// find node position
			Eigen::Vector3d merged_node;
			merged_node << 0,0,0;
			for (int node : nodes)
				merged_node += nodes_.row(node);
			merged_node /= nodes.size();

			// add merged node (it is stacked at the end so it has to be done before deleting the nodes)
			add_node(merged_node, neighbours);
			
			// delete other nodes
			std::sort(nodes.begin(), nodes.end(), std::greater<int> ());
			for (int node : nodes)
				remove_node(node);
		}

		bool add_edge(Eigen::Vector2i edge)
		{
			// add node
			Eigen::MatrixXi temp_edges = edges_;
			edges_.resize(edges_.rows()+1, 3);
			edges_ << temp_edges, edge.transpose();
			
			init();

			return true;
		}

		bool remove_edge(int edgeToRemove) 
		{
			// remove edge
			removeRow(edges_, edgeToRemove);

			init();

			return true;
		}

		bool collapse_edge(int edge_id)
		{
			// get nodes_id:
			int node_1 = edges_(edge_id, 0);
			int node_2 = edges_(edge_id, 1);

			// merge the two previous ones
			Eigen::Vector3d fused_node = (nodes_.row(node_1) + nodes_.row(node_2) ) / 2;

			// get the connected neighbours
			std::vector<int> neighbours;
			neighbours = adjacency_list_[node_1];
			neighbours.insert( neighbours.end(), adjacency_list_[node_2].begin(), adjacency_list_[node_2].end() );

			// remove old nodes
			neighbours.erase(std::remove(neighbours.begin(), neighbours.end(), node_1), neighbours.end());
			neighbours.erase(std::remove(neighbours.begin(), neighbours.end(), node_2), neighbours.end());

			sort( neighbours.begin(), neighbours.end() );
			neighbours.erase( unique( neighbours.begin(), neighbours.end() ), neighbours.end() );

			// add node
			add_node(fused_node, neighbours);
			
			// remove the previous nodes (the order matter)
			if (node_2<node_1) {
				remove_node(node_1);
				remove_node(node_2);
			} else {
				remove_node(node_2);
				remove_node(node_1);
			}

			return true;
		}

		std::vector<std::vector <int> >  make_tree(Eigen::MatrixXi& deleted_edges)
		{
			std::vector<std::vector <int> > nodes_references(num_nodes_);
			for (int i=0; i<num_nodes_; i++)
				nodes_references[i].push_back(i);
			
			bool verbose_temp = opts_.verbose;
			opts_.verbose = false;

			// check for bridges
			has_bridges_ = -1;
			has_bridges();

			deleted_edges.resize(num_edges_ - num_nodes_ + 1, 2); // cycle rank * 2
			int counter = 0;

			while (bridges_.size() != num_edges_)
			{

				// list all non bridges
				std::vector <int> edges_to_edit;
				std::vector <double> edges_length;
				for (int i=0; i<num_edges_; i++)
					if (find (bridges_.begin(), bridges_.end(), i) == bridges_.end()) {
						edges_to_edit.push_back(i);
						edges_length.push_back(edges_length_(i));
					}

				// sort edges by decreasing length
				std::vector<std::pair<double, int>> sorting_container;
				sorting_container.reserve(edges_to_edit.size());
				std::transform(edges_length.begin(), edges_length.end(), edges_to_edit.begin(), std::back_inserter(sorting_container),
					[](double a, int b) { return std::make_pair(a, b); });

				std::sort(sorting_container.begin(), sorting_container.end()); 
				std::reverse(sorting_container.begin(), sorting_container.end());

				// store edges
				deleted_edges.row(counter) << edges_.row(sorting_container[0].second);
				counter ++;

				// actually store the edges
				//collapse_edge(edges_to_edit[0]);
				remove_edge(sorting_container[0].second);
				
				// check for bridges
				has_bridges_ = -1;
				has_bridges();
			}

			opts_.verbose = verbose_temp;
			return nodes_references;
		}

		// overload make_tree if deleted edges are not needed
		std::vector<std::vector <int> >  make_tree()
		{
			Eigen::MatrixXi deleted_edges;
			return make_tree(deleted_edges);
		}

		void simplify_tree()
		{
			/* 
			 * for each junctions:
			 * if the distance between two junction is "small"
			 * then merge all the nodes in the path
			 */
			std::vector<int> junction_list;
			start_junction_checking:
			junction_list.clear();
			for (int i=0; i<adjacency_list_.size(); i++)
				if (adjacency_list_[i].size()>2)
					junction_list.push_back(i);

			for (int i = 0; i<junction_list.size(); i++) {
				for (int j = i+1; j<junction_list.size(); j++) {
					std::vector<int> path;
					double junctions_distance = dijkstra(junction_list[i], junction_list[j], path);

					if (junctions_distance < scale_/10)
					{
						merge_nodes(path);
						goto start_junction_checking; // went throwing up after writing this
					}
				}
			}

			/* 
			 * for each leaf:
			 * if the distance to the closest junction is "small"
			 * then delete all nodes until this node is reached
			 */
			std::vector<int> leaves_list;
			junction_list.clear();
			for (int i=0; i<adjacency_list_.size(); i++){
				if (adjacency_list_[i].size()==1)
					leaves_list.push_back(i);
				
				if (adjacency_list_[i].size()>2)
					junction_list.push_back(i);
			}

			// for each leaves check the distance to each junction
			std::vector<int> nodes_to_delete;
			for (int leaf: leaves_list) {
				for (int junction: junction_list) {
					std::vector<int> path;
					double leaf_to_junction_distance = dijkstra(leaf, junction, path);

					if (leaf_to_junction_distance < scale_/10)
					{
						for (int node_id : path)
							if (node_id != junction)
								nodes_to_delete.push_back(node_id);
					}
				}
			}

			if (nodes_to_delete.size() != 0) {
				std::sort(nodes_to_delete.begin(), nodes_to_delete.end(), std::greater<int>());
				nodes_to_delete.erase( std::unique( nodes_to_delete.begin(), nodes_to_delete.end() ), nodes_to_delete.end() );
				for (int node_id : nodes_to_delete)
					remove_node(node_id);
			}

		}

		bool transform (double scale, Eigen::Vector3d move) 
		{
			nodes_ /= scale;
			nodes_ += move.transpose();
		}

		double dijkstra(int source, int target, std::vector<int>& node_path) 
		{
			// initialization
			double distance_source_to_target;
			std::vector<double> min_distance(num_nodes_, std::numeric_limits<double>::infinity());
			std::vector<int> previous_node(num_nodes_, -1);

			// set the set of visited nodes:
			std::vector<int> visited;
			std::vector<int> to_visit;

			// initialize the node to start from
			int u = source;
			min_distance.at(source) = 0;

			// start searching
			bool target_found;
			while (!target_found) {
				// check all neighbours of "u"
				for (int i = 0; i < adjacency_list_.at(u).size(); ++i) {
					int neighbour_node = adjacency_list_.at(u).at(i);
					double edge_length = edges_length_(adjacency_edge_list_.at(u).at(i));
					if (not is_element_in_vector(neighbour_node, to_visit))
						if (not is_element_in_vector(neighbour_node, visited))
							to_visit.push_back(neighbour_node);
					
					if ( min_distance.at(u) + edge_length < min_distance.at( neighbour_node ) ) {
						min_distance.at( neighbour_node ) = min_distance.at(u) +  edge_length;
						previous_node.at( neighbour_node ) = u;
					}
				}

				visited.push_back(u);

				// check if the visited point is in sub_V
				target_found = is_element_in_vector(target, visited);

				// set next u
				int index_of_next_point = 0;
				for (int i = 0; i < to_visit.size(); ++i)
					if (min_distance.at( to_visit.at(i) ) < min_distance.at( to_visit.at( index_of_next_point) ) )
						index_of_next_point = i;


				// check if all vertices have been visited:
				if (to_visit.size() == 0)
					break;

				u = to_visit.at(index_of_next_point);
				to_visit.erase(to_visit.begin() + index_of_next_point);
			}
			
			// backtracking to generate path
			if (min_distance.at(target) != std::numeric_limits<double>::infinity()) {
				int backtracking_node = target;
				node_path.insert(node_path.begin(), backtracking_node);
				
				while (backtracking_node != source) {
					node_path.insert(node_path.begin(), previous_node[backtracking_node]);
					backtracking_node = previous_node[backtracking_node];
				}
			}

			return min_distance.at(target);
		}

		double dijkstra(int source, int target)
		{
			std::vector<int> node_path;
			return  dijkstra(source, target, node_path);
		}

		/* CONNECTIVITY TESTS */
		bool connectivity_tests()
		{
			is_connected();
			is_biconnected();
			is_triconnected();
			has_bridges();
		}

		bool is_connected()
		{
			if (is_connected_ == -1) {
				// check for graph connectivity using DFS:
				std::vector<bool> visited(num_nodes_, false);
				DFSUtil(0, adjacency_list_, visited);
				is_connected_ = std::all_of(visited.begin(), visited.end(), [](bool v) { return v; });

				if (opts_.verbose)
					std::cout << "graph is connected: " << is_connected_ << std::endl;

				// if not connected, update the next ones
				if (is_connected_ == 0) {
					is_biconnected_ = 0;
					is_triconnected_ = 0;
				}
			}
			return is_connected_;
		}

		// return the set of one cut vertices
		bool is_biconnected(std::vector<int>& one_cut_vertices)
		{
			if (is_biconnected_ == -1) {
				// check for 2 node connectivity:
				std::vector<bool> visited(num_nodes_, false);
				std::vector<int> parent(num_nodes_, -1);
				std::vector<bool> ap(num_nodes_, false);
				int *disc = new int[num_nodes_];
				int *low = new int[num_nodes_];
				// Call the recursive helper function to find articulation points 
				// in DFS tree rooted with vertex '0' 
				for (int i = 0; i < num_nodes_; i++) 
					if (visited[i] == false) 
						APUtil(i, visited, disc, low, parent, ap);
				
				// Now ap[] contains articulation points, print them and store them into one_cut_vertices_
				one_cut_vertices.clear();
				for (int i = 0; i < num_nodes_; i++) 
					if (ap[i] == true) {
						one_cut_vertices.push_back(i);
						if (opts_.verbose)
							std::cout << "Articulation point " << i << " at: "  << nodes_.row(i) << std::endl;
					}
				is_biconnected_ = std::none_of(ap.begin(), ap.end(), [](bool v) { return v; });

				// if not biconnected, update triconnectivity
				if (is_biconnected_ == 0)
					is_triconnected_ = 0;
				
				one_cut_vertices_ = one_cut_vertices;
			}

			if (opts_.verbose)
				std::cout << "graph is 2-connected: " << is_biconnected_ << std::endl;

			return is_biconnected_;
		}

		// overload is_biconnected
		bool is_biconnected()
		{
			std::vector< int > one_cut_vertices;
			return is_biconnected(one_cut_vertices);
		}

		// return the set of two cut vertices
		bool is_triconnected(std::vector< std::pair<int, int> >& two_cut_vertices)
		{ 
			/* this function run in quadratic time, this is not the most efficient way to do it
			* for alternative, see these papers:
			* - Finding the Triconnected Components of a Graph (1972)
			* - A Linear Time Implementation of SPQR-Trees (2000)
			* and these implementations:
			* - http://www.ogdf.net/doku.php
			* - https://github.com/adrianN/Triconnectivity
			*/
			graphOptions opts = opts_;
			opts.verbose = false;
			if (is_triconnected_ == -1) {
				is_triconnected_ = true;
				for (int i=0; i<num_nodes_; i++) {
					std::vector< int > one_cut_vertices;
					Graph reduced_graph(nodes_, edges_, opts);
					reduced_graph.init();
					reduced_graph.remove_node(i);
					bool reduced_graph_is_biconnected = reduced_graph.is_biconnected(one_cut_vertices);

					if (not reduced_graph_is_biconnected)
						for (int j=0; j<one_cut_vertices.size(); j++) {
							int node_offset = (one_cut_vertices[j]>=i); // offset needed because a node has been deleted
							two_cut_vertices.push_back(std::make_pair(i, one_cut_vertices[j]+node_offset));
						}
					is_triconnected_ &= reduced_graph_is_biconnected;
				}

				removeDuplicates(two_cut_vertices);
				two_cut_vertices_ = two_cut_vertices;
			}



			if (opts_.verbose) {
				std::cout << "graph is 3-connected: " << is_triconnected_ << std::endl;
				for (int i = 0; i<two_cut_vertices.size(); i++)
					std::cout << "two_cut_vertices between node " << two_cut_vertices[i].first <<" and node " << two_cut_vertices[i].second << std::endl;
			}

			return is_triconnected_;
		}
		
		// overload is_triconnected
		bool is_triconnected()
		{ 
			std::vector< std::pair<int, int> > two_cut_vertices;
			return is_triconnected(two_cut_vertices);
		}

		// return the set of bridges
		bool has_bridges(std::vector<int>& bridges)
		{
			if (has_bridges_ == -1) {
				// check for bridges:

				std::vector<bool> visited(num_nodes_, false);
				std::vector<int> parent(num_nodes_, -1);
				int *disc = new int[num_nodes_]; 
				int *low = new int[num_nodes_];
				
				// Call the recursive helper function to find Bridges 
				// in DFS tree rooted with vertex 'i' 
				for (int i = 0; i < num_nodes_; i++) 
					if (visited[i] == false) 
						bridgeUtil(i, visited, disc, low, parent, bridges);
				
				delete[] disc;
				delete[] low;

				if (bridges.size() == 0)
					has_bridges_ = 0;
				else
					has_bridges_ = 1;
				
				bridges_ = bridges;
			}

			if (opts_.verbose)
				std::cout << "graph has bridges: " << has_bridges_ << std::endl;
			
			return has_bridges_;
		}

		// overload has_bridges
		bool has_bridges()
		{
			std::vector<int> bridges;
			return has_bridges(bridges);
		}

		// return a cycles basis (see: An Algorithm for Finding a Fundamental Set of Cycles of a Graph)
		bool has_cycles(std::vector <std::vector<int>>& cycle_basis)
		{
			graphOptions opts = opts_;
			opts.verbose = false;
			Graph spanning_tree(nodes_, edges_, opts);
			Eigen::MatrixXi deleted_edges;
			spanning_tree.make_tree(deleted_edges);

			for (int i=0; i<deleted_edges.rows(); i++) {
				std::vector<int> cycle;
				spanning_tree.dijkstra(deleted_edges(i, 0), deleted_edges(i, 1), cycle);
				cycle_basis.push_back(cycle);
			}
			
			if (cycle_basis.size() != 0)
				return true;
			else
				return false;
		}

		// overload has_cycle
		bool has_cycles()
		{
			std::vector <std::vector <int>> cycles;
			return has_cycles(cycles);
		}

		// accessors
		int num_nodes() { return num_nodes_; }
		int num_edges() { return num_edges_; }
		Eigen::MatrixXd get_nodes() { return nodes_; }
		Eigen::Vector3d get_node(int i) { return nodes_.row(i); }
		Eigen::MatrixXi get_edges() { return edges_; }
		Eigen::Vector2i get_edge(int i) { return edges_.row(i); }
		std::vector <int> get_adjacency_list(int i) { return adjacency_list_[i]; }
		int get_adjacency_list(int i, int j) { return adjacency_list_[i][j]; }
		
		/* TO BE REMOVED? */

		int edge_source(int i)
		{
			return edges_(i, 0);
		}

		int edge_target(int i) 
		{
			return edges_(i, 1);
		}

		void swap_edge(int i) 
		{
			int temp = edges_(i,0);
			edges_(i,0) = edges_(i,1);
			edges_(i,1) = temp;
		}

	};

	//https://stackoverflow.com/a/46303314/2562693
	void Graph::removeRow(Eigen::MatrixXd& matrix, unsigned int rowToRemove)
	{
		unsigned int numRows = matrix.rows()-1;
		unsigned int numCols = matrix.cols();

		if( rowToRemove < numRows )
			matrix.block(rowToRemove,0,numRows-rowToRemove,numCols) = matrix.bottomRows(numRows-rowToRemove);

		matrix.conservativeResize(numRows,numCols);
	};

	void Graph::removeRow(Eigen::MatrixXi& matrix, unsigned int rowToRemove)
	{
		unsigned int numRows = matrix.rows()-1;
		unsigned int numCols = matrix.cols();

		if( rowToRemove < numRows )
			matrix.block(rowToRemove,0,numRows-rowToRemove,numCols) = matrix.bottomRows(numRows-rowToRemove);

		matrix.conservativeResize(numRows,numCols);
	};

	//https://stackoverflow.com/a/32842128/2562693
	void Graph::removeDuplicates(std::vector<std::pair<int, int>>& v)
	{
		
		//Normalize == sort the pair members
		for(auto& p : v){
			int x = std::max(p.first, p.second), y = std::min(p.first, p.second);
			p.first = x; p.second = y;
		}

		//Sort the pairs
		std::sort(v.begin(), v.end());

		//Unique the vector
		auto last = unique(v.begin(), v.end() );
		v.erase(last, v.end());
	};

	bool Graph::is_element_in_vector(int a, std::vector<int> & A)
	{
		auto it = std::find(A.begin(), A.end(), a);
		return it != A.end();
	}

	bool Graph::return_colors_highlight(std::vector<int> element_to_highlight, int size_array, Eigen::MatrixXd & colors)
	{
		colors = Eigen::MatrixXd::Constant(size_array,3,0.1);

		for (int element: element_to_highlight) {
			colors(element, 0) = 0.1;
			colors(element, 1) = 0.1;
			colors(element, 2) = 1.0;
		}
	};

	// used for checking connectivity:
	//
	// for reference, see Kosaraju's algorithm (https://en.wikipedia.org/wiki/Kosaraju%27s_algorithm)
	//
	// from: https://www.geeksforgeeks.org/graph-implementation-using-stl-for-competitive-programming-set-1-dfs-of-unweighted-and-undirected/
	// A utility function to do DFS of graph 
	// recursively from a given vertex u. 
	void Graph::DFSUtil(int u, std::vector< std::vector<int> > adj, std::vector<bool> &visited) 
	{ 
		visited[u] = true;
		for (int i=0; i<adj[u].size(); i++) 
			if (visited[adj[u][i]] == false) 
				DFSUtil(adj[u][i], adj, visited); 
	};

	// used for finding articulation points (1-node-connectivity):
	//
	// for reference, see Tarjan’s algorithm
	//
	// from: https://www.geeksforgeeks.org/articulation-points-or-cut-vertices-in-a-graph/
	// A recursive function that find articulation points using DFS traversal 
	// u --> The vertex to be visited next 
	// visited[] --> keeps tract of visited vertices 
	// disc[] --> Stores discovery times of visited vertices 
	// parent[] --> Stores parent vertices in DFS tree 
	// ap[] --> Store articulation points 
	void Graph::APUtil(int u, std::vector<bool> & visited, int disc[],  
										int low[], std::vector<int> & parent, std::vector<bool> & ap) 
	{ 
		// A static variable is used for simplicity, we can avoid use of static 
		// variable by passing a pointer. 
		static int time = 0; 

		// Count of children in DFS Tree 
		int children = 0; 

		// Mark the current node as visited 
		visited[u] = true; 

		// Initialize discovery time and low value 
		disc[u] = low[u] = ++time; 

		for (int i=0; i<adjacency_list_[u].size(); i++) { 
			int v = adjacency_list_[u][i];  // v is current adjacent of u 

			// If v is not visited yet, then make it a child of u 
			// in DFS tree and recur for it 
			if (!visited[v]) { 
				children++; 
				parent[v] = u; 
				APUtil(v, visited, disc, low, parent, ap); 

				// Check if the subtree rooted with v has a connection to 
				// one of the ancestors of u 
				low[u]  = std::min(low[u], low[v]); 

				// u is an articulation point in following cases 

				// (1) u is root of DFS tree and has two or more chilren. 
				if (parent[u] == -1 && children > 1) 
				ap[u] = true; 

				// (2) If u is not root and low value of one of its child is more 
				// than discovery value of u. 
				if (parent[u] != -1 && low[v] >= disc[u]) 
				ap[u] = true; 
			} 

			// Update low value of u for parent function calls. 
			else if (v != parent[u]) 
				low[u]  = std::min(low[u], disc[v]); 
		} 
	};

	// used for finding articulation points (1-node-connectivity):
	//
	// for reference, see Tarjan’s algorithm
	//
	// from: https://www.geeksforgeeks.org/bridge-in-a-graph/
	// A recursive function that finds and prints bridges using 
	// DFS traversal 
	// u --> The vertex to be visited next 
	// visited[] --> keeps tract of visited vertices 
	// disc[] --> Stores discovery times of visited vertices 
	// parent[] --> Stores parent vertices in DFS tree
	void Graph::bridgeUtil(int u, 
						std::vector<bool> & visited, 
						int disc[], 
						int low[], 
						std::vector<int> & parent, 
						std::vector<int> & bridges) 
	{ 
		// A static variable is used for simplicity, we can  
		// avoid use of static variable by passing a pointer. 
		static int time = 0; 
	
		// Mark the current node as visited 
		visited[u] = true; 
	
		// Initialize discovery time and low value 
		disc[u] = low[u] = ++time; 
	
		// Go through all vertices aadjacent to this 
		for (int i=0; i<adjacency_list_[u].size(); i++) { 
			
			int v = adjacency_list_[u][i];

			// If v is not visited yet, then recur for it 
			if (!visited[v]) { 
				parent[v] = u;

				bridgeUtil(v, visited, disc, low, parent, bridges); 
	
				// Check if the subtree rooted with v has a  
				// connection to one of the ancestors of u 
				low[u]  = std::min(low[u], low[v]); 
	
				if (low[v] > disc[u]) {
					bridges.push_back(adjacency_edge_list_[u][i]);
				}
					

				// If the lowest vertex reachable from subtree  
				// under v is  below u in DFS tree, then u-v  
				// is a bridge 
				if (low[v] > disc[u] && opts_.verbose) 
				std::cout << "Bridge at: " << u << " " << v << std::endl;
			} 
	
			// Update low value of u for parent function calls. 
			else if (v != parent[u]) 
				low[u]  = std::min(low[u], disc[v]); 
		}
	};

}



#endif
