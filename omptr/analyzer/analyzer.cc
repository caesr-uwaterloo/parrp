#include "analyzer.hh"
#include <iostream>
#include <fstream>
#include <stack>

void from_json(const json& j, BasicBlock& b) {
    j.at("ID").get_to(b.ID);
    j.at("taskID").get_to(b.taskID);
    j.at("nodeID").get_to(b.nodeID);
    j.at("taskCreated").get_to(b.taskCreated);
    j.at("numTasksWaitingFor").get_to(b.numTasksWaitingFor);
    j.at("waitFor").get_to(b.waitFor);
}

void connected_components(const Graph& g, Vertex root, ComponentsMap& c_map) {
    /**
     * Compute the connected components of every vertex in a directed acyclic graph g
     * A different vertex v is a connected vertex of u if there is a path from u to v in g
     * If v is a connected vertex of u, c_map[u][v] is set to true otherwise false
     * 
     * The algorithm uses a DFS traversal on the root vertex to compute the connected components on a bottom-up sequence.
     * Use C(u) to denote the set of connected vertex of u
     * Then, C(u) = Union(C(v) for each target vertex v in out edges of u, set of target vertex v of u) 
     */

    std::stack<Vertex> dfs_stack;

    // Vectors to keep track of DFS progress
    std::vector<bool> discovered(boost::num_vertices(g), false);
    std::vector<bool> done(boost::num_vertices(g), false);

    // Push the starting vertex onto the stack and mark it as visited
    dfs_stack.push(root);
    discovered[root] = true;

    // Perform DFS traversal
    while (!dfs_stack.empty()) {
        // Get the top vertex from the stack
        Vertex current_vertex = dfs_stack.top();

        // Get the out-edges of the current vertex
        auto out_edges = boost::out_edges(current_vertex, g);

        // Iterate over the out-edges to add undiscovered vertices to search stack
        for (auto it = out_edges.first; it != out_edges.second; ++it) {
            // Get the target vertex of the out-edge
            Vertex target_vertex = boost::target(*it, g);

            // If the target vertex hasn't been visited yet, push it onto the stack and mark it as visited
            if (!discovered[target_vertex]) {
                dfs_stack.push(target_vertex);
                discovered[target_vertex] = true;
            }
        }

        // Iterate over the out-edges again to determine if all children vertices are done
        bool is_done = true;
        for (auto it = out_edges.first; it != out_edges.second; ++it) {
            // Get the target vertex of the out-edge
            Vertex target_vertex = boost::target(*it, g);

            // If the target vertex is not done, current vertex is not done;
            if (!done[target_vertex]) {
                is_done = false;
            }
        }

        // If all children vertices are done, iterate over the out-edges again to process the current vertex
        if (is_done) {
            for (auto it = out_edges.first; it != out_edges.second; ++it) {
                // Get the target vertex of the out-edge
                int target_vertex = boost::target(*it, g);

                c_map[current_vertex][target_vertex] = true;
                for (Vertex i = 0; i < boost::num_vertices(g); ++i) {
                    if (c_map[target_vertex][i] == true) {
                        c_map[current_vertex][i] = true;
                    }
                }
            }

            done[current_vertex] = true;
            dfs_stack.pop();
        }
    }
}

void connected_components_slow(const Graph& g, ComponentsMap& c_map) {
    /**
     * A slow version of connected components
     * Simply perform DFS on each vertex to discover the connected vertices.
     */

    for (Vertex u = 0; u < boost::num_vertices(g); ++u) {
        try {
            boost::depth_first_search(g, 
                boost::visitor(ConnectedComponentRecorder(c_map,u)).root_vertex(u));
        } catch (const StopSearch&) {
            continue;
        }
    }
}

void parse_dag(std::string filename, Graph& g, Vertex& r, Vertex& e, int& num_tasks) {
    printf("Parsing DAG from %s...\n", filename.c_str());
    std::ifstream file(filename);
    if (!file.is_open()) {
        printf("[Error] Unable to open file.\n");
        exit(1);
    }

    json j;
    try {
        file >> j;
    } catch (const std::exception& e) {
        printf("[Error] exception happens when parsing JSON: %s\n", e.what());
    }
    file.close();

    std::vector<BasicBlock> basic_blocks_1d;
    j.get_to(basic_blocks_1d);
    std::sort(basic_blocks_1d.begin(), basic_blocks_1d.end());

    std::vector<std::vector<BasicBlock>> basic_blocks_2d;
    std::vector<BasicBlock> bbs;
    assert(!basic_blocks_1d.empty());
    int current_task_id = basic_blocks_1d[0].taskID;
    assert(current_task_id == 0);
    for (const auto& bb : basic_blocks_1d) {
        if (bb.taskID != current_task_id) {
            basic_blocks_2d.push_back(bbs);
            bbs.clear();
            assert(bb.taskID == current_task_id + 1);
            current_task_id = bb.taskID;
        }
        bbs.push_back(bb);
    }
    basic_blocks_2d.push_back(bbs);

    for (const auto& bbs : basic_blocks_2d) {
        for (const auto& bb : bbs) {
            // Add control flow edge
            if (bb.nodeID >= 1) {
                BasicBlock last_bb = basic_blocks_2d[bb.taskID][bb.nodeID - 1];
                boost::add_edge(last_bb.ID, bb.ID, g);
            }
            // Add task create edge
            if (bb.taskCreated != -1) {
                BasicBlock created_bb = basic_blocks_2d[bb.taskCreated][0];
                boost::add_edge(bb.ID, created_bb.ID, g);
            }
            // Add synchronization edge
            for (const auto& task_id : bb.waitFor) {
                BasicBlock last_bb = basic_blocks_2d[task_id].back();
                boost::add_edge(last_bb.ID, bb.ID, g);
            }
        }
    }

    assert(!basic_blocks_2d.empty());
    assert(!basic_blocks_2d[0].empty());
    r = basic_blocks_2d[0][0].ID; 
    e = basic_blocks_2d[0].back().ID;
    num_tasks = basic_blocks_2d.size();

    try {
        std::vector<Vertex> container;
        boost::topological_sort(g, std::back_inserter(container));
        assert(container.back() == r);
        assert(container.front() == e);
    } catch (boost::not_a_dag&) {
        printf("[Error] graph is not a DAG.\n");
        exit(1);
    }

    printf("Done\n");
}

void parse_mem_stats(std::string filename, MemStats& mem_stats, std::vector<size_t>& exec_cycles_map) {
    printf("Parsing memory stats from %s...\n", filename.c_str());
    exec_cycles_map.clear();
    ProtoInputStream *in_stream = new ProtoInputStream(filename);
    ProtoMessage::AddrAccessStats msg;
    int bb_id_counter = -1;
    while (in_stream->read(msg)) {
        int bb_id = msg.bb_id();
        Addr line_address = msg.line_address();

        if (bb_id == bb_id_counter + 1) {
            bb_id_counter++;
            mem_stats.push_back(std::map<Addr, AddrAccessStats>{});
        }
        assert(bb_id == bb_id_counter);  // sanity check to ensure bb_id is in increasing order

        if (msg.is_metadata()) {
            exec_cycles_map.push_back(msg.exec_cycles());
            continue;
        }

        auto it = mem_stats[bb_id].find(line_address);
        if (it != mem_stats[bb_id].end()) {
            assert(false);
        } else {
            AddrAccessStats entry;
            entry.bb_id = msg.bb_id();
            entry.thread_id = msg.thread_id();
            entry.address = msg.line_address();
            entry.line_address = msg.line_address();
            entry.data_region = static_cast<DataRegion>(msg.data_region());
            entry.is_ifetch = msg.is_ifetch();
            entry.num_local_l1_hit = msg.num_local_l1_hit();
            entry.num_remote_l1_hit = msg.num_remote_l1_hit();
            entry.num_l2_hit = msg.num_l2_hit();
            entry.num_memory_access = msg.num_memory_access();
            entry.is_shared = false;  // to be populated later based on the DAG structure
            mem_stats[bb_id].emplace(line_address, entry);
        }
    }
    assert(mem_stats.size() == exec_cycles_map.size());
    delete in_stream;
    printf("Done\n");
}

void analyze_shared_access(MemStats& mem_stats, Graph& g, const Vertex r, const Vertex e) {
    //printf("Analyzing shared access...\n");
    size_t num_vertices = boost::num_vertices(g);
    size_t num_bbs = mem_stats.size();
    assert(num_vertices == num_bbs);

    printf("Computing connection map...\n");
    ComponentsMap* c_map = new ComponentsMap(num_vertices, std::vector<bool>(num_vertices, false));
    connected_components(g, r, *c_map);
    for (Vertex u = 0; u < boost::num_vertices(g); ++u) {
        // check every vertex is connected to the root
        if (u != r) {
            assert((*c_map)[r][u] == true);
        }
        // check the exit vertex is connected to every vertex
        if (u != e) {
            assert((*c_map)[u][e] == true);
        }
    }
    printf("Done\n");
    printf("Computing shared status...\n");
    ProgressBar progressBar;
    progressBar.init(mem_stats.size());
    Vertex u = 0;
    Tid tid = 0;
    for (auto& addr_stats : mem_stats) {
        // sanity check
        if (addr_stats.size() != 0) {
            auto it = addr_stats.begin();
            tid = (it->second).thread_id;
        }

        std::vector<Vertex> parallel_vertices;
        for (Vertex v = 0; v < num_vertices; ++v) {
            if ((u != v) && ((*c_map)[u][v] == false) && ((*c_map)[v][u] == false)) {
                parallel_vertices.push_back(v);
            }
        }
        
        if (!parallel_vertices.empty()) {
            // Small optimization:
            // Vertex g is a vertex that is likely to have many parallel accesses
            // with the current vertex under analysis.
            // Try this vertex first to avoid exhaustly searching all parallel vertices.
            // Use the first parallel vertex as the initial guess for g.
            Vertex g = parallel_vertices[0];

            for (auto& pair : addr_stats) {
                assert((Vertex)pair.second.bb_id == u);
                assert(pair.second.thread_id == tid);
                
                // if the access is shared, no need to process it again
                if (pair.second.is_shared) {
                    continue;
                }

                Addr line_address = pair.first;
                bool is_ifetch = pair.second.is_ifetch;
                bool is_shared = false;

                if (!mem_stats[g].empty()) {
                    auto it = mem_stats[g].begin();
                    Tid g_tid = (it->second).thread_id;

                    if (!(!is_ifetch && tid == g_tid)) {
                        if (mem_stats[g].find(line_address) != mem_stats[g].end()) {
                            // if access found in parallel node, the access is shared
                            is_shared = true;
                        }
                    }
                }
                
                if (!is_shared) {
                    for (const Vertex& p : parallel_vertices) {
                        if (mem_stats[p].size() == 0) {
                            continue;
                        }
                        auto it = mem_stats[p].begin();
                        Tid p_tid = (it->second).thread_id;

                        // for instruction, check if it is shared analytically even if the node is executed by the same thread
                        // for data, skip the check if the node is executed by the same thread
                        if (!is_ifetch && tid == p_tid) {
                            continue;
                        }

                        // if (tid == p_tid) {
                        //     continue;
                        // }

                        it = mem_stats[p].find(line_address);
                        if (it != mem_stats[p].end()) {
                            // if access found in parallel node, the access is shared
                            is_shared = true;
                            // update the parallel access to be shared as well
                            it->second.is_shared = true;
                            g = p;  // Use p as a new guess for g.
                            break;
                        }
                    }
                }
                pair.second.is_shared = is_shared;
            }
        }
        ++u;
        progressBar.update(u);
    }
    progressBar.done();
    delete c_map;
    printf("Done\n");
}

void populate_vertex_weight(const MemStats& mem_stats, const std::vector<size_t>& exec_cycles_map, const int num_cores, std::vector<std::vector<size_t>>& weight_map) {
    printf("Populate vertex weight...\n");
    weight_map.clear();
    // size_t total_exec_cycles = 0;
    // for (size_t i = 0; i < exec_cycles_map.size(); ++i) {
    //     total_exec_cycles += exec_cycles_map[i];
    // }
    // printf("total exection cycles: %ld\n", total_exec_cycles);

    size_t wcl_mem = 1;
    size_t wcl_l1 = 1;
    size_t wcl_llc = 1;
    if (num_cores == 2) {
        wcl_l1 = 1;
        wcl_llc = 87;
        wcl_mem = 568;
    } else if (num_cores == 4) {
        wcl_l1 = 1;
        wcl_llc = 175;
        wcl_mem = 1063;
    } else {
        wcl_l1 = 1;
        wcl_llc = 431;
        wcl_mem = 2065;
    }
    for (size_t bb_id = 0; bb_id < mem_stats.size(); ++bb_id) {
        size_t exec_cycles = exec_cycles_map[bb_id];
        size_t bb_wcl_share = 0;  // WCL if simply sharing the LLC (no guarantee at all)
        size_t bb_wcl_color = 0;  // WCL if using perfect set coloring (preserve guarantee for private data but not for shared data)
        size_t bb_wcl_par = 0;  // WCL if using our proposed llc partitioning technique (preserve guarantee for both private data and shared data)
        size_t bb_wcl_color_pri_inst = 0;  // WCL if using perfect set coloring + distinct address assignment for instruction (set coloring + treat all instrcutions as private)
        
        for (const auto& pair : mem_stats[bb_id]) {
            size_t total_access_times = pair.second.num_local_l1_hit + pair.second.num_remote_l1_hit + 
                pair.second.num_l2_hit + pair.second.num_memory_access;
            size_t l1_hit_times = pair.second.num_local_l1_hit;
            size_t llc_hit_times = pair.second.num_remote_l1_hit + pair.second.num_l2_hit;
            size_t mem_access_times = pair.second.num_memory_access;
            if (pair.second.is_ifetch) {
                if (pair.second.is_shared) {
                    // shared instruction
                    bb_wcl_share += total_access_times * wcl_mem;
                    bb_wcl_color += total_access_times * wcl_mem;
                    bb_wcl_par += l1_hit_times * wcl_l1 + llc_hit_times * wcl_llc + mem_access_times * wcl_mem;
                    bb_wcl_color_pri_inst += l1_hit_times * wcl_l1 + llc_hit_times * wcl_llc + mem_access_times * wcl_mem;
                } else {
                    // private instruction
                    bb_wcl_share += total_access_times * wcl_mem;
                    bb_wcl_color += l1_hit_times * wcl_l1 + llc_hit_times * wcl_llc + mem_access_times * wcl_mem;
                    bb_wcl_par += l1_hit_times * wcl_l1 + llc_hit_times * wcl_llc + mem_access_times * wcl_mem;
                    bb_wcl_color_pri_inst += l1_hit_times * wcl_l1 + llc_hit_times * wcl_llc + mem_access_times * wcl_mem;
                }
            } else {
                if (pair.second.is_shared) {
                    // shared data
                    bb_wcl_share += total_access_times * wcl_mem;
                    bb_wcl_color += total_access_times * wcl_mem;
                    bb_wcl_par += (l1_hit_times + llc_hit_times) * wcl_llc + mem_access_times * wcl_mem;
                    bb_wcl_color_pri_inst += total_access_times * wcl_mem;
                } else {
                    // private data
                    bb_wcl_share += total_access_times * wcl_mem;
                    bb_wcl_color += l1_hit_times * wcl_l1 + llc_hit_times * wcl_llc + mem_access_times * wcl_mem;
                    bb_wcl_par += l1_hit_times * wcl_l1 + llc_hit_times * wcl_llc + mem_access_times * wcl_mem;
                    bb_wcl_color_pri_inst += l1_hit_times * wcl_l1 + llc_hit_times * wcl_llc + mem_access_times * wcl_mem;
                }
            }
        }
        std::vector<size_t> weights(NUM_CONFIGS, 0);
        weights[Configs::SHARE] = bb_wcl_share + exec_cycles;
        weights[Configs::COLOR] = bb_wcl_color + exec_cycles;
        weights[Configs::PAR] = bb_wcl_par + exec_cycles;
        weights[Configs::COLOR_PRIVATE_INST] = bb_wcl_color_pri_inst + exec_cycles;
        weight_map.push_back(weights);
    }
    printf("Done\n");
}

void compute_WCRTs(const Graph& g, const std::vector<std::vector<size_t>>& weight_map, const int num_cores, std::vector<size_t>& wcrts, std::vector<size_t>& critical_paths, std::vector<size_t>& volumes) {
    printf("Computing WCRTs (Graham's bound)...\n");
    wcrts.clear();
    critical_paths.clear();
    volumes.clear();
    std::vector<Vertex> sorted_vertices;
    boost::topological_sort(g, std::back_inserter(sorted_vertices));
    // size_t critical_paths[NUM_CONFIGS];
    for (int i = 0; i < NUM_CONFIGS; ++i) {
        std::vector<size_t> longest_path(boost::num_vertices(g), 0);
        for (auto vi = sorted_vertices.begin(); vi != sorted_vertices.end(); ++vi) {
            for (auto ei = boost::out_edges(*vi, g).first; ei != boost::out_edges(*vi, g).second; ++ei) {
                Vertex target_vertex = boost::target(*ei, g);
                longest_path[*vi] = std::max(longest_path[*vi], weight_map[*vi][i] + longest_path[target_vertex]);
            }
        }
        assert(longest_path.front() != 0);
        critical_paths.push_back(longest_path.front());
    }

    // size_t volumes[NUM_CONFIGS];
    for (int i = 0; i < NUM_CONFIGS; ++i) {
        volumes.push_back(0);
        for (size_t j = 0; j < weight_map.size(); ++j) {
            volumes[i] += weight_map[j][i];
        }
    }

    for (int i = 0; i < NUM_CONFIGS; ++i) {
        double wcrt = critical_paths[i] + (double)(volumes[i] - critical_paths[i]) / num_cores;
        wcrts.push_back((size_t) wcrt);
    }
    printf("Done\n");
}

void collect_statistics(MemStats& mem_stats, Graph& g, std::string filename, const int num_tasks, const std::vector<size_t> wcrts, const std::vector<size_t> critical_paths, const std::vector<size_t>& volumes) {
    printf("Outputing statistics...\n");
    size_t num_private_inst = 0;
    size_t num_private_stack = 0;
    size_t num_private_heap = 0;
    size_t num_shared_inst = 0;
    size_t num_shared_stack = 0;
    size_t num_shared_heap = 0;

    for (const auto& addr_stats : mem_stats) {
        for (const auto& pair : addr_stats) {
            size_t access_times = pair.second.num_local_l1_hit + pair.second.num_remote_l1_hit + 
                pair.second.num_l2_hit + pair.second.num_memory_access;
            if (pair.second.is_ifetch) {
                if (pair.second.is_shared) {
                    num_shared_inst += access_times;
                } else {
                    num_private_inst += access_times;
                }
            } else {
                switch (pair.second.data_region) {
                    case DataRegion::STACK:
                        if (pair.second.is_shared) {
                            num_shared_stack += access_times;
                        } else {
                            num_private_stack += access_times;
                        }
                        break;
                    default:
                        if (pair.second.is_shared) {
                            num_shared_heap += access_times;
                        } else {
                            num_private_heap += access_times;
                        }
                        break;
                }
            }
        }
    }
    size_t num_shared_data = num_shared_stack + num_shared_heap;
    size_t num_private_data = num_private_stack + num_private_heap;
    size_t num_total_access = num_shared_data + num_private_data + num_shared_inst + num_private_inst;
    printf("\t#basic blocks: %ld\n", boost::num_vertices(g));
    printf("\t#tasks: %d\n", num_tasks);
    printf("\t#shared data: %ld (%.2f%%)\n", num_shared_data, (float)num_shared_data / num_total_access * 100);
    printf("\t#shared inst: %ld (%.2f%%)\n", num_shared_inst, (float)num_shared_inst / num_total_access * 100);
    printf("\t#private data: %ld (%.2f%%)\n", num_private_data, (float)num_private_data / num_total_access * 100);
    printf("\t#private inst: %ld (%.2f%%)\n", num_private_inst, (float)num_private_inst / num_total_access * 100);
    printf("\tWCRT(SHARE): %ld\n", wcrts[Configs::SHARE]);
    printf("\tWCRT(COLOR): %ld\n", wcrts[Configs::COLOR]);
    printf("\tWCRT(PAR): %ld\n", wcrts[Configs::PAR]);
    printf("\tWCRT(COLOR + PRIVATE INST): %ld\n", wcrts[Configs::COLOR_PRIVATE_INST]);
    printf("\tcritical path(SHARE): %ld\n", critical_paths[Configs::SHARE]);
    printf("\tcritical path(COLOR): %ld\n", critical_paths[Configs::COLOR]);
    printf("\tcritical path(PAR): %ld\n", critical_paths[Configs::PAR]);
    printf("\tcritical path(COLOR + PRIVATE INST): %ld\n", critical_paths[Configs::COLOR_PRIVATE_INST]);
    printf("\tvolume(SHARE): %ld\n", volumes[Configs::SHARE]);
    printf("\tvolume(COLOR): %ld\n", volumes[Configs::COLOR]);
    printf("\tvolume(PAR): %ld\n", volumes[Configs::PAR]);
    printf("\tvolume(COLOR + PRIVATE INST): %ld\n", volumes[Configs::COLOR_PRIVATE_INST]);

    std::ofstream file(filename);

    if (!file.is_open()) {
        printf("[Error] Unable to open file %s to write.\n", filename.c_str());
        exit(1);
    }

    // write CSV header
    file << "#vertices,#tasks,#private inst,#private stack,#private heap,#shared inst,#shared stack,#shared heap,WCRT(SHARE),WCRT(COLOR),WCRT(PAR),WCRT(COLOR + PRIVATE INST)" << std::endl;
    file << boost::num_vertices(g) << ","
         << num_tasks << ","    
         << num_private_inst << "," 
         << num_private_stack << "," 
         << num_private_heap << ","
         << num_shared_inst << "," 
         << num_shared_stack << "," 
         << num_shared_heap << ","
         << wcrts[Configs::SHARE] << ","
         << wcrts[Configs::COLOR] << ","
         << wcrts[Configs::PAR] << ","
         << wcrts[Configs::COLOR_PRIVATE_INST] << std::endl;
    file.close();
    printf("Done\n");
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <mem stats file> <dag structure json> <num cores> <output csv>" << std::endl;
        return 1;
    }

    std::string mem_stats_file = argv[1];
    std::string dag_structure_json = argv[2];
    int num_cores = std::stoi(argv[3]);
    std::string output_csv = argv[4];

    MemStats *mem_stats = new MemStats();
    Graph *g = new Graph();
    std::vector<size_t> exec_cycles_map;
    parse_mem_stats(mem_stats_file, *mem_stats, exec_cycles_map);
    Vertex r;
    Vertex e;
    int num_tasks;
    parse_dag(dag_structure_json, *g, r, e, num_tasks);
    analyze_shared_access(*mem_stats, *g, r, e);
    std::vector<std::vector<size_t>> weight_map;
    populate_vertex_weight(*mem_stats, exec_cycles_map, num_cores, weight_map);
    std::vector<size_t> wcrts;
    std::vector<size_t> critical_paths;
    std::vector<size_t> volumes;
    compute_WCRTs(*g, weight_map, num_cores, wcrts, critical_paths, volumes);
    collect_statistics(*mem_stats, *g, output_csv, num_tasks, wcrts, critical_paths, volumes);
    delete mem_stats;
    delete g;
    return 0;
}
