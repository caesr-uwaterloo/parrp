/**
 * @author Xinzhe Wang
 */

#ifndef __MEM_CACHE_REPLACEMENT_POLICIES_PAR_RP_HH__
#define __MEM_CACHE_REPLACEMENT_POLICIES_PAR_RP_HH__

#include "mem/cache/replacement_policies/base.hh"
#include <set>

namespace gem5
{

struct ParRPParams;

GEM5_DEPRECATED_NAMESPACE(ReplacementPolicy, replacement_policy);
namespace replacement_policy
{

/**
 * This replacement policy is a partitioned implementation of a specific replacement policy.
 * For example, if the implemented policy is LRU, and there are two cores sharing a cache set,
 * each core maintains its own LRU state over its assigned partition.
 */
class Par : public Base
{ 
    protected:

        struct ParEntry {
            int way_index;  // cache way number that this replacement data entry points to
            std::shared_ptr<ReplacementData> replData; // replacement data of implemented replacement policy
        };

        /**
         * Partition Table:
         * The primary index is the partition id (i.e. owner id).
         * The secondary index is the entry index within a partition.
         * Each entry is of type ParEntry.
         * A ParEntry holds the replacement data of the implemented replacement policy for its partition.
         * Each ParEntry is associated with a cache way.
         * Multiple ParEntry from different partitions can share (i.e. own) a cache way.
         * Basically, these ParEntry share the same data stored in the cache way 
         * but keep disctinct copy of replacement data for its partition.
         */
        typedef std::vector<std::vector<ParEntry>> ParTable;

        /**
         * Owner Table:
         * The primary index is the parition id (i.e. owner id).
         * The secondary index is the cache way number in a cache set.
         * The value is a boolean identifying if this cache way is owned by a partition.
         * Dimension: #paritions x #ways
         */
        typedef std::vector<std::vector<bool>> OwnerTable;

        /** Par-specific implementation of ReplacementData required in the base class prototype **/
        struct ParReplData : ReplacementData
        {
            std::shared_ptr<ParTable> par_table;  // pointer to the partition table shared in a cache set
            std::shared_ptr<OwnerTable> owner_table;  // pointer to the owner table shared in a cache set
            int way_index;  // the way number of the cache entry associated with this replacement data
            
            /**
             * Default constructor.
             */
            ParReplData(const std::shared_ptr<ParTable>& par_table, 
            const std::shared_ptr<OwnerTable>& owner_table, const int way_index) 
            : ReplacementData(), par_table(par_table), owner_table(owner_table), way_index(way_index)
            {
            }
        };

        Base* const replPolicy;  // implemented replacement policy
        std::vector<int> par_config;
        int num_way;
        
    private:
        /**
         * Instance counter of replacement data.
         * It is used to setup the entry vector shared by all cache entries in a cache set.
         */
        uint64_t m_count;

        /**
         * Holds the latest temporary ParTable instance created by instantiateEntry().
         */
        ParTable* parTableInstance;

        /**
         * Holds the latest temporary OwnerTable instance created by instantiateEntry().
         */
        OwnerTable* ownerTableInstance;

        static std::shared_ptr<ReplacementData> get_replacement_data(
            std::shared_ptr<ParTable> par_table, int way_index, int par_id) 
        {
            for (ParEntry& par_entry : par_table->at(par_id)) {
                if (par_entry.way_index == way_index) {
                    return par_entry.replData;
                }
            }
            return nullptr;
        }

        static std::vector<int> get_owners(std::shared_ptr<OwnerTable> owner_table, int way_index) {
            std::vector<int> owners;
            for (int i = 0; i < owner_table->size(); i++) {
                if ((*owner_table)[i][way_index] == true) {
                    owners.push_back(i);
                }
            }
            return owners;
        }

    public:
        typedef ParRPParams Params;
        Par(const Params &p);
        ~Par() = default;

        /**
         * Invalidate replacement data to set it as the next probable victim.
         *
         * @Param replacement_data Replacement data to be invalidated.
         */
        void invalidate(const std::shared_ptr<ReplacementData>& replacement_data)
                                                                        override;
        
        void invalidate(const std::shared_ptr<ReplacementData>& replacement_data, int par_id)
                                                                        override;

        /**
         * Touch an entry to update its replacement data.
         *
         * @Param replacement_data Replacement data to be touched.
         */
        void touch(const std::shared_ptr<ReplacementData>& replacement_data) const
                                                                        override;
        
        void touch(const std::shared_ptr<ReplacementData>& replacement_data, int par_id)
                                                                        override;

        /**
         * Reset replacement data. Used when an entry is inserted.
         *
         * @Param replacement_data Replacement data to be reset.
         */
        void reset(const std::shared_ptr<ReplacementData>& replacement_data) const
                                                                        override;
        
        void reset(const std::shared_ptr<ReplacementData>& replacement_data, int par_id)
                                                                        override;

        /**
         * Find replacement victim.
         *
         * @Param candidates Replacement candidates, selected by indexing policy.
         * @return Replacement entry to be replaced.
         */
        ReplaceableEntry* getVictim(const ReplacementCandidates& candidates) const
                                                                        override;

        ReplaceableEntry* getVictim(const ReplacementCandidates& candidates, int par_id)
                                                                        override;
        
        /**
         *  Return true if the partition has an free entry (not full)
         */
        bool parAvail(const std::shared_ptr<ReplacementData>& replacement_data, int par_id);

        /**
         *  Return true if the entry is in a partition
         */
        bool parHit(const std::shared_ptr<ReplacementData>& replacement_data, int par_id);

        /**
         * Instantiate a replacement data entry.
         *
         * @return A shared pointer to the new replacement data.
         */
        std::shared_ptr<ReplacementData> instantiateEntry() override;
};

} // namespace replacement_policy
} // namespace gem5

#endif // __MEM_CACHE_REPLACEMENT_POLICIES_PAR_RP_HH__
