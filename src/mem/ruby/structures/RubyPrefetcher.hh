/*
 * Copyright (c) 2020 Inria
 * Copyright (c) 2020 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 1999-2012 Mark D. Hill and David A. Wood
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MEM_RUBY_STRUCTURES_PREFETCHER_HH__
#define __MEM_RUBY_STRUCTURES_PREFETCHER_HH__

// Implements Power 4 like prefetching

#include <bitset>

#include "base/circular_queue.hh"
#include "base/statistics.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/slicc_interface/AbstractController.hh"
#include "mem/ruby/slicc_interface/RubyRequest.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "params/RubyPrefetcher.hh"
#include "sim/sim_object.hh"

// Berti (some may not be needed)
#include <algorithm>
#include <iostream>
#include <stdlib.h>
#include <cassert>
#include <vector>
#include <time.h>
#include <cstdio>
#include <tuple>
#include <queue>
#include <cmath>
#include <map>

namespace gem5
{

namespace ruby
{


// Hash Function
//#define HASH_FN
//# define HASH_ORIGINAL
//# define THOMAS_WANG_HASH_1
//# define THOMAS_WANG_HASH_2
//# define THOMAS_WANG_HASH_3
//# define THOMAS_WANG_HASH_4
//# define THOMAS_WANG_HASH_5
//# define THOMAS_WANG_HASH_6
//# define THOMAS_WANG_HASH_7
//# define THOMAS_WANG_NEW_HASH
//# define THOMAS_WANG_HASH_HALF_AVALANCHE
//# define THOMAS_WANG_HASH_FULL_AVALANCHE
//# define THOMAS_WANG_HASH_INT_1
//# define THOMAS_WANG_HASH_INT_2
#define ENTANGLING_HASH
//# define FOLD_HASH

#define NO_CROSS_PAGE

  /*****************************************************************************
   *                      General Structs                                      *
   *****************************************************************************/

  typedef struct Delta {
    uint64_t conf;
    int64_t  delta;
    uint8_t  rpl;
    Delta(): conf(0), delta(0), rpl(0) {};
  } delta_t;

  /*****************************************************************************
   *                      Berti structures                                     *
   *****************************************************************************/
  class LatencyTable
  {
    /* Latency table simulate the modified PQ and MSHR */
    private:
      struct latency_table {
        uint64_t addr = 0; // Addr
        uint64_t tag  = 0; // IP-Tag
        uint64_t time = 0; // Event cycle
        bool     pf   = false;   // Is the entry accessed by a demand miss
      };
      int size;

      latency_table *latencyt;
      RubyPrefetcher& rp_instance;

    public:
      LatencyTable(const int size, RubyPrefetcher& rp) : size(size),rp_instance(rp)
      {
        latencyt = new latency_table[size];
      }
      ~LatencyTable() { delete latencyt;}

      uint8_t  add(uint64_t addr, uint64_t tag, bool pf, uint64_t cycle);
      uint64_t get(uint64_t addr);
      uint64_t del(uint64_t addr);
      uint64_t get_tag(uint64_t addr);

  };

  class ShadowCache
  {
    /* Shadow cache simulate the modified L1D Cache */
    private:
      struct shadow_cache {
        uint64_t addr = 0; // Addr
        uint64_t lat  = 0;  // Latency
        bool     pf   = false;   // Is a prefetch
      }; // This struct is the vberti table

      int sets;
      int ways;
      shadow_cache **scache;
      RubyPrefetcher& rp_instance;

    public:
      ShadowCache(const int sets, const int ways, RubyPrefetcher& rp) : rp_instance(rp)
      {
        scache = new shadow_cache*[sets];
        for (int i = 0; i < sets; i++) scache[i] = new shadow_cache[ways];

        this->sets = sets;
        this->ways = ways;
      }

      ~ShadowCache()
      {
        for (int i = 0; i < sets; i++) delete scache[i];
        delete scache;
      }

      bool add(uint32_t set, uint32_t way, uint64_t addr, bool pf, uint64_t lat);
      bool get(uint64_t addr);
      void set_pf(uint64_t addr, bool pf);
      bool is_pf(uint64_t addr);
      uint64_t get_latency(uint64_t addr);
  };

  class HistoryTable
  {
    /* History Table */
    private:
      struct history_table {
        uint64_t tag  = 0; // IP Tag
        uint64_t addr = 0; // IP @ accessed
        uint64_t time = 0; // Time where the line is accessed
      }; // This struct is the history table

      uint64_t sets;
      uint64_t ways;

      history_table **historyt;
      history_table **history_pointers;
      RubyPrefetcher& rp_instance;

      uint16_t get_aux(uint32_t latency, uint64_t tag, uint64_t act_addr,
          std::vector<uint64_t>& tags, std::vector<uint64_t>& addr, uint64_t cycle);
    public:
      HistoryTable(uint64_t sets_in, uint64_t ways_in, RubyPrefetcher& rp) : rp_instance(rp)
      {
	sets = sets_in;
	ways = ways_in;
        history_pointers = new history_table*[sets];
        historyt = new history_table*[sets];

        for (int i = 0; i < sets; i++) historyt[i] = new history_table[ways];
        for (int i = 0; i < sets; i++) history_pointers[i] = historyt[i];
      }

      ~HistoryTable()
      {
        for (int i = 0; i < sets; i++) delete historyt[i];
        delete historyt;

        delete history_pointers;
      }

      int get_ways();
      void add(uint64_t tag, uint64_t addr, uint64_t cycle);
      uint16_t get(uint32_t latency, uint64_t tag, uint64_t act_addr,
          std::vector<uint64_t>& tags, std::vector<uint64_t>& addr, uint64_t cycle);
  };

  class Berti
  {
    /* Berti Table */
    private:
      struct berti {
        std::vector<delta_t> deltas; //         std::array<delta_t, BERTI_TABLE_DELTA_SIZE> deltas;
        uint64_t conf = 0;
        uint64_t total_used = 0;
	berti(size_t delta_size) : deltas(delta_size) {}
      };

      std::map<uint64_t, berti*> bertit;
      std::queue<uint64_t> bertit_queue;

      uint64_t size = 0;

      RubyPrefetcher& rp_instance;
      static uint64_t berti_r;
      static uint64_t berti_l1;
      static uint64_t berti_l2;
      static uint64_t berti_l2r;

      bool static compare_greater_delta(delta_t a, delta_t b);
      bool static compare_rpl(delta_t a, delta_t b);

      void increase_conf_tag(uint64_t tag);
      void conf_tag(uint64_t tag);
      void add(uint64_t tag, int64_t delta);


    public:
      Berti(uint64_t p_size, RubyPrefetcher& rp);
      ~Berti() {
	for (auto& pair : bertit) {
	  delete pair.second;
	}
      }
      void find_and_update(uint64_t latency, uint64_t tag, uint64_t cycle,
			   uint64_t line_addr, HistoryTable *historyt);
      uint8_t get(uint64_t tag, std::vector<delta_t> &res);
      uint64_t ip_hash(uint64_t ip);
  };

class RubyPrefetcher : public SimObject
{
    public:
        typedef RubyPrefetcherParams Params;
        RubyPrefetcher(const Params &p);
        ~RubyPrefetcher() = default;

        /**
         * Implement the prefetch hit(miss) callback interface.
         * These functions are called by the cache when it hits(misses)
         * on a line with the line's prefetch bit set. If this address
         * hits in m_array we will continue prefetching the stream.
         */
         void observePfHit(Addr address, const RubyRequestType& type, Addr pc);
         void observePfMiss(Addr address, const RubyRequestType& type, Addr pc);

        /**
         * Observe a memory miss from the cache.
         *
         * @param address   The physical address that missed out of the cache.
         */
        void observeMiss(Addr address, const RubyRequestType& type, Addr pc);
        void observeHit(Addr address, const RubyRequestType& type, Addr pc);
        /**
         * Print out some statistics
         */
        void print(std::ostream& out) const;
        void setController(AbstractController *_ctrl)
        { m_controller = _ctrl; }

        void insertReplacement(Addr evicted_addr) {
	  assert(last_replaced_addr == 0);
	  last_replaced_addr = makeLineAddress(evicted_addr);
	}
        Addr getLastReplacedAddr() { return last_replaced_addr; }

        void prefetcher_cache_operate(Addr addr, Addr ip, bool cache_hit,
				      bool useful_prefetch, const RubyRequestType& type);
        void prefetcher_cache_fill(Addr addr, bool prefetch, uint32_t set, uint32_t way);

        uint64_t getBertiTableSize() const { return berti_table_size; }
        uint64_t getBertiTableDeltaSize() const { return berti_table_delta_size; }
        uint64_t getHistoryTableSets() const { return history_table_sets; }
        uint64_t getHistoryTableWays() const { return history_table_ways; }
        uint64_t getSizeIpMask() const { return size_ip_mask; }
        uint64_t getIpMask() const { return ip_mask; }
        uint64_t getTimeMask() const { return time_mask; }
        uint64_t getLatMask() const { return lat_mask; }
        uint64_t getAddrMask() const { return addr_mask; }
        uint64_t getDeltaMask() const { return delta_mask; }
        uint64_t getTableSetMask() const { return table_set_mask; }
        uint64_t getConfidenceMax() const { return confidence_max; }
        uint64_t getConfidenceInc() const { return confidence_inc; }
        uint64_t getConfidenceInit() const { return confidence_init; }
        uint64_t getConfidenceL1() const { return confidence_l1; }
        uint64_t getConfidenceL2() const { return confidence_l2; }
        uint64_t getConfidenceL2r() const { return confidence_l2r; }
        uint64_t getConfidenceMiddleL1() const { return confidence_middle_l1; }
        uint64_t getConfidenceMiddleL2() const { return confidence_middle_l2; }
        uint64_t getLaunchMiddleConf() const { return launch_middle_conf; }
        uint64_t getMaxHistoryIp() const { return max_history_ip; }
        uint64_t getMshrLimit() const { return mshr_limit; }
        uint64_t getBertiR() const { return berti_r; }
        uint64_t getBertiL1() const { return berti_l1; }
        uint64_t getBertiL2() const { return berti_l2; }
        uint64_t getBertiL2r() const { return berti_l2r; }
        uint64_t getPageShift() const { return page_shift; }
        uint64_t getLatencyTableSize() const { return latency_table_size; }
        uint64_t getL0Sets() const { return l0_sets; }
        uint64_t getL0Ways() const { return l0_ways; }

        void setMSHRLoad(int current_mshr_load) { mshr_load = current_mshr_load; }

    private:

        /// determine the page aligned address
        Addr pageAddress(Addr addr) const;

        uint64_t berti_table_size;
        uint64_t berti_table_delta_size;

        uint64_t history_table_sets;
        uint64_t history_table_ways;

        uint64_t size_ip_mask;
        uint64_t ip_mask;
        uint64_t time_mask;
        uint64_t lat_mask;

        uint64_t addr_mask;
        uint64_t delta_mask;
        uint64_t table_set_mask;

        uint64_t confidence_max;
        uint64_t confidence_inc;
        uint64_t confidence_init;

        uint64_t confidence_l1;
        uint64_t confidence_l2;
        uint64_t confidence_l2r;

        uint64_t confidence_middle_l1;
        uint64_t confidence_middle_l2;
        uint64_t launch_middle_conf;

        uint64_t max_history_ip;
        uint64_t mshr_limit;

        uint64_t berti_r;
        uint64_t berti_l1;
        uint64_t berti_l2;
        uint64_t berti_l2r;

        const uint64_t page_shift;

        uint64_t latency_table_size;
        uint64_t l0_sets;
        uint64_t l0_ways;

        struct RubyPrefetcherStats : public statistics::Group
        {
	  RubyPrefetcherStats(statistics::Group *parent);

	  statistics::Scalar welford_average;
	  statistics::Scalar welford_num;

	  // Get more info
	  statistics::Scalar pf_to_l1 = 0;
	  statistics::Scalar pf_to_l2 = 0;
	  statistics::Scalar pf_to_l2_bc_mshr = 0;
	  statistics::Scalar cant_track_latency = 0;
	  statistics::Scalar cross_page = 0;
	  statistics::Scalar no_cross_page = 0;
	  statistics::Scalar no_found_berti = 0;
	  statistics::Scalar found_berti = 0;
	  statistics::Scalar average_issued = 0;
	  statistics::Scalar average_num = 0;
//	  statistics::Formula average = 0;
	  statistics::Scalar total_misses = 0;
	  statistics::Scalar total_hits = 0;
	  statistics::Scalar pf_hits = 0;
	  statistics::Scalar late_pf = 0;
	  statistics::Scalar total_acceses = 0;

        } rubyPrefetcherStats;

        AbstractController *m_controller;
        LatencyTable *latencyt;
        ShadowCache *scache;
        HistoryTable *historyt;
        Berti *berti;

       int mshr_load = 0; // in %

       Addr last_replaced_addr;

};

} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_STRUCTURES_PREFETCHER_HH__
