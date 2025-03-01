#include "analyzer.hh"
#include <iostream>
#include <fstream>
#include <stack>

void parse_mem_stats(std::string filename, MemStats& mem_stats) {
    printf("Parsing memory stats from %s...\n", filename.c_str());
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
        assert(bb_id == bb_id_counter);

        auto it = mem_stats[bb_id].find(line_address);
        if (it != mem_stats[bb_id].end()) {
            it->second.num_local_l1_hit += msg.num_local_l1_hit();
            it->second.num_remote_l1_hit += msg.num_remote_l1_hit();
            it->second.num_l2_hit += msg.num_l2_hit();
            it->second.num_memory_access += msg.num_memory_access();
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
    delete in_stream;
    printf("Done\n");
}

void analyze_shared_access(MemStats& mem_stats) {
    assert(!mem_stats.empty());
    auto& cua_mem_stats = mem_stats[0];
    for (auto& pair : cua_mem_stats) {
        Addr line_address = pair.first;
        for (size_t core_id = 1; core_id < mem_stats.size(); core_id++) {
            if (mem_stats[core_id].find(line_address) != mem_stats[core_id].end()) {
                // if address is found in other cores' access traces, mark it as shared
                pair.second.is_shared = true;
            }
        }
    }
}

void compute_wcrt(MemStats& mem_stats, bool partition_enable) {
    printf("Computing WCRT...\n");
    // TODO: plug in the following architectural number
    int num_cores = 8;
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
    assert(!mem_stats.empty());
    auto& cua_mem_stats = mem_stats[0];
    size_t wcrt = 0;
    for (auto& pair : cua_mem_stats) {
        size_t total_access_times = pair.second.num_local_l1_hit + pair.second.num_remote_l1_hit + 
                pair.second.num_l2_hit + pair.second.num_memory_access;
        size_t l1_hit_times = pair.second.num_local_l1_hit;
        size_t llc_hit_times = pair.second.num_remote_l1_hit + pair.second.num_l2_hit;
        assert(pair.second.num_remote_l1_hit == 0);
        size_t mem_access_times = pair.second.num_memory_access;
        if (partition_enable) {
            if (pair.second.is_shared) {
                wcrt += (l1_hit_times + llc_hit_times) * wcl_llc + mem_access_times * wcl_mem;
            } else {
                wcrt += l1_hit_times * wcl_l1 + llc_hit_times * wcl_llc + mem_access_times * wcl_mem;
            }
        } else {
            wcrt += total_access_times * wcl_mem;
        }
    }
    printf("WCRT is %ld\n", wcrt);
}

void check(MemStats& iso_mem_stats, MemStats& mem_stats, bool partition_enable) {
    printf("Checking...\n");
    assert(!mem_stats.empty());
    assert(!iso_mem_stats.empty());
    auto& cua_mem_stats = mem_stats[0];
    auto& iso_cua_mem_stats = iso_mem_stats[0];
    if (cua_mem_stats.size() != iso_cua_mem_stats.size()) {
        printf("[Error]: traces are different (reason: #accessed address are different).\n");
        exit(-1);
    }
    // first do sanity check to make sure the traces are the same
    for (auto& pair : iso_cua_mem_stats) {
        Addr line_address = pair.first;
        auto it = cua_mem_stats.find(line_address);
        if (it == cua_mem_stats.end()) {
            printf("[Error]: traces are different (reason: accessed address are different).\n");
            exit(-1);
        }
        size_t access_times = it->second.num_local_l1_hit + it->second.num_remote_l1_hit + 
                it->second.num_l2_hit + it->second.num_memory_access;
        size_t iso_access_times = pair.second.num_local_l1_hit + pair.second.num_remote_l1_hit + 
                pair.second.num_l2_hit + pair.second.num_memory_access;
        if (access_times != iso_access_times) {
            printf("[Error]: traces are different (reason: accessed times for an address are different).\n");
            exit(-1);
        }
    }

    bool private_data_detected = false;
    bool private_data_check_res = true;
    bool shared_data_detected = false;
    bool shared_data_check_res = true;

    // traces are the same, now proceed to isolation property check
    for (auto& pair : iso_cua_mem_stats) {
        Addr line_address = pair.first;
        auto it = cua_mem_stats.find(line_address);
        if (it->second.is_shared) {
            shared_data_detected = true;
            // for shared data, make sure the number of memory access are equal
            size_t iso_num_memory_access = pair.second.num_memory_access;
            size_t num_memory_access = it->second.num_memory_access;

            if (num_memory_access != iso_num_memory_access) {
                shared_data_check_res = false;
            }
        } else {
            private_data_detected = true;
            // for private data, make sure the number of l1 hit and llc hit are the same
            size_t iso_l1_hit= pair.second.num_local_l1_hit;
            size_t iso_llc_hit = pair.second.num_l2_hit;
            size_t num_l1_hit = it->second.num_local_l1_hit;
            size_t num_llc_hit = it->second.num_l2_hit;
            if ((iso_l1_hit != num_l1_hit) || (iso_llc_hit != num_llc_hit)) {
                private_data_check_res = false;
            }
        }
    }
    assert(private_data_detected);
    assert(shared_data_detected);
    if (private_data_check_res && shared_data_check_res) {
        if (!partition_enable) {
            printf("Unexpected: property violation is not observed in config of shared LLC\n");
            exit(1);
        }
        printf("Result: Passed\n");
        return;
    }
    if (partition_enable) {
        printf("Unexpected: property violation is observed in config of partitioned LLC\n");
        exit(2);
    }
    printf("Result: Failed\n");
    if (!private_data_check_res) {
        printf("Reason: difference in private data hit status detected\n");
    }
    if (!shared_data_check_res) {
        printf("Reason: difference in shared data hit status detected\n");
    }
    return;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <isolation mem stats file> <mem stats file> <partition enable: 1 if partition enabled 0 otherwise>" << std::endl;
        return 1;
    }
    std::string iso_mem_stats_file = argv[1];
    std::string mem_stats_file = argv[2];
    bool partition_enable = (std::stoi(argv[3]) == 1);
    printf("partition enable %d\n", partition_enable);

    MemStats *iso_mem_stats = new MemStats();
    MemStats *mem_stats = new MemStats();
    parse_mem_stats(iso_mem_stats_file, *iso_mem_stats);
    parse_mem_stats(mem_stats_file, *mem_stats);
    analyze_shared_access(*mem_stats);
    check(*iso_mem_stats, *mem_stats, partition_enable);
    compute_wcrt(*mem_stats, partition_enable);
    delete iso_mem_stats;
    delete mem_stats;
    return 0;
}
