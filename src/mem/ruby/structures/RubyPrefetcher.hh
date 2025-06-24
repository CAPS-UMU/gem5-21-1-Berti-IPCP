/*
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

// Implements IPCP Prefetcher Championship

#include <bitset>

#include "base/statistics.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/slicc_interface/AbstractController.hh"
#include "mem/ruby/slicc_interface/RubyRequest.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "params/RubyPrefetcher.hh"
#include "sim/sim_object.hh"
#include "sim/system.hh"
#include "cpu/base.hh"

//--------------------- Sangam --------------------------------

namespace gem5
{

namespace ruby
{


// #define SIG_DEBUG_PRINT
#ifdef SIG_DEBUG_PRINT
#define SIG_DP(x) x
#else
#define SIG_DP(x)
#endif

#define BASE_PREFETCH_DEGREE_L1 4 // Careful about this one, I do not have time to modify all data structures to replace this define
#define NUM_STRIDES_IN_LONG_HIST_IP_TABLE 20 // Careful about this one, I do not have time to modify all data structures to replace this define

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
    void l1d_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, const RubyRequestType& type);
    /**
     * Print out some statistics
     */
    void print(std::ostream& out) const;
    void setController(AbstractController *_ctrl)
    { m_controller = _ctrl; }

    void regStats();

private:

    /// determine the page aligned address
    Addr pageAddress(Addr addr) const;

    AbstractController *m_controller;

    //! Count of accesses to the prefetcher
    statistics::Scalar numMissObserved;
    //! Count of prefetch streams allocated
    statistics::Scalar numAllocatedStreams;
    //! Count of prefetch requests made
    statistics::Scalar numPrefetchRequested;
    //! Count of prefetch requests accepted
    statistics::Scalar numPrefetchAccepted;
    //! Count of prefetches dropped
    statistics::Scalar numDroppedPrefetches;
    //! Count of successful prefetches
    statistics::Scalar numHits;
    //! Count of partial successful prefetches
    statistics::Scalar numPartialHits;
    //! Count of pages crossed
    statistics::Scalar numPagesCrossed;
    //! Count of misses incurred for blocks that were prefetched
    statistics::Scalar numMissedPrefetchedBlocks;
    // Count total cache hits
    statistics::Scalar total_hits;

    uint64_t m_num_sets_in_ip_table_l1; // OK
    uint64_t m_prediction_threshold_l1; // OK
    uint64_t m_num_sets_in_ip_delta_table_l1; // OK
    uint64_t m_page_offset_mask; // OK
    uint64_t m_throttle_level_max_l1; //OK

    uint64_t m_log_num_sets_in_recent_access_tag_array_l1; // OK
    uint64_t m_num_sets_in_recent_access_tag_array_l1; // OK
    uint64_t m_num_ways_in_recent_access_tag_array_l1; // OK

    uint64_t m_ip_table_tag_mask; // OK
    uint64_t m_log_num_sets_in_ip_table_l1; // OK
    uint64_t m_num_ways_in_ip_table_l1; // OK

    uint64_t m_ip_delta_table_tag_mask; // OK
    uint64_t m_log_num_sets_in_ip_delta_table_l1; // OK
    uint64_t m_num_ways_in_ip_delta_table_l1; // OK

    uint64_t m_saturating_counter_max_l1; // OK
    uint64_t m_base_prefetch_degree_l1; // NO // Careful about this one, I do not have time to modify all data structures to replace this define

    uint64_t m_num_entries_in_nl_buffer_l1; // OK
    uint64_t m_nl_threshold_numer_l1; // OK
    uint64_t m_nl_threshold_denom_l1; // OK

    bool m_pointer_last; // OK
    bool m_pointer_non_last; // OK
    uint64_t m_stride_conf_max; // OK
    uint64_t m_stride_conf_threshold; // OK

    uint64_t m_partial_ip_mask; // OK

    uint64_t m_num_strides_in_long_hist_ip_table; // NO // // Careful about this one, I do not have time to modify all data structures to replace this define
    uint64_t m_long_hist_ip_table_tag_mask; // OK
    uint64_t m_num_entries_in_long_hist_ip_table; // OK
    uint64_t m_long_hist_match_length; // OK

    uint64_t m_num_ip_table_l1_entries; // OK
    uint64_t m_num_ghb_entries; // OK
    uint64_t m_num_ip_index_bits; // OK
    uint64_t m_num_ip_tag_bits; // OK
    uint64_t m_s_type; // OK
    uint64_t m_cs_type; // OK
    uint64_t m_cplx_type; // OK
    uint64_t m_nl_type; // OK

    const Addr m_page_shift = 12;

    uint64_t m_num_cpus;
    uint64_t m_log2_block_size;

    unsigned char throttle_level_L1;

// Next line buffer entry: 73 bits
    class NLBufferL1 {
    public:
	// Tag
	uint64_t tag;          // 64 bits

	// LRU states
	uint64_t lru;          // 6 bits

	// Valid bit
	bool valid;            // 1 bit

	// Degree
	unsigned char degree;  // 2 bits

	NLBufferL1 () {}
    };

    NLBufferL1 *nlBufferL1;
    unsigned degreeInsertionsL1[BASE_PREFETCH_DEGREE_L1];  // 32x4 bits = 128 bits
    unsigned degreeHitsL1[BASE_PREFETCH_DEGREE_L1];        // 32x4 bits = 128 bits

// Recent access tag array entry: 71 bits
    class RecentAccessTagArrayL1 {
    public:
	// Tag
	uint64_t tag;    // 64 bits

	// LRU states
	uint64_t lru;    // 6 bits

	// Valid bit
	bool valid;      // 1 bit

	RecentAccessTagArrayL1 () {}
    };

    RecentAccessTagArrayL1 **recentAccessTagArrayL1;

// IP stride table entry: 63 bits
    class IPtableL1 {
    public:
	// Tag
	uint64_t tag;	// 63 - (3+4+1+6+7*(BASE_PREFETCH_DEGREE_L1+1)) bits
	// LRU states
	uint64_t lru;	// 4 bits
	// Valid bit
	bool valid;	// 1 bit
	// Last seen offset in a page
	unsigned char offset;	// 6 bits

	// Last seen strides within a page
	char stride[BASE_PREFETCH_DEGREE_L1+1];	// 7*(BASE_PREFETCH_DEGREE_L1+1)

	// Stride confidence
	unsigned char conf;	// 2 bits

	// Stride confidence pointer
	bool confPointer;	// 1 bit

	IPtableL1 () {}
    };

    IPtableL1 **ipTableL1;
    char ipTableStride[BASE_PREFETCH_DEGREE_L1+1];	// 7*5 bits		= 35 bits

// IP-delta correlation table entry: 64 bits
    class IPDeltaTableL1 {
    public:
	// Tag
	uint64_t tag;	// 64 - (8+3+1+9*BASE_PREFETCH_DEGREE_L1) bits
	// LRU states
	uint64_t lru;	// 3 bits
	// Valid bit
	bool valid;	// 1 bit

	// Last seen strides within a page
	char stride[BASE_PREFETCH_DEGREE_L1];	// 7*BASE_PREFETCH_DEGREE_L1 bits

	// Confidence counters
	unsigned char counters[BASE_PREFETCH_DEGREE_L1];	// 2*BASE_PREFETCH_DEGREE_L1 bits

	// Partial IP for last prediction
	uint64_t partial_ip;	// 7 bits
	bool partial_ip_valid;	// 1 bit

	IPDeltaTableL1 () {}
    };

    char ipPrefetchStride[BASE_PREFETCH_DEGREE_L1+1];	// 7*5 bits					= 35 bits
    IPDeltaTableL1 **ipDeltaTableL1;

// Long history IP stride table entry: 225 bits
    class LongHistIPtableL1 {
    public:
	// Tag
	uint64_t ip;     // 225 - (5+1+58+7*NUM_STRIDES_IN_LONG_HIST_TABLE) bits
	// LRU states
	uint64_t lru;    // 5 bits
	// Valid bit
	bool valid;  // 1 bit
	// Last seen block address
	uint64_t block_addr;	// 58 bits
	// Last seen many strides within a page
	char stride[NUM_STRIDES_IN_LONG_HIST_IP_TABLE];	// 7*NUM_STRIDES_IN_LONG_HIST_IP_TABLE

	LongHistIPtableL1 () {}
    };


    // JMCG: THIS IS NOT IMPLEMENTED, ALWAYS HAS SIZE AND ALWAYS CAN SEND PREFETCH REQUESTS
    class PrefetchQueue {
    public:
	uint32_t occupancy = 0;
	uint32_t SIZE = 1;
	PrefetchQueue () {}
    };

    PrefetchQueue PQ;

    LongHistIPtableL1 *longHistIPTableL1;
    char *longHistory;

// ----------------------- Bouquet ---------------------------------

    class IP_TABLE_L1 {
    public:
	uint64_t ip_tag;
	uint64_t last_page;	// last page seen by IP
	uint64_t last_cl_offset;	// last cl offset in the 4KB page
	int64_t last_stride;	// last delta observed
	uint16_t ip_valid;	// Valid IP or not
	int conf;   // CS conf
	uint16_t signature;	// CPLX signature
	uint16_t str_dir;	// stream direction
	uint16_t str_valid;	// stream valid
	uint16_t str_strength;	// stream strength

	IP_TABLE_L1 () {
	    ip_tag = 0;
	    last_page = 0;
	    last_cl_offset = 0;
	    last_stride	= 0;
	    ip_valid = 0;
	    signature = 0;
	    conf = 0;
	    str_dir = 0;
	    str_valid = 0;
	    str_strength = 0;
	};
    };

    class DELTA_PRED_TABLE {
    public:
	int delta;
	int conf;

	DELTA_PRED_TABLE () {
	    delta = 0;
	    conf = 0;
	};
    };

    IP_TABLE_L1	*trackers_l1;
    DELTA_PRED_TABLE DPT_l1[4096];
    uint64_t *ghb_l1;
    uint64_t prev_cpu_cycle;
    uint64_t num_misses;
    float mpkc = 0;
    int spec_nl = 0;


////#####################################################################////
//------------------- Helper Methods ----------------------------

// ------------------------ Sangam -------------------------------

    void NLBufferL1Insert (uint64_t cl_addr, int current_degree_index) {
	int j;
	for (j=0; j<m_num_entries_in_nl_buffer_l1; j++) {
	    if (nlBufferL1[j].valid && (nlBufferL1[j].tag == cl_addr)) {
		if (nlBufferL1[j].degree > (current_degree_index+1)) {
		    assert(degreeInsertionsL1[nlBufferL1[j].degree-1] > 0);
		    degreeInsertionsL1[nlBufferL1[j].degree-1]--;
		    nlBufferL1[j].degree = current_degree_index+1;	// Always favor smaller degree NL prefetcher
		    degreeInsertionsL1[current_degree_index]++;
		    for (int jj=0; jj<m_num_entries_in_nl_buffer_l1; jj++) {
			nlBufferL1[jj].lru++;
		    }
		    nlBufferL1[j].lru = 0;
		    }
		break;
		}
	}
	if (j == m_num_entries_in_nl_buffer_l1) {	// MISS
	    for (j=0; j<m_num_entries_in_nl_buffer_l1; j++) {
		if (!nlBufferL1[j].valid) break;
	    }
	    if (j == m_num_entries_in_nl_buffer_l1) {
		uint64_t maxlru = 0;
		int repl_index = -1;
		for (j=0; j<m_num_entries_in_nl_buffer_l1; j++) {
		    if (nlBufferL1[j].lru >= maxlru) {
			maxlru = nlBufferL1[j].lru;
			repl_index = j;
		    }
		}
		j = repl_index;
	    }
	    nlBufferL1[j].tag = cl_addr;
	    nlBufferL1[j].degree = current_degree_index+1;
	    nlBufferL1[j].valid = true;
	    degreeInsertionsL1[current_degree_index]++;
	    for (int jj=0; jj<m_num_entries_in_nl_buffer_l1; jj++) {
		nlBufferL1[jj].lru++;
	    }
	    nlBufferL1[j].lru = 0;
	}
    }

    void RecentAccessTagArrayL1LookupAndInsertIfMiss (uint64_t cl_addr) {
	int recentAccessTagArrayIndex = cl_addr & (m_num_sets_in_recent_access_tag_array_l1 - 1);
	uint64_t recentAccessTagArrayTag = cl_addr >> m_log_num_sets_in_recent_access_tag_array_l1;
	int	ii;
	for (ii=0; ii<m_num_ways_in_recent_access_tag_array_l1; ii++) {
	    if (recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].valid && (recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].tag == recentAccessTagArrayTag)) {
		break;
	    }
	}
	if (ii == m_num_ways_in_recent_access_tag_array_l1) {
	    for (ii=0; ii<m_num_ways_in_recent_access_tag_array_l1; ii++) {
		if (!recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].valid) break;
	    }
	    if (ii == m_num_ways_in_recent_access_tag_array_l1) {
		uint64_t maxlru = 0;
		int repl_index = -1;
		for (ii=0; ii<m_num_ways_in_recent_access_tag_array_l1; ii++) {
		    if (recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].lru > maxlru) {
			maxlru = recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].lru;
			repl_index = ii;
		    }
		}
		ii = repl_index;
	    }
	    recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].tag = recentAccessTagArrayTag;
	    recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].valid = true;
	}
	for (int jj=0; jj<m_num_ways_in_recent_access_tag_array_l1; jj++) recentAccessTagArrayL1[recentAccessTagArrayIndex][jj].lru++;
	recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].lru = 0;
    }

    bool RecentAccessTagArrayL1DetermineHit (uint64_t cl_addr) {
	int recentAccessTagArrayIndex = cl_addr & (m_num_sets_in_recent_access_tag_array_l1 - 1);
	uint64_t recentAccessTagArrayTag = cl_addr >> m_log_num_sets_in_recent_access_tag_array_l1;
	int ii;
	for (ii=0; ii<m_num_ways_in_recent_access_tag_array_l1; ii++) {
	    if (recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].valid && (recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].tag == recentAccessTagArrayTag)) {
		return true;
	    }
	}
	return false;
    }

    void RecentAccessTagArrayL1Insert (uint64_t cl_addr) {
	int recentAccessTagArrayIndex = cl_addr & (m_num_sets_in_recent_access_tag_array_l1 - 1);
	uint64_t recentAccessTagArrayTag = cl_addr >> m_log_num_sets_in_recent_access_tag_array_l1;

	int ii;

	for (ii=0; ii<m_num_ways_in_recent_access_tag_array_l1; ii++) {
	    if (!recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].valid) break;
	}
	if (ii == m_num_ways_in_recent_access_tag_array_l1) {
	    uint64_t maxlru = 0;
	    int repl_index = -1;
	    for (ii=0; ii<m_num_ways_in_recent_access_tag_array_l1; ii++) {
		if (recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].lru > maxlru) {
		    maxlru = recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].lru;
		    repl_index = ii;
		}
	    }
	    ii = repl_index;
	}
	recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].tag = recentAccessTagArrayTag;
	recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].valid = true;
	for (int jj=0; jj<m_num_ways_in_recent_access_tag_array_l1; jj++) recentAccessTagArrayL1[recentAccessTagArrayIndex][jj].lru++;
	recentAccessTagArrayL1[recentAccessTagArrayIndex][ii].lru = 0;
    }


// ------------------------------ Bouquet ---------------------------------


/***************Updating the signature*************************************/
    uint16_t update_sig_l1(uint16_t old_sig, int delta){
	uint16_t new_sig = 0;
	int sig_delta = 0;
// 7-bit sign magnitude form, since we need to track deltas from +63 to -63
	sig_delta = (delta < 0) ? (((-1) * delta) + (1 << 6)) : delta;
	new_sig	= ((old_sig << 1) ^ sig_delta) & 0xFFF;	// 12-bit signature
	return new_sig;
    }



/****************Encoding the metadata***********************************/
    uint64_t encode_metadata(int stride, uint16_t type, int in_spec_nl) {
	uint64_t metadata = 0;
// first encode stride in the last 8 bits of the metadata
	if(stride > 0)
	    metadata = stride;
	else
	    metadata = ((-1*stride) | 0b1000000);
// encode the type of IP in the next 4 bits
	metadata = metadata | (type << 8);
// encode the speculative NL bit in the next 1 bit
	metadata = metadata | (in_spec_nl << 12);
	return metadata;
    }


/*********************Checking for a global stream (GS class)***************/

    void check_for_stream_l1(int index, uint64_t cl_addr) {
	int pos_count = 0, neg_count=0, count=0;
	uint64_t check_addr = cl_addr;

// check for +ve stream
	for(int i=0; i<m_num_ghb_entries; i++){
	    check_addr--;
	    for(int j=0; j<m_num_ghb_entries; j++)
		if(check_addr == ghb_l1[j]){
		    pos_count++;
		    break;
		}
	}

	check_addr = cl_addr;
// check for -ve stream
	for(int i=0; i<m_num_ghb_entries; i++){
	    check_addr++;
	    for(int j=0; j<m_num_ghb_entries; j++)
		if(check_addr == ghb_l1[j]){
		    neg_count++;
		    break;
		}
	}

	if(pos_count > neg_count) {  // stream direction is +ve
	    trackers_l1[index].str_dir = 1;
	    count = pos_count;
	} else { // stream direction is -ve
	    trackers_l1[index].str_dir = 0;
	    count = neg_count;
	}

	if(count > m_num_ghb_entries/2) {  // stream is detected
	    trackers_l1[index].str_valid = 1;
	    if(count >= (m_num_ghb_entries*3)/4)	// stream is classified as strong if more than 3/4th entries belong to stream
		trackers_l1[index].str_strength = 1;
	} else {
	    if(trackers_l1[index].str_strength == 0)	// if identified as weak stream, we need to reset
		trackers_l1[index].str_valid = 0;
	}

    }

/************************** Updating confidence for the CS class ****************/
    int update_conf(int stride, int pred_stride, int conf) {
	if(stride == pred_stride){  // use 2-bit saturating counter for confidence
	    conf++;
	    if(conf > 3)
		conf = 3;
	} else {
	    conf--;
	    if(conf < 0)
		conf = 0;
	}

	return conf;
    }
};

} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_STRUCTURES_PREFETCHER_HH__
