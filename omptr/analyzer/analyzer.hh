// @author Xinzhe

#ifndef __ANALYZER_HH__
#define __ANALYZER_HH__

#include "proto/protoio.hh"
#include "proto/custom_mem_trace.pb.h"
#include "json.hpp"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/graph_utility.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <stdexcept>
#include <chrono>
#include <thread>

using json = nlohmann::json;

typedef uint64_t Addr; 
typedef int Tid;

enum DataRegion {
    GLOBAL,
    STACK,
    HEAP
};

enum Configs {
    SHARE,
    COLOR,
    PAR,
    COLOR_PRIVATE_INST
};

#define NUM_CONFIGS 4

typedef struct AddrAccessStats {
    int bb_id;
    Tid thread_id;
    Addr address;
    Addr line_address;
    bool is_ifetch;
    uint64_t num_local_l1_hit;
    uint64_t num_remote_l1_hit;
    uint64_t num_l2_hit;
    uint64_t num_memory_access;
    DataRegion data_region;
    bool is_shared;
} AddrAccessStats;

typedef std::vector<std::map<Addr, AddrAccessStats>> MemStats;

typedef struct BasicBlock {
    int ID;
    int taskID;
    int nodeID;
    int taskCreated;
    int numTasksWaitingFor;
    std::vector<int> waitFor;

     // Overload the less-than operator
    bool operator<(const BasicBlock& other) const {
        if (taskID == other.taskID) {
            return nodeID < other.nodeID;
        } else {
            return taskID < other.taskID;
        }
    }
} BasicBlock;

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS> Graph;
typedef boost::graph_traits<Graph>::vertex_descriptor Vertex;
typedef boost::graph_traits<Graph>::edge_descriptor Edge;

typedef std::vector<std::vector<bool>> ComponentsMap;

class StopSearch : public std::exception 
{
};

// Custom visitor to record the connected components found by DFS
struct ConnectedComponentRecorder : boost::default_dfs_visitor {
    ComponentsMap& c_map;
    Vertex src;
    ConnectedComponentRecorder(ComponentsMap& c_map, Vertex src) : c_map(c_map), src(src)
    {
    }

    void discover_vertex(Vertex u, const Graph& g) {
        if (u != src) {
            c_map[src][u] = true;
        }
    }

    void back_edge(Edge e, const Graph& g) {
        assert(false);
    }

    void finish_vertex(Vertex u, const Graph& g) {
        if (u == src) {
            throw StopSearch();
        }
    }
};

class ProgressBar {
public:
    ProgressBar() : m_total_steps(1), m_current_step(0) 
    {
    }

    void init(int totalSteps) {
        m_start_time = std::chrono::steady_clock::now();
        m_current_step = 0;
        m_total_steps = totalSteps;
        m_interval = totalSteps / 100;
    }

    void update(int step) {
        if (step != m_total_steps && step - m_current_step < m_interval) {
            return;
        }
        m_current_step = step;
        int progress = static_cast<int>((static_cast<float>(m_current_step) / m_total_steps) * 100);
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - m_start_time;
        double elapsedSeconds = elapsed.count();
        double estimatedTotalSeconds = elapsedSeconds / (static_cast<double>(m_current_step) / m_total_steps);
        double estimatedRemainingSeconds = estimatedTotalSeconds - elapsedSeconds;

        std::cout << "\r" << progress << "%"
                  << " | ETA: " << formatTime(estimatedRemainingSeconds) << " remaining   " << std::flush;
    }

    void done() {
        std::cout << std::endl;
    }

private:
    int m_total_steps;
    int m_current_step;
    int m_interval;
    std::chrono::steady_clock::time_point m_start_time;

    std::string formatTime(double seconds) {
        int h = static_cast<int>(seconds / 3600);
        int m = static_cast<int>((seconds - h * 3600) / 60);
        int s = static_cast<int>(seconds - h * 3600 - m * 60);
        return std::to_string(h) + "h " + std::to_string(m) + "m " + std::to_string(s) + "s";
    }
};

#endif //__ANALYZER_HH
