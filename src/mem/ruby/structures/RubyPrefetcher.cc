#include "mem/ruby/structures/RubyPrefetcher.hh"

#include "debug/RubyPrefetcher.hh"
#include "mem/ruby/slicc_interface/RubySlicc_ComponentMapping.hh"
#include "mem/ruby/system/RubySystem.hh"


namespace gem5
{

namespace ruby
{

RubyPrefetcher::RubyPrefetcher(const Params &p)
    : SimObject(p),
      m_log_num_sets_in_recent_access_tag_array_l1(p.log_num_sets_in_recent_access_tag_array_l1),
      m_num_ways_in_recent_access_tag_array_l1(p.num_ways_in_recent_access_tag_array_l1),
      m_ip_table_tag_mask(p.ip_table_tag_mask),
      m_log_num_sets_in_ip_table_l1(p.log_num_sets_in_ip_table_l1),
      m_num_ways_in_ip_table_l1(p.num_ways_in_ip_table_l1),
      m_ip_delta_table_tag_mask(p.ip_delta_table_tag_mask),
      m_log_num_sets_in_ip_delta_table_l1(p.log_num_sets_in_ip_delta_table_l1),
      m_num_ways_in_ip_delta_table_l1(p.num_ways_in_ip_delta_table_l1),
      m_saturating_counter_max_l1(p.saturating_counter_max_l1),
      m_base_prefetch_degree_l1(p.base_prefetch_degree_l1),
      m_num_entries_in_nl_buffer_l1(p.num_entries_in_nl_buffer_l1),
      m_nl_threshold_numer_l1(p.nl_threshold_numer_l1),
      m_nl_threshold_denom_l1(p.nl_threshold_denom_l1),
      m_pointer_last(p.pointer_last),
      m_pointer_non_last(p.pointer_non_last),
      m_stride_conf_max(p.stride_conf_max),
      m_stride_conf_threshold(p.stride_conf_threshold),
      m_partial_ip_mask(p.partial_ip_mask),
      m_num_strides_in_long_hist_ip_table(p.num_strides_in_long_hist_ip_table),
      m_long_hist_ip_table_tag_mask(p.long_hist_ip_table_tag_mask),
      m_num_entries_in_long_hist_ip_table(p.num_entries_in_long_hist_ip_table),
      m_long_hist_match_length(p.long_hist_match_length),
      m_num_ip_table_l1_entries(p.num_ip_table_l1_entries),
      m_num_ghb_entries(p.num_ghb_entries),
      m_num_ip_index_bits(p.num_ip_index_bits),
      m_num_ip_tag_bits(p.num_ip_tag_bits),
      m_s_type(p.s_type),
      m_cs_type(p.cs_type),
      m_cplx_type(p.cplx_type),
      m_nl_type(p.nl_type)
{

    m_num_cpus = BaseCPU::numSimulatedCPUs();
    m_log2_block_size = RubySystem::getBlockSizeBits();

    m_prediction_threshold_l1 = ((m_num_cpus == 1) ? 2 : 3); // define num_cpus
    m_num_sets_in_ip_table_l1 = (1 << m_log_num_sets_in_ip_table_l1);
    m_num_sets_in_ip_delta_table_l1 = (1 << m_log_num_sets_in_ip_delta_table_l1);
    m_page_offset_mask = ((1 << m_page_shift) - 1);
    m_throttle_level_max_l1 = (2 + m_base_prefetch_degree_l1);

    m_num_sets_in_recent_access_tag_array_l1 = (1 << m_log_num_sets_in_recent_access_tag_array_l1);

    trackers_l1 = (IP_TABLE_L1 *)calloc(m_num_ip_table_l1_entries, sizeof(IP_TABLE_L1));
    ghb_l1 = (uint64_t *)calloc(m_num_ghb_entries,sizeof(uint64_t));

    ipTableL1 = new IPtableL1*[m_num_sets_in_ip_table_l1]; // 128x15x63 bits = 120960 bits
    for (int i = 0; i < m_num_sets_in_ip_table_l1; i++) {
	ipTableL1[i] = new IPtableL1[m_num_ways_in_ip_table_l1];
    }

    ipDeltaTableL1 = new IPDeltaTableL1*[m_num_sets_in_ip_delta_table_l1]; // 256x8x64 bits = 131072 bits
    for (int i = 0; i < m_num_sets_in_ip_delta_table_l1; i++) {
	ipDeltaTableL1[i] = new IPDeltaTableL1[m_num_ways_in_ip_delta_table_l1];
    }

    recentAccessTagArrayL1 = new RecentAccessTagArrayL1*[m_num_sets_in_recent_access_tag_array_l1]; // 1x40x71 bits = 2840 bits
    for (int i = 0; i < m_num_sets_in_recent_access_tag_array_l1; i++) {
	recentAccessTagArrayL1[i] = new RecentAccessTagArrayL1[m_num_ways_in_recent_access_tag_array_l1];
    }

    nlBufferL1 = new NLBufferL1[m_num_entries_in_nl_buffer_l1];       // 64x73 bits = 4672 bits

    longHistIPTableL1 = new LongHistIPtableL1[m_num_entries_in_long_hist_ip_table];	// 225*32 bits			= 7200 bits
    longHistory = new char[m_num_entries_in_long_hist_ip_table+1]; // 7*(NUM_ENTRIES_IN_LONG_HIST_IP_TABLE+1) bits


    for (int i=0; i<m_num_entries_in_nl_buffer_l1; i++) {
	nlBufferL1[i].valid = false;
	nlBufferL1[i].lru = 0;
    }

    for (int i=0; i<BASE_PREFETCH_DEGREE_L1; i++) {
	degreeInsertionsL1[i] = 0;
	degreeHitsL1[i] = 0;
    }

    for (int i=0; i<m_num_sets_in_recent_access_tag_array_l1; i++) {
	for (int j=0; j<m_num_ways_in_recent_access_tag_array_l1; j++) {
	    recentAccessTagArrayL1[i][j].valid = false;
	    recentAccessTagArrayL1[i][j].lru = 0;
	}
    }

    for (int i=0; i<m_num_sets_in_ip_table_l1; i++) {
	for (int j=0; j<m_num_ways_in_ip_table_l1; j++) {
	    ipTableL1[i][j].valid = false;
	    ipTableL1[i][j].lru = 0;
	}
    }

    for (int i=0; i<m_num_sets_in_ip_delta_table_l1; i++) {
	for (int j=0; j<m_num_ways_in_ip_delta_table_l1; j++) {
	    ipDeltaTableL1[i][j].valid = false;
	    ipDeltaTableL1[i][j].lru = 0;
	}
    }

    for (int i=0; i<m_num_entries_in_long_hist_ip_table; i++) {
	longHistIPTableL1[i].valid = false;
	longHistIPTableL1[i].lru = 0;
    }

    throttle_level_L1 = 0;

}

void
RubyPrefetcher::regStats()
{
    SimObject::regStats();

    numMissObserved
        .name(name() + ".miss_observed")
        .desc("number of misses observed")
        ;

    numAllocatedStreams
        .name(name() + ".allocated_streams")
        .desc("number of streams allocated for prefetching")
        ;

    numPrefetchRequested
        .name(name() + ".prefetches_requested")
        .desc("number of prefetch requests made")
        ;

    numPrefetchAccepted
        .name(name() + ".prefetches_accepted")
        .desc("number of prefetch requests accepted")
        ;

    numDroppedPrefetches
        .name(name() + ".dropped_prefetches")
        .desc("number of prefetch requests dropped")
        ;

    numHits
        .name(name() + ".hits")
        .desc("number of prefetched blocks accessed")
        ;

    total_hits
        .name(name() + ".total_hits")
        .desc("number of cache hits that are not pf")
        ;

    numPartialHits
        .name(name() + ".partial_hits")
        .desc("number of misses observed for a block being prefetched")
        ;

    numPagesCrossed
        .name(name() + ".pages_crossed")
        .desc("number of prefetches across pages")
        ;

    numMissedPrefetchedBlocks
        .name(name() + ".misses_on_prefetched_blocks")
        .desc("number of misses for blocks that were prefetched, yet missed")
        ;
}

void
RubyPrefetcher::observeMiss(Addr address, const RubyRequestType& type, Addr pc)
{
    DPRINTF(RubyPrefetcher, "Observed miss for %#x\n", address);
    Addr line_addr = makeLineAddress(address);
    numMissObserved++;
    l1d_prefetcher_operate((uint64_t)line_addr, (uint64_t)pc, 0, type);
}

void
RubyPrefetcher::observePfMiss(Addr address, const RubyRequestType& type, Addr pc)
{
    numPartialHits++;
    DPRINTF(RubyPrefetcher, "Observed partial hit for %#x\n", address);
    l1d_prefetcher_operate((uint64_t)address, (uint64_t)pc, 0, type);
}

void
RubyPrefetcher::observePfHit(Addr address, const RubyRequestType& type, Addr pc)
{
    numHits++;
    DPRINTF(RubyPrefetcher, "Observed hit for %#x\n", address);
    l1d_prefetcher_operate((uint64_t)address, (uint64_t)pc, 1, type);
}

void
RubyPrefetcher::observeHit(Addr address, const RubyRequestType& type, Addr pc)
{
    total_hits++;
}


void RubyPrefetcher::print(std::ostream& out) const { }

Addr RubyPrefetcher::pageAddress(Addr addr) const { return maskLowOrderBits(addr, m_page_shift); }


/***************************************************************************
For the Third Data Prefetching Championship - DPC3
Paper ID: #4
Instruction Pointer Classifying Prefetcher - IPCP
Authors:
Samuel Pakalapati - samuelpakalapati@gmail.com
Biswabandan Panda - biswap@cse.iitk.ac.in
***************************************************************************/

///////////////////////////////////////////////////////////////////////////
//////////////////////       MAIN ROUTINE        //////////////////////////
///////////////////////////////////////////////////////////////////////////


void RubyPrefetcher::l1d_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, const RubyRequestType& type)
{
    int i = 0;

    uint64_t curr_page = addr >> m_page_shift;
    uint64_t cl_addr = addr >> m_log2_block_size;
    uint64_t cl_offset = (addr >> m_log2_block_size) & 0x3F;
    uint16_t signature = 0, last_signature = 0;
    int prefetch_degree = 0;
    int spec_nl_threshold = 0;
    int num_prefs = 0;
    uint64_t __attribute__((unused)) metadata=0;
    uint16_t ip_tag = (ip >> m_num_ip_index_bits) & ((1 << m_num_ip_tag_bits)-1);

    uint64_t pageid = addr >> m_page_shift;
    unsigned char offset = (addr & m_page_offset_mask) >> m_log2_block_size;
    bool did_pref = false;
    bool current_delta_nonzero = false;

    if(m_num_cpus == 1) {
        prefetch_degree = 3;
        spec_nl_threshold = 15;
    } else {                                    // tightening the degree and MPKC constraints for multi-core
        prefetch_degree = 2;
        spec_nl_threshold = 5;
    }

    // update miss counter
    if(cache_hit == 0)
        num_misses += 1;

    // update spec nl bit when num misses crosses certain threshold
    if(num_misses == 256){
        mpkc = ((float) num_misses/(m_controller->curCycle()-prev_cpu_cycle))*1000;
        prev_cpu_cycle = m_controller->curCycle(); // Alternative m_controller->curCycle()
        if(mpkc > spec_nl_threshold)
            spec_nl = 0;
        else
            spec_nl = 1;
        num_misses = 0;
    }

    //
    //  Long history IP table lookup
    //
    char longHistIPTableNewDelta = 0;
    for (i=0; i<m_num_entries_in_long_hist_ip_table; i++) {
        if (longHistIPTableL1[i].valid && (longHistIPTableL1[i].ip == (ip & m_long_hist_ip_table_tag_mask))) {
            for (int j=0; j<NUM_STRIDES_IN_LONG_HIST_IP_TABLE; j++) longHistory[j] = longHistIPTableL1[i].stride[j];
            if (pageid == (longHistIPTableL1[i].block_addr >> (m_page_shift - m_log2_block_size))) {
                longHistory[NUM_STRIDES_IN_LONG_HIST_IP_TABLE] = offset - (((longHistIPTableL1[i].block_addr << m_log2_block_size) & m_page_offset_mask) >> m_log2_block_size);
            }
            else longHistory[NUM_STRIDES_IN_LONG_HIST_IP_TABLE] = 0;
            longHistIPTableNewDelta = longHistory[NUM_STRIDES_IN_LONG_HIST_IP_TABLE];
            longHistIPTableL1[i].block_addr = cl_addr;
            if (longHistIPTableNewDelta != 0) {
                for (int j=0; j<NUM_STRIDES_IN_LONG_HIST_IP_TABLE-1; j++) longHistIPTableL1[i].stride[j] = longHistIPTableL1[i].stride[j+1];
                longHistIPTableL1[i].stride[NUM_STRIDES_IN_LONG_HIST_IP_TABLE-1] = longHistIPTableNewDelta;
            }
            break;
        }
    }
    if (i == m_num_entries_in_long_hist_ip_table) {
        for (i=0; i<m_num_entries_in_long_hist_ip_table; i++) {
            if (!longHistIPTableL1[i].valid) break;
        }
        if (i == m_num_entries_in_long_hist_ip_table) {
            uint64_t maxlru = 0;
            int rep_index = 0;
            for (i=0; i<m_num_entries_in_long_hist_ip_table; i++) {
                if (longHistIPTableL1[i].lru >= maxlru) {
                maxlru = longHistIPTableL1[i].lru;
                rep_index = i;
            }
            }
            i = rep_index;
        }
        assert(i < m_num_entries_in_long_hist_ip_table);
        longHistIPTableL1[i].ip = (ip & m_long_hist_ip_table_tag_mask);
        longHistIPTableL1[i].block_addr = cl_addr;
        for (int j=0; j<NUM_STRIDES_IN_LONG_HIST_IP_TABLE; j++) longHistIPTableL1[i].stride[j] = 0;
        longHistIPTableL1[i].valid = true;
    }
    for (int j=0; j<m_num_entries_in_long_hist_ip_table; j++)
        longHistIPTableL1[j].lru++;
    longHistIPTableL1[i].lru = 0;

    //
    // NL Buffer Lookup
    //
    for (i=0; i<m_num_entries_in_nl_buffer_l1; i++) {
        // Determine next line prefetcher's usefulness
        if (nlBufferL1[i].valid && (nlBufferL1[i].tag == cl_addr)) {
            degreeHitsL1[nlBufferL1[i].degree-1]++;
            nlBufferL1[i].valid = false;
            break;
        }
    }

    //
    // IP Table Lookup
    //
    bool constantStrideValid = false;
    char constantStride = 0;
    int ipTableIndex = (pageid) & (m_num_sets_in_ip_table_l1 - 1);
    uint64_t ipTableTag = ((pageid) >> m_log_num_sets_in_ip_table_l1) & m_ip_table_tag_mask;
    int ii;
    for (ii=0; ii<m_num_ways_in_ip_table_l1; ii++) {
        if (ipTableL1[ipTableIndex][ii].valid && (ipTableL1[ipTableIndex][ii].tag == ipTableTag)) {
            if ((signed)(offset - ipTableL1[ipTableIndex][ii].offset) != 0) {
                current_delta_nonzero = true;
                for (i=0; i<BASE_PREFETCH_DEGREE_L1+1; i++) {
                if (ipTableL1[ipTableIndex][ii].stride[i] == 0) {
                    ipTableL1[ipTableIndex][ii].stride[i] = (signed)(offset - ipTableL1[ipTableIndex][ii].offset);
                    break;
                }
                }
                if (i == BASE_PREFETCH_DEGREE_L1+1) {
                for (i=0; i<BASE_PREFETCH_DEGREE_L1; i++) {
                    ipTableL1[ipTableIndex][ii].stride[i] = ipTableL1[ipTableIndex][ii].stride[i+1];
                }
                ipTableL1[ipTableIndex][ii].stride[i] = (signed)(offset - ipTableL1[ipTableIndex][ii].offset);
                }

                if (i == 0) {
                ipTableL1[ipTableIndex][ii].conf = 0;
                ipTableL1[ipTableIndex][ii].confPointer = m_pointer_last;
                }
                else if (ipTableL1[ipTableIndex][ii].stride[i] == ipTableL1[ipTableIndex][ii].stride[i-1]) {
                if (ipTableL1[ipTableIndex][ii].confPointer == m_pointer_last) {
                    if (ipTableL1[ipTableIndex][ii].conf < m_stride_conf_max) ipTableL1[ipTableIndex][ii].conf++;
                }
                else {
                    ipTableL1[ipTableIndex][ii].conf = 1;
                    ipTableL1[ipTableIndex][ii].confPointer = m_pointer_last;
                }
                }
                else {
                if (ipTableL1[ipTableIndex][ii].confPointer == m_pointer_last) {
                    ipTableL1[ipTableIndex][ii].confPointer = m_pointer_non_last;
                if (ipTableL1[ipTableIndex][ii].conf > 0) ipTableL1[ipTableIndex][ii].conf--;
                }
                else {
                    assert(i > 1);
                    ipTableL1[ipTableIndex][ii].confPointer = m_pointer_last;
                    if (ipTableL1[ipTableIndex][ii].stride[i] == ipTableL1[ipTableIndex][ii].stride[i-2]) {
                        if (ipTableL1[ipTableIndex][ii].conf < m_stride_conf_max) ipTableL1[ipTableIndex][ii].conf++;
                    }
                    else {
                        ipTableL1[ipTableIndex][ii].conf = 0;
                    }
                }
                }

            if ((ipTableL1[ipTableIndex][ii].conf >= m_stride_conf_threshold) && (ipTableL1[ipTableIndex][ii].stride[i] == ipTableL1[ipTableIndex][ii].stride[i-1])) {
                constantStride = ipTableL1[ipTableIndex][ii].stride[i];
                constantStrideValid = true;
                }

                ipTableL1[ipTableIndex][ii].offset = offset;
            }
            break;
        }
    }
    if (ii == m_num_ways_in_ip_table_l1) {
        for (ii=0; ii<m_num_ways_in_ip_table_l1; ii++) {
            if (!ipTableL1[ipTableIndex][ii].valid) break;
        }
        if (ii == m_num_ways_in_ip_table_l1) {
            uint64_t maxlru = 0;
            int repl_index = -1;
            for (ii=0; ii<m_num_ways_in_ip_table_l1; ii++) {
                if (ipTableL1[ipTableIndex][ii].lru > maxlru) {
                    maxlru = ipTableL1[ipTableIndex][ii].lru;
                    repl_index = ii;
                }
            }
            ii = repl_index;
        }
        ipTableL1[ipTableIndex][ii].tag = ipTableTag;
        ipTableL1[ipTableIndex][ii].offset = offset;
        for (i=0; i<BASE_PREFETCH_DEGREE_L1+1; i++)
            ipTableL1[ipTableIndex][ii].stride[i] = 0;
        ipTableL1[ipTableIndex][ii].valid = true;
    }
    for (i=0; i<m_num_ways_in_ip_table_l1; i++) ipTableL1[ipTableIndex][i].lru++;
    ipTableL1[ipTableIndex][ii].lru = 0;
    int lastNonZeroIndex = -1;
    for (i=0; i<BASE_PREFETCH_DEGREE_L1+1; i++) {
        ipTableStride[i] = ipTableL1[ipTableIndex][ii].stride[i];
        if (ipTableStride[i] != 0) lastNonZeroIndex = i;
    }

    //
    // IP delta table lookup and training
    //
    if (current_delta_nonzero) {
      for (i=0; i<lastNonZeroIndex; i++) {
         assert(ipTableStride[i] != 0);
         unsigned delta;
         delta = (ipTableStride[i] >= 0) ? ipTableStride[i] : ((-ipTableStride[i]) | (1 <<  (m_page_shift - m_log2_block_size)));
         int ipDeltaTableIndex = (((pageid) << 3) ^ delta) & (m_num_sets_in_ip_delta_table_l1 - 1);
         uint64_t ipDeltaTableTag = ((((pageid) << 3) ^ delta) >> m_log_num_sets_in_ip_delta_table_l1) & m_ip_delta_table_tag_mask;
         for (ii=0; ii<m_num_ways_in_ip_delta_table_l1; ii++) {
            if (ipDeltaTableL1[ipDeltaTableIndex][ii].valid && (ipDeltaTableL1[ipDeltaTableIndex][ii].tag == ipDeltaTableTag)) {
               if (ipTableStride[lastNonZeroIndex] == ipDeltaTableL1[ipDeltaTableIndex][ii].stride[lastNonZeroIndex-i-1]) {
                  if (ipDeltaTableL1[ipDeltaTableIndex][ii].counters[lastNonZeroIndex-i-1] < m_saturating_counter_max_l1) {
                     ipDeltaTableL1[ipDeltaTableIndex][ii].counters[lastNonZeroIndex-i-1]++;
                  }
               }
               else {
                  ipDeltaTableL1[ipDeltaTableIndex][ii].stride[lastNonZeroIndex-i-1] = ipTableStride[lastNonZeroIndex];
                  ipDeltaTableL1[ipDeltaTableIndex][ii].counters[lastNonZeroIndex-i-1] = 1;
               }
               break;
            }
         }
         if (ii == m_num_ways_in_ip_delta_table_l1) {
            for (ii=0; ii<m_num_ways_in_ip_delta_table_l1; ii++) {
               if (!ipDeltaTableL1[ipDeltaTableIndex][ii].valid) break;
            }
            if (ii == m_num_ways_in_ip_delta_table_l1) {
               uint64_t maxlru = 0;
               int repl_index = -1;
               for (ii=0; ii<m_num_ways_in_ip_delta_table_l1; ii++) {
                  if (ipDeltaTableL1[ipDeltaTableIndex][ii].lru > maxlru) {
                     maxlru = ipDeltaTableL1[ipDeltaTableIndex][ii].lru;
                     repl_index = ii;
                  }
               }
               ii = repl_index;
            }
            ipDeltaTableL1[ipDeltaTableIndex][ii].partial_ip_valid = false;
            ipDeltaTableL1[ipDeltaTableIndex][ii].tag = ipDeltaTableTag;
            for (int j=0; j<BASE_PREFETCH_DEGREE_L1; j++) {
               if (i+j+1 < BASE_PREFETCH_DEGREE_L1+1) {
                  ipDeltaTableL1[ipDeltaTableIndex][ii].stride[j] = ipTableStride[i+j+1];
                  if (ipTableStride[i+j+1] != 0) ipDeltaTableL1[ipDeltaTableIndex][ii].counters[j] = 1;
                  else ipDeltaTableL1[ipDeltaTableIndex][ii].counters[j] = 0;
               }
               else {
                  ipDeltaTableL1[ipDeltaTableIndex][ii].stride[j] = 0;
                  ipDeltaTableL1[ipDeltaTableIndex][ii].counters[j] = 0;
               }
            }
            ipDeltaTableL1[ipDeltaTableIndex][ii].valid = true;
         }
         for (int j=0; j<m_num_ways_in_ip_delta_table_l1; j++) ipDeltaTableL1[ipDeltaTableIndex][j].lru++;
         ipDeltaTableL1[ipDeltaTableIndex][ii].lru = 0;
      }
   }
    RecentAccessTagArrayL1LookupAndInsertIfMiss (cl_addr);


    //-----------------------------------------------------------------------
    //-------------------- Main Prefetching decision ------------------------
    //-----------------------------------------------------------------------


    // calculate the index bit
    int index = ip & ((1 << m_num_ip_index_bits)-1);
    if(trackers_l1[index].ip_tag != ip_tag){               // new/conflict IP
        if(trackers_l1[index].ip_valid == 0){              // if valid bit is zero, update with latest IP info
        trackers_l1[index].ip_tag = ip_tag;
        trackers_l1[index].last_page = curr_page;
        trackers_l1[index].last_cl_offset = cl_offset;
        trackers_l1[index].last_stride = 0;
        trackers_l1[index].signature = 0;
        trackers_l1[index].conf = 0;
        trackers_l1[index].str_valid = 0;
        trackers_l1[index].str_strength = 0;
        trackers_l1[index].str_dir = 0;
        trackers_l1[index].ip_valid = 1;
    } else {                                                    // otherwise, reset valid bit and leave the previous IP as it is
        trackers_l1[index].ip_valid = 0;
    }

    // issue a next line prefetch upon encountering new IP
        uint64_t pf_address = ((addr>>m_log2_block_size)+1) << m_log2_block_size; // BASE NL=1, changing it to 3
        metadata = encode_metadata(1, m_nl_type, spec_nl);
	m_controller->enqueuePrefetch(pf_address, type);
        return;
    }
    else {                                                     // if same IP encountered, set valid bit
        trackers_l1[index].ip_valid = 1;
    }


    // calculate the stride between the current address and the last address
    int64_t stride = 0;
    if (cl_offset > trackers_l1[index].last_cl_offset)
        stride = cl_offset - trackers_l1[index].last_cl_offset;
    else {
        stride = trackers_l1[index].last_cl_offset - cl_offset;
        stride *= -1;
    }

    // don't do anything if same address is seen twice in a row
    if (stride == 0)
        return;


    // page boundary learning
    if(curr_page != trackers_l1[index].last_page){
        if(stride < 0)
            stride += 64;
        else
            stride -= 64;
    }

    // update constant stride(CS) confidence
    trackers_l1[index].conf = update_conf(stride, trackers_l1[index].last_stride, trackers_l1[index].conf);

    // update CS only if confidence is zero
    if(trackers_l1[index].conf == 0)
        trackers_l1[index].last_stride = stride;

    last_signature = trackers_l1[index].signature;
    // update complex stride(CPLX) confidence
    DPT_l1[last_signature].conf = update_conf(stride, DPT_l1[last_signature].delta, DPT_l1[last_signature].conf);

    // update CPLX only if confidence is zero
    if(DPT_l1[last_signature].conf == 0)
        DPT_l1[last_signature].delta = stride;

    // calculate and update new signature in IP table
    signature = update_sig_l1(last_signature, stride);
    trackers_l1[index].signature = signature;

    // check GHB for stream IP
    check_for_stream_l1(index, cl_addr);

    SIG_DP(
    cout << ip << ", " << cache_hit << ", " << cl_addr << ", " << addr << ", " << stride << "; ";
    cout << last_signature<< ", "  << DPT_l1[last_signature].delta<< ", "  << DPT_l1[last_signature].conf << "; ";
    cout << trackers_l1[index].last_stride << ", " << stride << ", " << trackers_l1[index].conf << ", " << "; ";
    );

    if(trackers_l1[index].str_valid == 1){                         // stream IP
        // for stream, prefetch with twice the usual degree
            prefetch_degree = prefetch_degree*2;
        for (int i=0; i<prefetch_degree; i++) {
            uint64_t pf_address = 0;

            if(trackers_l1[index].str_dir == 1){                   // +ve stream
                pf_address = (cl_addr + i + 1) << m_log2_block_size;
                metadata = encode_metadata(1, m_s_type, spec_nl);    // stride is 1
            }
            else{                                                       // -ve stream
                pf_address = (cl_addr - i - 1) << m_log2_block_size;
                metadata = encode_metadata(-1, m_s_type, spec_nl);   // stride is -1
            }

            // Check if prefetch address is in same 4 KB page
            if ((pf_address >> m_page_shift) != (addr >> m_page_shift)){
                break;
            }

	    m_controller->enqueuePrefetch(pf_address, type);
            num_prefs++;
            SIG_DP(cout << "1, ");
            }

    } else if(trackers_l1[index].conf > 1 && trackers_l1[index].last_stride != 0){            // CS IP
        for (int i=0; i<prefetch_degree; i++) {
            uint64_t pf_address = (cl_addr + (trackers_l1[index].last_stride*(i+1))) << m_log2_block_size;

            // Check if prefetch address is in same 4 KB page
            if ((pf_address >> m_page_shift) != (addr >> m_page_shift)){
                break;
            }

            metadata = encode_metadata(trackers_l1[index].last_stride, m_cs_type, spec_nl);
	    m_controller->enqueuePrefetch(pf_address, type);
            num_prefs++;
            SIG_DP(cout << trackers_l1[index].last_stride << ", ");
        }
    } else if(DPT_l1[signature].conf >= 0 && DPT_l1[signature].delta != 0) {  // if conf>=0, continue looking for delta
        int pref_offset = 0,i=0;                                                        // CPLX IP
        for (i=0; i<prefetch_degree; i++) {
            pref_offset += DPT_l1[signature].delta;
            uint64_t pf_address = ((cl_addr + pref_offset) << m_log2_block_size);

            // Check if prefetch address is in same 4 KB page
            if (((pf_address >> m_page_shift) != (addr >> m_page_shift)) ||
                    (DPT_l1[signature].conf == -1) ||
                    (DPT_l1[signature].delta == 0)){
                // if new entry in DPT or delta is zero, break
                break;
            }

            // we are not prefetching at L2 for CPLX type, so encode delta as 0
            metadata = encode_metadata(0, m_cplx_type, spec_nl);
            if(DPT_l1[signature].conf > 0){                                 // prefetch only when conf>0 for CPLX
		m_controller->enqueuePrefetch(pf_address, type);
                num_prefs++;
                SIG_DP(cout << pref_offset << ", ");
            }
            signature = update_sig_l1(signature, DPT_l1[signature].delta);
        }
    }

    // update the IP table entries
    trackers_l1[index].last_cl_offset = cl_offset;
    trackers_l1[index].last_page = curr_page;

    // update GHB
    // search for matching cl addr
    int ghb_index=0;
    for(ghb_index = 0; ghb_index < m_num_ghb_entries; ghb_index++)
        if(cl_addr == ghb_l1[ghb_index])
            break;
    // only update the GHB upon finding a new cl address
    if(ghb_index == m_num_ghb_entries){
        for(ghb_index=m_num_ghb_entries-1; ghb_index>0; ghb_index--)
            ghb_l1[ghb_index] = ghb_l1[ghb_index-1];
        ghb_l1[0] = cl_addr;
    }

    if ((lastNonZeroIndex == -1) || !current_delta_nonzero) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    INVOKE  LONG  HISTORY  IP  PREFETCHER  IF  NO  PREFETCH  YET                                     //
////////////////////////////////////////////////////////////////////////////////////////////////////////
      if (longHistIPTableNewDelta) {
         int j, length, chosen_j=-1;
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    DETERMINE STRIDE PATTERN MATCH WITH MATCH_LENGTH HISTORY                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////
         for (j=0; j<NUM_STRIDES_IN_LONG_HIST_IP_TABLE+1-m_long_hist_match_length; j++) {
            length = 0;
            while (length != m_long_hist_match_length) {
               if (longHistory[NUM_STRIDES_IN_LONG_HIST_IP_TABLE+1-m_long_hist_match_length+length] != longHistory[j+length]) break;
               length++;
            }
            if (length == m_long_hist_match_length) {
               assert(chosen_j == -1);
               chosen_j = j;
               break;
            }
         }
         if (chosen_j != -1) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    MATCHING LONGEST PATTERN DETERMINED                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////
            j = chosen_j + m_long_hist_match_length;
            if (throttle_level_L1 == 0) {
               while (j < NUM_STRIDES_IN_LONG_HIST_IP_TABLE+1) {
                  uint64_t pf_address = (cl_addr + longHistory[j]) << m_log2_block_size;
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    RECENT  ACCESS  TAG  ARRAY  LOOKUP  TO  DETERMINE  RECENT  ACCESS  TO  PREFETCH  CANDIDATE       //
////////////////////////////////////////////////////////////////////////////////////////////////////////
                  if (!RecentAccessTagArrayL1DetermineHit(pf_address >> m_log2_block_size)) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    MISS  IN  RECENT  ACCESS  TAG  ARRAY, INJECT  PREFETCH                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////
		      m_controller->enqueuePrefetch(pf_address, type);
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//                           INSERT  IN  RECENT  ACCESS  TAG  ARRAY                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////
		      RecentAccessTagArrayL1Insert(pf_address >> m_log2_block_size);
		      did_pref = true;
                  }
                  j++;
               }
               if (did_pref) return;
            }
         }
      }

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    INVOKE  NEXT  LINE  PREFETCHER  IF  IP-DELTA  TABLE  CANNOT  OFFER  PREDICTION                  //
////////////////////////////////////////////////////////////////////////////////////////////////////////
      uint64_t pf_address = (cl_addr+1) << m_log2_block_size;
      if (throttle_level_L1 == 0) {
         i = 0;
         while (i < BASE_PREFETCH_DEGREE_L1) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    INJECT  PREFETCHES  FOR  APPROPRIATE  DEGREE                                                     //
////////////////////////////////////////////////////////////////////////////////////////////////////////
            if (degreeHitsL1[i]*m_nl_threshold_denom_l1 > degreeInsertionsL1[i]*m_nl_threshold_numer_l1) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    RECENT  ACCESS  TAG  ARRAY  LOOKUP  TO  DETERMINE  RECENT  ACCESS  TO  PREFETCH  CANDIDATE       //
////////////////////////////////////////////////////////////////////////////////////////////////////////
               if (!RecentAccessTagArrayL1DetermineHit (pf_address >> m_log2_block_size)) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    MISS  IN  RECENT  ACCESS  TAG  ARRAY, INJECT  PREFETCH                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////
		   m_controller->enqueuePrefetch(pf_address, type);
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//                           INSERT  IN  RECENT  ACCESS  TAG  ARRAY                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////
		   RecentAccessTagArrayL1Insert (pf_address >> m_log2_block_size);
               }
            }
            pf_address = pf_address + (1 << m_log2_block_size);
            i++;
         }
      }
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    INSERT  POSSIBLE  NEXT  LINE  PREFETCH  CANDIDATES  IN  THE  NL  BUFFER  TO  MEASURE  USEFULNESS //
////////////////////////////////////////////////////////////////////////////////////////////////////////
      i = 0;
      pf_address = (cl_addr + 1) << m_log2_block_size;
      while (i < BASE_PREFETCH_DEGREE_L1) {
         if ((pf_address >> m_page_shift) != (addr >> m_page_shift)) break;
         NLBufferL1Insert (pf_address >> m_log2_block_size, i);
         pf_address = pf_address + (1 << m_log2_block_size);
         i++;
      }
      return;
   }

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//                     IP  DELTA  TABLE  LOOKUP  TO  DECIDE  PREFETCH  CANDIDATES                      //
////////////////////////////////////////////////////////////////////////////////////////////////////////
   ipPrefetchStride[0] = ipTableStride[lastNonZeroIndex];
   for (i=1; i<BASE_PREFETCH_DEGREE_L1+1; i++) ipPrefetchStride[i] = 0;
   unsigned delta = (ipTableStride[lastNonZeroIndex] >= 0) ? ipTableStride[lastNonZeroIndex] : ((-ipTableStride[lastNonZeroIndex]) | (1 <<  (m_page_shift - m_log2_block_size)));
   int ipDeltaTableIndex = (((pageid) << 3) ^ delta) & (m_num_sets_in_ip_delta_table_l1 - 1);
   uint64_t ipDeltaTableTag = ((((pageid) << 3) ^ delta) >> m_log_num_sets_in_ip_delta_table_l1) & m_ip_delta_table_tag_mask;
   for (ii=0; ii<m_num_ways_in_ip_delta_table_l1; ii++) {
      if (ipDeltaTableL1[ipDeltaTableIndex][ii].valid && (ipDeltaTableL1[ipDeltaTableIndex][ii].tag == ipDeltaTableTag)) {
         for (i=1; i<BASE_PREFETCH_DEGREE_L1+1; i++) {
	    if (m_num_cpus == 1) {
               if (((i < BASE_PREFETCH_DEGREE_L1) && (ipDeltaTableL1[ipDeltaTableIndex][ii].counters[i-1] >= m_prediction_threshold_l1)) ||
                   ((i == BASE_PREFETCH_DEGREE_L1) && (ipDeltaTableL1[ipDeltaTableIndex][ii].counters[i-1] >= (m_prediction_threshold_l1 + 1)))) {
                  ipPrefetchStride[i] = ipDeltaTableL1[ipDeltaTableIndex][ii].stride[i-1];
               }
            }
            else {
               if (ipDeltaTableL1[ipDeltaTableIndex][ii].counters[i-1] >= m_prediction_threshold_l1) {
                  ipPrefetchStride[i] = ipDeltaTableL1[ipDeltaTableIndex][ii].stride[i-1];
               }
            }
         }
         break;
      }
   }
   if (ii < m_num_ways_in_ip_delta_table_l1) {
      for (int j=0; j<m_num_ways_in_ip_delta_table_l1; j++) ipDeltaTableL1[ipDeltaTableIndex][j].lru++;
      ipDeltaTableL1[ipDeltaTableIndex][ii].lru = 0;
      ipDeltaTableL1[ipDeltaTableIndex][ii].partial_ip = ip & m_partial_ip_mask;
      ipDeltaTableL1[ipDeltaTableIndex][ii].partial_ip_valid = true;
   }
   else {
      for (ii=0; ii<m_num_ways_in_ip_delta_table_l1; ii++) {
         if (ipDeltaTableL1[ipDeltaTableIndex][ii].valid && ipDeltaTableL1[ipDeltaTableIndex][ii].partial_ip_valid && (ipDeltaTableL1[ipDeltaTableIndex][ii].partial_ip == (ip & m_partial_ip_mask))) {
            for (i=1; i<BASE_PREFETCH_DEGREE_L1+1; i++) {
               if (m_num_cpus == 1) {
                  if (((i < BASE_PREFETCH_DEGREE_L1) && (ipDeltaTableL1[ipDeltaTableIndex][ii].counters[i-1] >= m_prediction_threshold_l1)) ||
                      ((i == BASE_PREFETCH_DEGREE_L1) && (ipDeltaTableL1[ipDeltaTableIndex][ii].counters[i-1] >= (m_prediction_threshold_l1 + 1)))) {
                     ipPrefetchStride[i] = ipDeltaTableL1[ipDeltaTableIndex][ii].stride[i-1];
                  }
               }
               else {
                  if (ipDeltaTableL1[ipDeltaTableIndex][ii].counters[i-1] >= m_prediction_threshold_l1) {
                     ipPrefetchStride[i] = ipDeltaTableL1[ipDeltaTableIndex][ii].stride[i-1];
                  }
               }
            }
            break;
         }
      }
   }

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//                     INJECT  PREFETCHES                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////
   uint64_t pf_address = cl_addr << m_log2_block_size;
   bool stopPrefetching = false;
   int num_pref = 0;
   if (throttle_level_L1 < m_throttle_level_max_l1) {
      for (i=1; i<((throttle_level_L1 > 2) ? BASE_PREFETCH_DEGREE_L1 - (throttle_level_L1 - 2) + 1 : BASE_PREFETCH_DEGREE_L1 + 1); i++) {
         if (ipPrefetchStride[i] == 0) break;
         pf_address = pf_address + (ipPrefetchStride[i] << m_log2_block_size);
         if ((pf_address >> m_page_shift) != (addr >> m_page_shift)) continue;

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    RECENT  ACCESS  TAG  ARRAY  LOOKUP  TO  DETERMINE  RECENT  ACCESS  TO  PREFETCH  CANDIDATE       //
////////////////////////////////////////////////////////////////////////////////////////////////////////
         if (!RecentAccessTagArrayL1DetermineHit (pf_address >> m_log2_block_size)) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    MISS  IN  RECENT  ACCESS  TAG  ARRAY, INJECT  PREFETCH                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////
            if (PQ.occupancy < (PQ.SIZE - 1)) {
		m_controller->enqueuePrefetch(pf_address, type);
//               assert(prefetch_line(ip, addr, pf_address, FILL_L1, 0));
               did_pref = true;
            }
            else if (PQ.occupancy < PQ.SIZE) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    HAND  OVER  REMAINING  PREFETCH  INJECTION  TO  L2  CACHE  BY  ENCODING  THE  METADATA           //
////////////////////////////////////////////////////////////////////////////////////////////////////////
               uint32_t pfmetadata = 0;
               unsigned char residue = 0;
               for (int j=i+1; j<((throttle_level_L1 > 2) ? BASE_PREFETCH_DEGREE_L1 - (throttle_level_L1 - 2) + 1 : BASE_PREFETCH_DEGREE_L1 + 1); j++) {
                  if (ipPrefetchStride[j] == 0) break;
                  unsigned char delta = ((ipPrefetchStride[j] < 0) ? ((-ipPrefetchStride[j]) | (1 << (m_page_shift - m_log2_block_size))) : ipPrefetchStride[j]);
                  pfmetadata = pfmetadata | (delta << ((1 + m_page_shift - m_log2_block_size)*residue));
                  residue++;
               }
	       m_controller->enqueuePrefetch(pf_address, type);
//               assert(prefetch_line(ip, addr, pf_address, FILL_L1, pfmetadata));
               did_pref = true;
            }
            else {
		m_controller->enqueuePrefetch(pf_address, type);
//               assert(!prefetch_line(ip, addr, pf_address, FILL_L1, 0));
		stopPrefetching = true;
		break;
            }
            num_pref++;
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//                           INSERT  IN  RECENT  ACCESS  TAG  ARRAY                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////
            RecentAccessTagArrayL1Insert (pf_address >> m_log2_block_size);
         }
      }
   }

   if (throttle_level_L1 < 2) {
      if ((BASE_PREFETCH_DEGREE_L1 > 1) && !stopPrefetching && (i<BASE_PREFETCH_DEGREE_L1+1)) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//            INJECT  MORE  PREFETCHES  BASED  ON  LAST  TWO  IDENTICAL  DELTAS,  IF  ANY              //
////////////////////////////////////////////////////////////////////////////////////////////////////////
         if ((ipPrefetchStride[i-1] != 0) && (i > 1) && (ipPrefetchStride[i-1] == ipPrefetchStride[i-2])) {
            assert(num_pref < BASE_PREFETCH_DEGREE_L1);

            while (1) {
               pf_address = pf_address + (ipPrefetchStride[i-1] << m_log2_block_size);
               if ((pf_address >> m_page_shift) != (addr >> m_page_shift)) break;

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    RECENT  ACCESS  TAG  ARRAY  LOOKUP  TO  DETERMINE  RECENT  ACCESS  TO  PREFETCH  CANDIDATE       //
////////////////////////////////////////////////////////////////////////////////////////////////////////
               if (!RecentAccessTagArrayL1DetermineHit(pf_address >> m_log2_block_size)) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//             MISS  IN  RECENT  ACCESS  TAG  ARRAY, INJECT  PREFETCH                                  //
////////////////////////////////////////////////////////////////////////////////////////////////////////
                  if (PQ.occupancy < (PQ.SIZE - 1)) {
		      m_controller->enqueuePrefetch(pf_address, type);
//                     assert(prefetch_line(ip, addr, pf_address, FILL_L1, 0));
                     num_pref++;
                     did_pref = true;
                  }
                  else if (PQ.occupancy < PQ.SIZE) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    HAND  OVER  REMAINING  PREFETCH  INJECTION  TO  L2  CACHE  BY  ENCODING  THE  METADATA           //
////////////////////////////////////////////////////////////////////////////////////////////////////////
                     uint32_t pfmetadata = 0x80000000U;
                     unsigned char delta = ((ipPrefetchStride[i-1] < 0) ? ((-ipPrefetchStride[i-1]) | (1 << (m_page_shift - m_log2_block_size))) : ipPrefetchStride[i-1]);
                     pfmetadata = pfmetadata | delta;
//                     assert(prefetch_line(ip, addr, pf_address, FILL_L1, pfmetadata));
		     m_controller->enqueuePrefetch(pf_address, type);
                     num_pref++;
                     did_pref = true;
                  }
                  else {
		      m_controller->enqueuePrefetch(pf_address, type);
//                     assert(!prefetch_line(ip, addr, pf_address, FILL_L1, 0));
                     break;
                  }
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//                         INSERT  IN  RECENT  ACCESS  TAG  ARRAY                                      //
////////////////////////////////////////////////////////////////////////////////////////////////////////
                  RecentAccessTagArrayL1Insert (pf_address >> m_log2_block_size);
               }

               if (num_pref == (BASE_PREFETCH_DEGREE_L1)) break;
            }
            if ((num_pref == (BASE_PREFETCH_DEGREE_L1)) && (PQ.occupancy < PQ.SIZE)) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    HAND  OVER  FURTHER  PREFETCH  INJECTION  TO  L2  CACHE  BY  ENCODING  THE  METADATA           //
////////////////////////////////////////////////////////////////////////////////////////////////////////
               pf_address = pf_address + (ipPrefetchStride[i-1] << m_log2_block_size);
               if ((pf_address >> m_page_shift) == (addr >> m_page_shift)) {
                  uint32_t pfmetadata = 0x80000000U;
                  unsigned char delta = ((ipPrefetchStride[i-1] < 0) ? ((-ipPrefetchStride[i-1]) | (1 << (m_page_shift - m_log2_block_size))) : ipPrefetchStride[i-1]);
                  pfmetadata = pfmetadata | delta;
		  m_controller->enqueuePrefetch(pf_address, type);
//                  assert(prefetch_line(ip, addr, pf_address, FILL_L1, pfmetadata));
                  did_pref = true;
               }
            }
         }
         else if (constantStrideValid) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//                   INJECT  MORE  PREFETCHES  BASED  ON  IP  STRIDE                                   //
////////////////////////////////////////////////////////////////////////////////////////////////////////
            while (1) {
               pf_address = pf_address + (constantStride << m_log2_block_size);
               if ((pf_address >> m_page_shift) != (addr >> m_page_shift)) break;

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    RECENT  ACCESS  TAG  ARRAY  LOOKUP  TO  DETERMINE  RECENT  ACCESS  TO  PREFETCH  CANDIDATE       //
////////////////////////////////////////////////////////////////////////////////////////////////////////
               if (!RecentAccessTagArrayL1DetermineHit(pf_address >> m_log2_block_size)) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//              MISS  IN  RECENT  ACCESS  TAG  ARRAY, INJECT  PREFETCH                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////
                  if (PQ.occupancy < (PQ.SIZE - 1)) {
//                     assert(prefetch_line(ip, addr, pf_address, FILL_L1, 0));
		      m_controller->enqueuePrefetch(pf_address, type);
                     num_pref++;
                     did_pref = true;
                  }
                  else if (PQ.occupancy < PQ.SIZE) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    HAND  OVER  REMAINING  PREFETCH  INJECTION  TO  L2  CACHE  BY  ENCODING  THE  METADATA           //
////////////////////////////////////////////////////////////////////////////////////////////////////////
                     uint32_t pfmetadata = 0x80000000U;
                     unsigned char delta = ((constantStride < 0) ? ((-constantStride) | (1 << (m_page_shift - m_log2_block_size))) : constantStride);
                     pfmetadata = pfmetadata | delta;
//                     assert(prefetch_line(ip, addr, pf_address, FILL_L1, pfmetadata));
		     m_controller->enqueuePrefetch(pf_address, type);
                     num_pref++;
                     did_pref = true;
                  }
                  else {
		      m_controller->enqueuePrefetch(pf_address, type);
//                     assert(!prefetch_line(ip, addr, pf_address, FILL_L1, 0));
                     break;
                  }
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//                       INSERT  IN  RECENT  ACCESS  TAG  ARRAY                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////
                  RecentAccessTagArrayL1Insert(pf_address >> m_log2_block_size);
               }

               if (num_pref == (BASE_PREFETCH_DEGREE_L1)) break;
            }
            if ((num_pref == (BASE_PREFETCH_DEGREE_L1)) && (PQ.occupancy < PQ.SIZE)) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    HAND  OVER  FURTHER  PREFETCH  INJECTION  TO  L2  CACHE  BY  ENCODING  THE  METADATA           //
////////////////////////////////////////////////////////////////////////////////////////////////////////
               pf_address = pf_address + (constantStride << m_log2_block_size);
               if ((pf_address >> m_page_shift) == (addr >> m_page_shift)) {
                  uint32_t pfmetadata = 0x80000000U;
                  unsigned char delta = ((constantStride < 0) ? ((-constantStride) | (1 << (m_page_shift - m_log2_block_size))) : constantStride);
                  pfmetadata = pfmetadata | delta;
		  m_controller->enqueuePrefetch(pf_address, type);
//                  assert(prefetch_line(ip, addr, pf_address, FILL_L1, pfmetadata));
                  did_pref = true;
               }
            }
         }
      }
      else if (!stopPrefetching) {
         if ((ipPrefetchStride[i-1] != 0) && (i > 1) && (ipPrefetchStride[i-1] == ipPrefetchStride[i-2])) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    HAND  OVER  REMAINING  PREFETCH  INJECTION  TO  L2  CACHE  BY  ENCODING  THE  METADATA           //
////////////////////////////////////////////////////////////////////////////////////////////////////////
            pf_address = pf_address + (ipPrefetchStride[i-1] << m_log2_block_size);
            if ((pf_address >> m_page_shift) == (addr >> m_page_shift)) {
               uint32_t pfmetadata = 0x80000000U;
               unsigned char delta = ((ipPrefetchStride[i-1] < 0) ? ((-ipPrefetchStride[i-1]) | (1 << (m_page_shift - m_log2_block_size))) : ipPrefetchStride[i-1]);
               pfmetadata = pfmetadata | delta;
	       m_controller->enqueuePrefetch(pf_address, type);
//               prefetch_line(ip, addr, pf_address, FILL_L1, pfmetadata);
               did_pref = true;
            }
         }
         else if (constantStrideValid) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    HAND  OVER  REMAINING  PREFETCH  INJECTION  TO  L2  CACHE  BY  ENCODING  THE  METADATA           //
////////////////////////////////////////////////////////////////////////////////////////////////////////
            pf_address = pf_address + (constantStride << m_log2_block_size);
            if ((pf_address >> m_page_shift) == (addr >> m_page_shift)) {
               uint32_t pfmetadata = 0x80000000U;
               unsigned char delta = ((constantStride < 0) ? ((-constantStride) | (1 << (m_page_shift - m_log2_block_size))) : constantStride);
               pfmetadata = pfmetadata | delta;
	       m_controller->enqueuePrefetch(pf_address, type);
//               prefetch_line(ip, addr, pf_address, FILL_L1, pfmetadata);
               did_pref = true;
            }
         }
      }
   }
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    INVOKE  LONG  HISTORY  IP  PREFETCHER  IF  NO  PREFETCH  YET                                     //
////////////////////////////////////////////////////////////////////////////////////////////////////////
   if (!did_pref && longHistIPTableNewDelta) {
      int j, length, chosen_j=-1;
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    DETERMINE STRIDE PATTERN MATCH WITH MATCH_LENGTH HISTORY                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////
      for (j=0; j<NUM_STRIDES_IN_LONG_HIST_IP_TABLE+1-m_long_hist_match_length; j++) {
         length = 0;
         while (length != m_long_hist_match_length) {
            if (longHistory[NUM_STRIDES_IN_LONG_HIST_IP_TABLE+1-m_long_hist_match_length+length] != longHistory[j+length]) break;
            length++;
	 }
         if (length == m_long_hist_match_length) {
	    assert(chosen_j == -1);
            chosen_j = j;
            break;
         }
      }
      if (chosen_j != -1) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    MATCHING LONGEST PATTERN DETERMINED                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////
         j = chosen_j + m_long_hist_match_length;
         if (throttle_level_L1 == 0) {
            while (j < NUM_STRIDES_IN_LONG_HIST_IP_TABLE+1) {
               uint64_t pf_address = (cl_addr + longHistory[j]) << m_log2_block_size;
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    RECENT  ACCESS  TAG  ARRAY  LOOKUP  TO  DETERMINE  RECENT  ACCESS  TO  PREFETCH  CANDIDATE       //
////////////////////////////////////////////////////////////////////////////////////////////////////////
               if (!RecentAccessTagArrayL1DetermineHit(pf_address >> m_log2_block_size)) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    MISS  IN  RECENT  ACCESS  TAG  ARRAY, INJECT  PREFETCH                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////
//                  if (prefetch_line(ip, addr, pf_address, FILL_L1, 0)) {
		   m_controller->enqueuePrefetch(pf_address, type);
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//                           INSERT  IN  RECENT  ACCESS  TAG  ARRAY                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////
		   RecentAccessTagArrayL1Insert(pf_address >> m_log2_block_size);
		   did_pref = true;
//                  }
               }
               j++;
            }
         }
      }
   }

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    INVOKE  NEXT  LINE  PREFETCHER  IF  NO  PREFETCH  YET                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////
   if (!did_pref) {
      uint64_t pf_address = (cl_addr+1) << m_log2_block_size;
      if (throttle_level_L1 == 0) {
         i = 0;
         while (i < BASE_PREFETCH_DEGREE_L1) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    INJECT  PREFETCHES  FOR  APPROPRIATE  DEGREE                                                     //
////////////////////////////////////////////////////////////////////////////////////////////////////////
            if (degreeHitsL1[i]*m_nl_threshold_denom_l1 > degreeInsertionsL1[i]*m_nl_threshold_numer_l1) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    RECENT  ACCESS  TAG  ARRAY  LOOKUP  TO  DETERMINE  RECENT  ACCESS  TO  PREFETCH  CANDIDATE       //
////////////////////////////////////////////////////////////////////////////////////////////////////////
               if (!RecentAccessTagArrayL1DetermineHit(pf_address >> m_log2_block_size)) {
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    MISS  IN  RECENT  ACCESS  TAG  ARRAY, INJECT  PREFETCH                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////
		   //                if (prefetch_line(ip, addr, pf_address, FILL_L1, 0)) {
		   m_controller->enqueuePrefetch(pf_address, type);
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//                           INSERT  IN  RECENT  ACCESS  TAG  ARRAY                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////
		   RecentAccessTagArrayL1Insert(pf_address >> m_log2_block_size);
//                  }
               }
            }
            pf_address = pf_address + (1 << m_log2_block_size);
            i++;
         }
      }
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//    INSERT  POSSIBLE  NEXT  LINE  PREFETCH  CANDIDATES  INTO  NL  BUFFER TO  MEASURE  USEFULNESS     //
////////////////////////////////////////////////////////////////////////////////////////////////////////
      i = 0;
      pf_address = (cl_addr+1) << m_log2_block_size;
      while (i < BASE_PREFETCH_DEGREE_L1) {
         if ((pf_address >> m_page_shift) != (addr >> m_page_shift)) break;
         NLBufferL1Insert(pf_address >> m_log2_block_size, i);
         pf_address = pf_address + (1 << m_log2_block_size);
         i++;
      }
   }

//This is just to avoid metadata not used
   DPRINTF(RubyPrefetcher, "Metadata %#x\n", metadata);

   SIG_DP(cout << metadata << endl);
   return;
}


} // namespace ruby
} // namespace gem5
