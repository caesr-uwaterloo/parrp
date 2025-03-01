#include "mem/ruby/system/CustomMemProbe.hh"

#include "base/output.hh"
#include "base/callback.hh"
#include "base/trace.hh"
#include "sim/core.hh"
#include "cpu/simple/exec_context.hh"
#include "debug/OMPTR.hh"
#include "proto/custom_mem_trace.pb.h"


namespace gem5
{

namespace ruby
{

// @omptr initialize basic block id to -1, indicating no basic block is associated with the current execution context
std::map<int,int> CustomMemProbe::m_bb_id_map;
std::set<int> CustomMemProbe::m_done_bb;
std::map<int,uint64_t> CustomMemProbe::m_cpus_simulated_cycles;
std::vector<BaseSimpleCPU*> CustomMemProbe::m_cpus; 
CustomMemProbe* CustomMemProbe::m_instance;

void
CustomMemProbe::check()
{
    // perform sanity check to ensure that 
    // each cpu has only one thread context and the context id is the same as cpu id
    for (int i = 0; i < m_cpus.size(); ++i) {
        int cpu_id = m_cpus[i]->cpuId();
        assert(m_cpus[i]->numContexts() == 1);
        ThreadContext* thread_context = m_cpus[i]->getContext(0);
        assert(thread_context != nullptr);
        assert(thread_context->contextId() == cpu_id);
        assert(cpu_id == i);
    }
}

void 
CustomMemProbe::start_bb_scope(int bb_id, int thread_id) { 
    check();
    auto it = m_bb_id_map.find(thread_id);
    if (it != m_bb_id_map.end()) {
        it->second = bb_id;
    } else {
        m_bb_id_map[thread_id] = bb_id;
    }

    m_cpus[thread_id]->threadInfo[0]->execContextStats.scopedNotIdleFraction.reset();
    uint64_t simulated_cycles = m_cpus[thread_id]->baseStats.numCycles.result();
    m_cpus_simulated_cycles[thread_id] = simulated_cycles;

    DPRINTFR(OMPTR, "BB %d starts on thread %d\n ", bb_id, thread_id);
}

void 
CustomMemProbe::end_bb_scope(int thread_id) { 
    check();
    auto it = m_bb_id_map.find(thread_id);
    DPRINTFR(OMPTR, "BB %d ends on thread %d\n ", it->second, thread_id);
    assert(it != m_bb_id_map.end());
    assert(it->second != -1);
    assert(m_done_bb.find(it->second) == m_done_bb.end());
    int bb_id = it->second;
    m_done_bb.insert(it->second);  
    it->second = -1;

    double scopedNotIdleFraction = m_cpus[thread_id]->threadInfo[0]->execContextStats.scopedNotIdleFraction.result();
    uint64_t simulated_cycles = m_cpus[thread_id]->baseStats.numCycles.result();
    assert(simulated_cycles >= m_cpus_simulated_cycles[thread_id]);
    uint64_t scopedSimulatedCycles = simulated_cycles - m_cpus_simulated_cycles[thread_id];
    uint64_t exec_cycles = scopedNotIdleFraction * scopedSimulatedCycles;
    m_instance->recordExecCycles(bb_id, thread_id, exec_cycles);
}

CustomMemTrace_DataRegion
CustomMemProbe::getDataRegion(Addr v_addr, Process *process) {
    Addr stack_min = process->memState->getStackMin();
    Addr stack_base = process->memState->getStackBase();
    Addr brk_point = process->memState->getBrkPoint();
    Addr start_brk_point = process->memState->getStartBrkPoint();

    // make sure the stack grows downwards
    assert(stack_min <= stack_base);
    assert(brk_point <= stack_min);
    assert(start_brk_point <= brk_point);

    if (stack_min <= v_addr && v_addr <= stack_base) {
        return CustomMemTrace_DataRegion::STACK;
    } else if ( brk_point <= v_addr && v_addr <= stack_min ) {
        return CustomMemTrace_DataRegion::HEAP;
    } else {
        return CustomMemTrace_DataRegion::GLOBAL;
    }
}

CustomMemProbe::CustomMemProbe(const Params &p)
    : ProbeListenerObject(p),
      m_enable_raw_trace(p.enable_raw_trace),
      m_use_traffic_gen(p.use_traffic_gen),
      m_trace_stream(nullptr),
      m_addr_stats()
{
    // create proto output stream to dump traces
    if (p.trace_file != "") {
        // If the trace file is not specified as an absolute path,
        // append the current simulation output directory
        m_trace_file = simout.resolve(p.trace_file);
    } else {
        // If the trace file is not set, use the current sim object name
        m_trace_file = simout.resolve(name());
    }

    m_instance = this;

    if (!m_use_traffic_gen) {
        m_cpus = p.cpus;
    }

    m_trace_file = m_trace_file + (m_enable_raw_trace ? ".trc" : ".stats");
    m_trace_file = m_trace_file + (p.trace_compress ? ".gz" : "");
    m_trace_stream = new ProtoOutputStream(m_trace_file);

    // register simulation exit callback to safely close proto output stream
    registerExitCallback([this]() { closeStreams(); });
}

void
CustomMemProbe::regProbeListeners()
{
    // register listener to listen on CustomMemPorbe probe point
    typedef ProbeListenerArg<CustomMemProbe, CustomMemTrace> CustomMemTraceListener;
    listeners.push_back(
        new CustomMemTraceListener(
            this, "CustomMemProbe", &CustomMemProbe::recordMemTrace
        )
    );
}

void
CustomMemProbe::recordMemTrace(const CustomMemTrace &mem_trace)
{
    int thread_id = mem_trace.thread_id;
    int bb_id = -1;
    if (m_use_traffic_gen) {
        bb_id = thread_id;
        assert(bb_id != -1);
    } else {
        auto it = m_bb_id_map.find(thread_id);
        if (it != m_bb_id_map.end()) {
            bb_id = it->second;
        }
    }
    
    if (m_enable_raw_trace) {
        ProtoMessage::CustomMemTrace mem_trace_msg;
        mem_trace_msg.set_bb_id(bb_id);
        mem_trace_msg.set_address(mem_trace.address);
        mem_trace_msg.set_line_address(mem_trace.line_address);
        mem_trace_msg.set_access_type(static_cast<ProtoMessage::CustomMemTrace_AccessType>(mem_trace.access_type));
        mem_trace_msg.set_hit_status(static_cast<ProtoMessage::CustomMemTrace_HitStatus>(mem_trace.hit_status));
        mem_trace_msg.set_thread_id(thread_id);
        mem_trace_msg.set_data_region(static_cast<ProtoMessage::CustomMemTrace_DataRegion>(mem_trace.data_region));
        m_trace_stream->write(mem_trace_msg);
    } else if (bb_id != -1) {
        AddrAccessKey key;
        key.bb_id = bb_id;
        key.address = mem_trace.line_address;  // record statistics per line address

        auto entry = m_addr_stats.find(key);
        if (entry != m_addr_stats.end()) {
            // update access counter if entry exists
            entry->second.record(mem_trace.hit_status);
        } else {
            // create new addr stats entry
            AddrAccessStats new_entry;
            new_entry.bb_id = bb_id;
            new_entry.thread_id = thread_id;
            new_entry.address = mem_trace.line_address;  // use line address instead
            new_entry.line_address = mem_trace.line_address;
            new_entry.data_region = mem_trace.data_region;
            new_entry.is_ifetch = (mem_trace.access_type == CustomMemTrace_AccessType::IFETCH);
            new_entry.num_local_l1_hit = 0;
            new_entry.num_remote_l1_hit = 0;
            new_entry.num_l2_hit = 0;
            new_entry.num_memory_access = 0;
            new_entry.is_metadata = false;
            new_entry.exec_cycles = 0;
            // update access counter
            new_entry.record(mem_trace.hit_status);
            // insert to map
            m_addr_stats.emplace(key, new_entry);
        }
    }
}

void
CustomMemProbe::recordExecCycles(int bb_id, int thread_id, uint64_t exec_cycles) {
    AddrAccessKey key;
    key.bb_id = bb_id;
    key.address = 0;

    auto entry = m_addr_stats.find(key);
    if (entry != m_addr_stats.end()) {
        // update access counter if entry exists
        entry->second.is_metadata = true;
        entry->second.exec_cycles = exec_cycles;
    } else {
        // create new addr stats entry
        AddrAccessStats new_entry;
        new_entry.bb_id = bb_id;
        new_entry.thread_id = thread_id;
        new_entry.address = 0;
        new_entry.line_address = 0;
        new_entry.data_region = CustomMemTrace_DataRegion::GLOBAL;
        new_entry.is_ifetch = false;
        new_entry.num_local_l1_hit = 0;
        new_entry.num_remote_l1_hit = 0;
        new_entry.num_l2_hit = 0;
        new_entry.num_memory_access = 0;
        new_entry.is_metadata = true;
        new_entry.exec_cycles = exec_cycles;
        // insert to map
        m_addr_stats.emplace(key, new_entry);
    }
}

void 
CustomMemProbe::closeStreams()
{
    if (m_enable_raw_trace == false) {
        for (const auto& entry : m_addr_stats) {
            ProtoMessage::AddrAccessStats msg;
            msg.set_bb_id(entry.second.bb_id);
            msg.set_address(entry.second.address);
            msg.set_line_address(entry.second.line_address);
            msg.set_is_ifetch(entry.second.is_ifetch);
            msg.set_num_local_l1_hit(entry.second.num_local_l1_hit);
            msg.set_num_remote_l1_hit(entry.second.num_remote_l1_hit);
            msg.set_num_l2_hit(entry.second.num_l2_hit);
            msg.set_num_memory_access(entry.second.num_memory_access);
            msg.set_thread_id(entry.second.thread_id);
            msg.set_data_region(static_cast<ProtoMessage::AddrAccessStats_DataRegion>(entry.second.data_region));
            msg.set_is_metadata(entry.second.is_metadata);
            msg.set_exec_cycles(entry.second.exec_cycles);
            m_trace_stream->write(msg);
        }
    }
    if (m_trace_stream != NULL)
        delete m_trace_stream;
}

}

}