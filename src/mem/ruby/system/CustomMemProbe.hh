// @author Xinzhe

#ifndef __CUSTOM_MEM_PROBE_HH__
#define __CUSTOM_MEM_PROBE_HH__

#include "params/CustomMemProbe.hh"
#include "sim/sim_object.hh"
#include "base/types.hh"
#include "proto/protoio.hh"
#include "sim/probe/probe.hh"
#include "sim/process.hh"
#include "cpu/simple/base.hh"

namespace gem5 
{

class Process;

namespace ruby
{

enum CustomMemTrace_AccessType {
    IFETCH,
    READ,
    WRITE
};

enum CustomMemTrace_DataRegion {
    GLOBAL,
    STACK,
    HEAP
};

enum CustomMemTrace_HitStatus {
    Local_L1Cache,
    Remote_L1Cache,
    L2Cache,
    Memory
};

typedef struct CustomMemTrace {
    int bb_id;
    Addr address;
    Addr line_address;
    CustomMemTrace_AccessType access_type;
    CustomMemTrace_HitStatus hit_status;
    CustomMemTrace_DataRegion data_region;
    int thread_id;
} CustomMemTrace;

typedef struct AddrAccessStats {
    int bb_id;
    int thread_id;
    Addr address;
    Addr line_address;
    bool is_ifetch;
    uint64_t num_local_l1_hit;
    uint64_t num_remote_l1_hit;
    uint64_t num_l2_hit;
    uint64_t num_memory_access;
    CustomMemTrace_DataRegion data_region;
    bool is_metadata;
    uint64_t exec_cycles;

    void record(CustomMemTrace_HitStatus hit_status) {
        switch (hit_status) {
            case Local_L1Cache:
                num_local_l1_hit++;
                break;
            case Remote_L1Cache:
                num_remote_l1_hit++;
                break;
            case L2Cache:
                num_l2_hit++;
                break;
            case Memory:
                num_memory_access++;
                break;
        }
    }
} AddrAccessStats;

typedef struct AddrAccessKey {
    int bb_id;
    Addr address;

    // Overload the less-than operator
    bool operator<(const AddrAccessKey& other) const {
        if (bb_id < other.bb_id) {
            return true;
        } else if (bb_id > other.bb_id) {
            return false;
        }
        // If bb_id is equal, compare address
        return address < other.address;
    }
} AddrAccessKey;

class CustomMemProbe : public ProbeListenerObject
{
    public:
        typedef CustomMemProbeParams Params;
        CustomMemProbe(const Params &p); 
        void regProbeListeners() override;  // Register probe listeners

        static std::map<int,int> m_bb_id_map;
        static std::set<int> m_done_bb;
        static std::map<int,uint64_t> m_cpus_simulated_cycles;
        static std::vector<BaseSimpleCPU*> m_cpus; 
        static CustomMemProbe* m_instance;
        static void start_bb_scope(int bb_id, int thread_id);
        static void end_bb_scope(int thread_id);

        void recordMemTrace(const CustomMemTrace &mem_trace);
        void recordExecCycles(int bb_id, int thread_id, uint64_t exec_cycles);

        static CustomMemTrace_DataRegion getDataRegion(Addr v_addr, Process *process);
    
    private:
        std::string m_trace_file;
        bool m_enable_raw_trace;
        bool m_use_traffic_gen;
        ProtoOutputStream *m_trace_stream;
        std::map<AddrAccessKey, AddrAccessStats> m_addr_stats;

        static void check();
        void closeStreams();
};

typedef ProbePointArg<CustomMemTrace>  CustomMemProbePoint;
typedef std::unique_ptr<CustomMemProbePoint> CustomMemProbePointUPtr;

}

}

#endif
