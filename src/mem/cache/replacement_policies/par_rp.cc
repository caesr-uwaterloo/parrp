/**
 * @author Xinzhe Wang
 */

#include "mem/cache/replacement_policies/par_rp.hh"

#include <cassert>
#include <memory>

#include "params/ParRP.hh"
#include "sim/cur_tick.hh"
#include <cstdlib>
#include "base/trace.hh"
#include "debug/RP.hh"

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(ReplacementPolicy, replacement_policy);
namespace replacement_policy
{

Par::Par(const Params &p)
  : Base(p), replPolicy(p.replacement_policy), 
    par_config(p.par_config),
    num_way(p.num_way),
    m_count(0),
    parTableInstance(nullptr),
    ownerTableInstance(nullptr)
{
    fatal_if(replPolicy == nullptr,
        "Replacement policy must be instantiated");

    int tot_par_size = 0;
    for (int i = 0; i < par_config.size(); ++i) {
        tot_par_size += par_config[i];
    }
    fatal_if(tot_par_size != num_way,
        "The total number of entries across all partitions must be equal to the number of ways.");
}

void
Par::invalidate(const std::shared_ptr<ReplacementData>& replacement_data)
{
    DPRINTFR(RP, "invalidate\n");
    // invalidate an unowned cache entry
    // simply perform sanity check to ensure the entry is unowned
    std::shared_ptr<ParReplData> par_repl_data = 
        std::static_pointer_cast<ParReplData>(replacement_data);
    int way_index = par_repl_data->way_index;
    std::shared_ptr<ParTable> par_table = par_repl_data->par_table;
    std::shared_ptr<OwnerTable> owner_table = par_repl_data->owner_table;
    // sanity check: entry must be unowned
    for (const auto& owner_vec : *owner_table) {
        assert(!owner_vec[way_index]);
    }
    for (const auto& par : *par_table) {
        for (const auto& par_entry : par) {
            assert(par_entry.way_index != way_index);
        }
    }
}

void
Par::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    fatal("Wrong version of touch() called in Par replacement policy");
}

void
Par::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    fatal("Wrong version of reset() called in Par replacement policy");
}

ReplaceableEntry*
Par::getVictim(const ReplacementCandidates& candidates) const
{
    DPRINTFR(RP, "getVictim\n");

    // keep a reference of the replacement data of the first candidate
    assert(candidates.size() != 0);
    assert(candidates[0] != nullptr);
    std::shared_ptr<ParReplData> par_repl_data_0 = 
        std::static_pointer_cast<ParReplData>(candidates[0]->replacementData);
    std::shared_ptr<ParTable> par_table = par_repl_data_0->par_table;
    std::shared_ptr<OwnerTable> owner_table = par_repl_data_0->owner_table;
    for (auto& candidate : candidates) {
        assert(candidate != nullptr);
        std::shared_ptr<ParReplData> par_repl_data = 
            std::static_pointer_cast<ParReplData>(candidate->replacementData);
        // sanity check: all candidates should be in the same set, 
        // thus sharing the same partition table and owner table
        assert(par_repl_data->par_table == par_table);
        assert(par_repl_data->owner_table == owner_table);
        // also ensure that way number is consistent
        assert(par_repl_data->way_index == candidate->getWay());
    }

    // Return an unowned entry to replace
    // Such entry is guaranteed to exist when this function is called
    // Select the first entry that meets the requirement
    for (auto& candidate : candidates) {
        std::shared_ptr<ParReplData> par_repl_data = 
            std::static_pointer_cast<ParReplData>(candidate->replacementData);
        int way_index = candidate->getWay();
        std::vector<int> owners = get_owners(owner_table, way_index);
        if (owners.empty()) {
            DPRINTFR(RP, "getVictim: found unowned\n");
            return candidate;
        }
    }

    assert(false);  // should never reach here
    return nullptr;
}

void
Par::invalidate(const std::shared_ptr<ReplacementData>& replacement_data, int par_id)
{
    DPRINTFR(RP, "invalidate: on par %d\n", par_id);
    // ensure valid partition id
    assert(par_id < par_config.size());

    std::shared_ptr<ParReplData> par_repl_data = 
        std::static_pointer_cast<ParReplData>(replacement_data);
    int way_index = par_repl_data->way_index;
    std::shared_ptr<ParTable> par_table = par_repl_data->par_table;
    std::shared_ptr<OwnerTable> owner_table = par_repl_data->owner_table;

    // sanity check: the cache way must be owned by the partition
    assert((*owner_table)[par_id][way_index]);
    // remove ownership: 
    // update the owner table
    (*owner_table)[par_id][way_index] = false;
    // unlink par entry by setting way index to -1
    bool found = false;
    for (ParEntry& par_entry : par_table->at(par_id)) {
        if (par_entry.way_index == way_index) {
            par_entry.way_index = -1;
            // also invalidate partition data
            replPolicy->invalidate(par_entry.replData);
            found = true;
            break;
        }
    }
    assert(found);
}

void
Par::touch(const std::shared_ptr<ReplacementData>& replacement_data, int par_id)
{
    DPRINTFR(RP, "touch: on par %d\n", par_id);
    // ensure valid partition id
    assert(par_id < par_config.size());

    std::shared_ptr<ParReplData> par_repl_data = 
        std::static_pointer_cast<ParReplData>(replacement_data);
    int way_index = par_repl_data->way_index;
    DPRINTFR(RP, "touch: way index %d\n", way_index);
    std::shared_ptr<ParTable> par_table = par_repl_data->par_table;
    std::shared_ptr<OwnerTable> owner_table = par_repl_data->owner_table;

    // if the cache way already belongs to the partition
    // simply update the underlying replacement data of the implemented replacement policy
    if ((*owner_table)[par_id][way_index] == true) {
        DPRINTFR(RP, "touch: address is already in parition\n");
        std::shared_ptr<ReplacementData> replData = get_replacement_data(par_table, way_index, par_id);
        assert(replData != nullptr);
        replPolicy->touch(replData);
        return;
    }
    
    // get the number of cache ways belonging to the partition
    int count = 0;
    for (auto& par_entry : par_table->at(par_id)) {
        if (par_entry.way_index != -1) {
            count++;
        }
    }
    assert(count < par_config[par_id]);  // there must be at least one vacant partition entry

    // bring the touched entry in the partition
    (*owner_table)[par_id][way_index] = true;
    // update par entry
    bool found = false;
    for (auto& par_entry : par_table->at(par_id)) {
        if (par_entry.way_index == -1) {
            found = true;
            par_entry.way_index = way_index;
            replPolicy->reset(par_entry.replData);
            break;
        }
    }
    assert(found); 

    // update the underlying replacement data of the implemented replacement policy
    std::shared_ptr<ReplacementData> replData = get_replacement_data(par_table, way_index, par_id);
    assert(replData != nullptr);
    replPolicy->touch(replData);
}

void
Par::reset(const std::shared_ptr<ReplacementData>& replacement_data, int par_id)
{
    DPRINTFR(RP, "reset: on par %d\n", par_id);
    // ensure valid partition id
    assert(par_id < par_config.size());

    std::shared_ptr<ParReplData> par_repl_data = 
        std::static_pointer_cast<ParReplData>(replacement_data);
    int way_index = par_repl_data->way_index;
    DPRINTFR(RP, "reset: way index %d\n", way_index);
    std::shared_ptr<ParTable> par_table = par_repl_data->par_table;
    std::shared_ptr<OwnerTable> owner_table = par_repl_data->owner_table;

    // sanity check: the cache way is unowned
    std::vector<int> owners = get_owners(owner_table, way_index);
    DPRINTFR(RP, "reset: number of owners %d\n", owners.size());
    assert(owners.size() == 0);
    for (const auto& par_entry : par_table->at(par_id)) {
        assert(par_entry.way_index != way_index);
    }

    // set to be owned
    // update owner table
    (*owner_table)[par_id][way_index] = true;
    // allocate a par entry
    bool found = false;
    for (auto& par_entry : par_table->at(par_id)) {
        if (par_entry.way_index == -1) {
            par_entry.way_index = way_index;
            replPolicy->reset(par_entry.replData);
            found = true;
            break;
        }
    }
    assert(found);
}

/**
 * Select the victim in a partition. The partition must be full.
 */
ReplaceableEntry*
Par::getVictim(const ReplacementCandidates& candidates, int par_id)
{
    DPRINTFR(RP, "getVictim: on parition %d\n", par_id);
    // ensure valid partition id
    assert(par_id < par_config.size());

    // at least one victim
    assert(candidates.size() != 0);
    
    // Select a victim among entries that are solely owned by the partition
    ReplacementCandidates par_candidates;
    std::shared_ptr<ParTable> par_table;
    std::shared_ptr<OwnerTable> owner_table;
    bool found = false;
    for (auto& candidate : candidates) {
        if (candidate == nullptr) {
            continue;
        }
        if (!found) {
            std::shared_ptr<ParReplData> par_repl_data = 
                std::static_pointer_cast<ParReplData>(candidate->replacementData);
            owner_table = par_repl_data->owner_table;
            par_table = par_repl_data->par_table;
            found = true;
        }
        int way_index = candidate->getWay();
        if ((*owner_table)[par_id][way_index]) {
            ReplaceableEntry* par_candidate = new ReplaceableEntry();
            par_candidate->replacementData = get_replacement_data(par_table, way_index, par_id);
            assert(par_candidate->replacementData != nullptr);
            par_candidate->setPosition(0, way_index);
            par_candidates.push_back(par_candidate);
        }  
    }

    // partition must be full
    DPRINTFR(RP, "getVictim: current size %d, total size %d\n", par_candidates.size(), par_config[par_id]);
    assert(par_candidates.size() == par_config[par_id]);
    // get the victim in this partition using the implemented replacement policy
    int victim_way = replPolicy->getVictim(par_candidates)->getWay();
    // freeup memory 
    for (auto& par_candidate : par_candidates) {
        delete par_candidate;
    }

    // return victim candidate
    for (auto& candidate : candidates) {
        if (candidate == nullptr) {
            continue;
        }
        if (candidate->getWay() == victim_way) {
            return candidate;
        }
    }

    // should never reach here
    assert(false);
    return nullptr;
}

bool 
Par::parAvail(const std::shared_ptr<ReplacementData>& replacement_data, int par_id) 
{
    DPRINTFR(RP, "parAvail: on parition %d\n", par_id);
    // ensure valid partition id
    assert(par_id < par_config.size());
    std::shared_ptr<ParReplData> par_repl_data = 
        std::static_pointer_cast<ParReplData>(replacement_data);
    std::shared_ptr<ParTable> par_table = par_repl_data->par_table;
    std::shared_ptr<OwnerTable> owner_table = par_repl_data->owner_table;
    
    // get the number of cache ways belonging to the partition
    int count = 0;
    for (auto& par_entry : par_table->at(par_id)) {
        if (par_entry.way_index != -1) {
            count++;
        }
    }
    DPRINTFR(RP, "parAvail: current size %d, total size %d\n", count, par_config[par_id]);

    assert(count <= par_config[par_id]);
    // return if the partition has a free entry
    return count < par_config[par_id];
}

bool 
Par::parHit(const std::shared_ptr<ReplacementData>& replacement_data, int par_id) 
{
    // ensure valid partition id
    assert(par_id < par_config.size());
    std::shared_ptr<ParReplData> par_repl_data = 
        std::static_pointer_cast<ParReplData>(replacement_data);
    int way_index = par_repl_data->way_index;
    std::shared_ptr<ParTable> par_table = par_repl_data->par_table;
    std::shared_ptr<OwnerTable> owner_table = par_repl_data->owner_table;

    // if the cache way belongs to the partition, return true
    if ((*owner_table)[par_id][way_index] == true) {
        return true;
    } else {
        return false;
    }
}

std::shared_ptr<ReplacementData>
Par::instantiateEntry()
{
    // Note: the logic expects the instantiateEntry() is called 
    // in the sequence that creates entries within a cache set first.
    // e.g. see logic in ruby CacheMemory.
    int way_index = m_count % num_way;
    // DPRINTFR(RP, "instantiateEntry: way index %d\n", way_index);
    if (way_index == 0) {
        // create new partition table
        parTableInstance = new ParTable();
        for (int par_size : par_config) {
            std::vector<ParEntry> partitions;
            for (int i = 0; i < par_size; ++i) {
                std::shared_ptr<ReplacementData> repl_data = replPolicy->instantiateEntry();
                partitions.push_back({-1, repl_data});  // use -1 way index to indicate this entry is empty
            }
            parTableInstance->push_back(partitions);
        }

        // create new owner table
        ownerTableInstance = new OwnerTable();
        int par_num = par_config.size();
        for (int i = 0; i < par_num; ++i) {
            ownerTableInstance->push_back(std::vector<bool>(num_way, false));
        }
    }
    
    m_count++;  // increment counter
    std::shared_ptr<ReplacementData> repl_data = replPolicy->instantiateEntry();
    ParReplData* par_repl_data = new ParReplData(
        std::shared_ptr<ParTable>(parTableInstance),
        std::shared_ptr<OwnerTable>(ownerTableInstance),
        way_index);
    // DPRINTFR(RP, "instantiateEntry: way index %d\n", par_repl_data->way_index);
    return std::shared_ptr<ReplacementData>(par_repl_data);
}

} // namespace replacement_policy
} // namespace gem5