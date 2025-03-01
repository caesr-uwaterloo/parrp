#ifndef __MEM_RUBY_STRUCTURES_SETSEQUENCER_HH__
#define __MEM_RUBY_STRUCTURES_SETSEQUENCER_HH__

#include "base/types.hh"
#include <map>
#include <set>


namespace gem5
{

namespace ruby
{

class SetSequencer
{
  private:
    std::map<Addr, std::set<Cycles>> sequencer;

  public:
    SetSequencer() {
        sequencer.clear();
    }

    void add_pending_request(Addr addr, Cycles reqID) {
        auto it = sequencer.find(addr);
        if (it != sequencer.end()) {
            it->second.insert(reqID);
        } else {
            std::set<Cycles> pending_requests;
            pending_requests.insert(reqID);
            sequencer[addr] = pending_requests;
        }
    }

    Cycles get_oldest_req(Addr addr) {
        auto it = sequencer.find(addr);
        if (it == sequencer.end()) {
            return Cycles(0);
        } else {
            Cycles oldest_req = *(it->second.begin());
            return oldest_req;
        }
    } 

    bool can_retire(Addr addr, Cycles reqID) {
        auto it = sequencer.find(addr);
        if (it == sequencer.end()) {
            return true;
        } else {
            Cycles oldest_req = *(it->second.begin());
            return oldest_req == reqID;
        }
    }

    void finish_request(Addr addr, Cycles reqID) {
        assert(can_retire(addr, reqID));
        auto it = sequencer.find(addr);
        assert(it != sequencer.end());
        it->second.erase(reqID);
        if (it->second.empty()) {
            sequencer.erase(addr);
        }
    }
};

} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_STRUCTURES_SETSEQUENCER_HH__