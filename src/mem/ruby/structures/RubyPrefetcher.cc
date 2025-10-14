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

#include "mem/ruby/structures/RubyPrefetcher.hh"

#include <cassert>
#include <cmath>

#include "base/bitfield.hh"
#include "debug/RubyPrefetcher.hh"
#include "mem/ruby/slicc_interface/RubySlicc_ComponentMapping.hh"
#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{

namespace ruby
{

/******************************************************************************/
/*                      Latency table functions                               */
/******************************************************************************/
uint8_t LatencyTable::add(uint64_t addr, uint64_t tag, bool pf, uint64_t cycle)
{
  /*
   * Save if possible the new miss into the pqmshr (latency) table
   *
   * Parameters:
   *  - addr: address without cache offset
   *  - access: is theh entry accessed by a demand request
   *  - cycle: time to use in the latency table
   *
   * Return: pf
   */

  latency_table *free;
  free   = nullptr;

  for (int i = 0; i < size; i++)
  {
    // Search if the addr already exists. If it exist we does not have
    // to do nothing more
    if (latencyt[i].addr == addr)
    {
      // latencyt[i].time = cycle;
      latencyt[i].pf   = pf;
      latencyt[i].tag  = tag;
      return latencyt[i].pf;
    }

    // We discover a free space into the latency table, save it for later
    if (latencyt[i].tag == 0) free = &latencyt[i];
  }

  if (free == nullptr) assert(0 && "No free space latency table");

  // We save the new entry into the latency table
  DPRINTF(RubyPrefetcher, "New entry LatencyTable %#x\n", addr);
  free->addr = addr;
  free->time = cycle;
  free->tag  = tag;
  free->pf   = pf;

  return free->pf;
}

uint64_t LatencyTable::del(uint64_t addr)
{
  /*
   * Remove the address from the latency table
   *
   * Parameters:
   *  - addr: address without cache offset
   *
   *  Return: the latency of the address
   */

  for (int i = 0; i < size; i++)
  {
    // Line already in the table
    if (latencyt[i].addr == addr)
    {
      // Calculate latency
      uint64_t time = latencyt[i].time;

      latencyt[i].addr = 0; // Free the entry
      latencyt[i].tag  = 0; // Free the entry
      latencyt[i].time = 0; // Free the entry
      latencyt[i].pf   = 0; // Free the entry
      DPRINTF(RubyPrefetcher, "Free entry LatencyTable %#x\n", addr);
      // Return the latency
      return time;
    }
  }

  // We should always track the misses
  return 0;
}

uint64_t LatencyTable::get(uint64_t addr)
{
  /*
   * Return time or 0 if the addr is or is not in the pqmshr (latency) table
   *
   * Parameters:
   *  - addr: address without cache offset
   *
   * Return: time if the line is in the latency table, otherwise 0
   */

  for (int i = 0; i < size; i++)
  {
    // Search if the addr already exists
    if (latencyt[i].addr == addr)
    {
      return latencyt[i].time;
    }
  }

  return 0;
}

uint64_t LatencyTable::get_tag(uint64_t addr)
{
  /*
   * Return IP-Tag or 0 if the addr is or is not in the pqmshr (latency) table
   *
   * Parameters:
   *  - addr: address without cache offset
   *
   * Return: ip-tag if the line is in the latency table, otherwise 0
   */

  for (int i = 0; i < size; i++)
  {
    if (latencyt[i].addr == addr && latencyt[i].tag) // This is the address
    {
      return latencyt[i].tag;
    }
  }

  return 0;
}

/******************************************************************************/
/*                       Shadow Cache functions                               */
/******************************************************************************/
bool ShadowCache::add(uint32_t set, uint32_t way, uint64_t addr, bool pf, uint64_t lat)
{
  /*
   * Add block to shadow cache
   *
   * Parameters:
   *      - cpu: cpu
   *      - set: cache set
   *      - way: cache way
   *      - addr: cache block v_addr
   *      - access: the cache is access by a demand
   */
  assert(set < sets);
  assert(way < ways);
//  assert(lat >= 0);

  scache[set][way].addr = addr;
  scache[set][way].pf   = pf;
  scache[set][way].lat  = lat;
  return scache[set][way].pf;
}

bool ShadowCache::get(uint64_t addr)
{
  /*
   * Parameters:
   *      - addr: cache block v_addr
   *
   * Return: true if the addr is in the l1d cache, false otherwise
   */

  for (uint32_t i = 0; i < sets; i++)
  {
    for (uint32_t ii = 0; ii < ways; ii++)
    {
      if (scache[i][ii].addr == addr)
      {
        return true;
      }
    }
  }

  return false;
}

void ShadowCache::set_pf(uint64_t addr, bool pf)
{
  /*
   * Parameters:
   *      - addr: cache block v_addr
   *
   * Return: change value of pf field
   */
  for (int i = 0; i < sets; i++)
  {
    for (int ii = 0; ii < ways; ii++)
    {
      if (scache[i][ii].addr == addr)
      {
        scache[i][ii].pf = pf;
        return;
      }
    }
  }

  // The address should always be in the cache
  assert((0) && "Address is must be in shadow cache");
}

bool ShadowCache::is_pf(uint64_t addr)
{
  /*
   * Parameters:
   *      - addr: cache block v_addr
   *
   * Return: True if the saved one is a prefetch
   */

  for (int i = 0; i < sets; i++)
  {
    for (int ii = 0; ii < ways; ii++)
    {
      if (scache[i][ii].addr == addr)
      {
        return scache[i][ii].pf;
      }
    }
  }

  assert((0) && "Address is must be in shadow cache");
  return 0;
}

uint64_t ShadowCache::get_latency(uint64_t addr)
{
  /*
   * Init shadow cache
   *
   * Parameters:
   *      - addr: cache block v_addr
   *
   * Return: the saved latency
   */
  for (int i = 0; i < sets; i++)
  {
    for (int ii = 0; ii < ways; ii++)
    {
      if (scache[i][ii].addr == addr)
      {
        return scache[i][ii].lat;
      }
    }
  }

  assert((0) && "Address is must be in shadow cache");
  return 0;
}

/******************************************************************************/
/*                       History Table functions                               */
/******************************************************************************/
void HistoryTable::add(uint64_t tag, uint64_t addr, uint64_t cycle)
{
  /*
   * Save the new information into the history table
   *
   * Parameters:
   *  - tag: PC tag
   *  - addr: addr access
   */
  uint16_t set = tag & rp_instance.getTableSetMask();
  assert(set < sets);

  // If the latest entry is the same, we do not add it
  if (history_pointers[set] == &historyt[set][ways - 1])
  {
    if (historyt[set][0].addr == (addr & rp_instance.getAddrMask())) return;
  } else if ((history_pointers[set] - 1)->addr == (addr & rp_instance.getAddrMask())) return;

  // Save new element into the history table
  history_pointers[set]->tag       = tag;
  history_pointers[set]->time      = cycle & rp_instance.getTimeMask();
  history_pointers[set]->addr      = addr & rp_instance.getAddrMask();

  if (history_pointers[set] == &historyt[set][ways - 1])
  {
    history_pointers[set] = &historyt[set][0]; // End the cycle
  } else history_pointers[set]++; // Pointer to the next (oldest) entry
}

uint16_t HistoryTable::get_aux(uint32_t latency,
    uint64_t tag, uint64_t act_addr, std::vector<uint64_t>& tags, std::vector<uint64_t>& addr,
    uint64_t cycle)
{
  /*
   * Return an array (by parameter) with all the possible PC that can launch
   * an on-time and late prefetch
   *
   * Parameters:
   *  - tag: PC tag
   *  - latency: latency of the processor
   */

  uint16_t num_on_time = 0;
  uint16_t set = tag & rp_instance.getTableSetMask();

  assert(set < sets);

  // This is the begin of the simulation
  if (cycle < latency) return num_on_time;

  // The IPs that is launch in this cycle will be able to launch this prefetch
  cycle -= latency;

  // Pointer to guide
  history_table *pointer = history_pointers[set];

  do
  {
    // Look for the IPs that can launch this prefetch
    if (pointer->tag == tag && pointer->time <= cycle)
    {
      // Test that addr is not duplicated
      if (pointer->addr == act_addr) return num_on_time;

      assert(num_on_time < tags.size());
      assert(num_on_time < addr.size());

      // This IP can launch the prefetch
      tags[num_on_time] = pointer->tag;
      addr[num_on_time] = pointer->addr;
      num_on_time++;
    }

    if (pointer == historyt[set])
    {
      // We get at the end of the history, we start again
      pointer = &historyt[set][ways - 1];
    } else pointer--;
  } while (pointer != history_pointers[set]);

  return num_on_time;
}

uint16_t HistoryTable::get(uint32_t latency, uint64_t tag, uint64_t act_addr,
    std::vector<uint64_t>& tags, std::vector<uint64_t>& addr, uint64_t cycle)
{
  /*
   * Return an array (by parameter) with all the possible PC that can launch
   * an on-time and late prefetch
   *
   * Parameters:
   *  - tag: PC tag
   *  - latency: latency of the processor
   *  - on_time_ip (out): ips that can launch an on-time prefetch
   *  - on_time_addr (out): addr that can launch an on-time prefetch
   *  - num_on_time (out): number of ips that can launch an on-time prefetch
   */

  act_addr &= rp_instance.getAddrMask();

  uint16_t num_on_time = get_aux(latency, tag, act_addr, tags, addr, cycle & rp_instance.getTimeMask());

  // We found on-time prefetchs
  return num_on_time;
}

/******************************************************************************/
/*                        Berti table functions                               */
/******************************************************************************/

uint64_t Berti::berti_r;
uint64_t Berti::berti_l1;
uint64_t Berti::berti_l2;
uint64_t Berti::berti_i;

Berti::Berti(uint64_t p_size, RubyPrefetcher& rp) : size(p_size),
                                                    rp_instance(rp)
{
  berti_r = rp.getBertiR();
  berti_l1 = rp.getBertiL1();
  berti_l2 = rp.getBertiL2();
  berti_i = rp.getBertiI();
};

void Berti::increase_conf_tag(uint64_t tag)
{
  /*
   * Increase the global confidence of the deltas associated to the tag
   *
   * Parameters:
   *  tag : tag to find
   */
  if (bertit.find(tag) == bertit.end())
  {
    // Tag not found
    return;
  }

  // Get the entries and the deltas

  bertit[tag]->conf += rp_instance.getConfidenceInc();

  if (bertit[tag]->conf == rp_instance.getConfidenceMax())
  {

    // Max confidence achieve
    for (auto &i: bertit[tag]->deltas)
    {
      // Set bits to prefetch level
      if (i.conf > rp_instance.getConfidenceL1())i.rpl = rp_instance.getBertiL1();
      else if (i.conf > rp_instance.getConfidenceL2()) i.rpl = rp_instance.getBertiL2();
      else i.rpl = rp_instance.getBertiR();

      i.conf = 0; // Reset confidence
    }

    bertit[tag]->conf = 0; // Reset global confidence
  }
}

void Berti::add(uint64_t tag, int64_t delta)
{
  /*
   * Save the new information into the history table
   *
   * Parameters:
   *  - tag: PC tag
   *  - cpu: actual cpu
   *  - stride: actual cpu
   */
  auto add_delta = [this](auto delta, auto entry)
  {
    // Lambda function to add a new element
    delta_t new_delta;
    new_delta.delta = delta;
    new_delta.conf = rp_instance.getConfidenceInit();
    new_delta.rpl = rp_instance.getBertiI(); // Initial replacement level
    auto it = std::find_if(std::begin(entry->deltas), std::end(entry->deltas), [](const auto i){
      return (i.delta == 0);
    });
    assert(it != std::end(entry->deltas));
    *it = new_delta;
  };

  if (bertit.find(tag) == bertit.end())
  {

    // We are not tracking this tag
    if (bertit_queue.size() > rp_instance.getBertiTableSize())
    {
      // FIFO replacent algorithm
      uint64_t key = bertit_queue.front();
      berti *entry = bertit[key];

      delete entry; // Free previous entry

      bertit.erase(bertit_queue.front());
      bertit_queue.pop();
    }

    bertit_queue.push(tag); // Add new tag
    assert((bertit.size() <= rp_instance.getBertiTableSize()) && "Tracking too much tags");

    // Confidence IP
    berti *entry = new berti(rp_instance.getBertiTableDeltaSize());
    entry->conf = rp_instance.getConfidenceInc();

    // Saving the new stride
    add_delta(delta, entry);

    // Save the new tag
    bertit.insert(std::make_pair(tag, entry));
    return;
  }

  // Get the delta
  berti *entry  = bertit[tag];

  for (auto &i: entry->deltas)
  {
    if (i.delta == delta)
    {
      // We already track the delta
      i.conf += rp_instance.getConfidenceInc();

      if (i.conf > rp_instance.getConfidenceMax()) i.conf = rp_instance.getConfidenceMax();

      return;
    }
  }

  // We have space to add a new entry
  auto ssize = std::count_if(std::begin(entry->deltas), std::end(entry->deltas),[](const auto i){
    return i.delta != 0;
  });

  if (ssize < size)
  {
    add_delta(delta, entry);
    assert((std::size(entry->deltas) <= size) && "I remember too much deltas");
    return;
  }

  // We find the delta with less confidence
  std::sort(std::begin(entry->deltas), std::end(entry->deltas), compare_rpl);
  if (entry->deltas.front().rpl == rp_instance.getBertiR() || entry->deltas.front().rpl == rp_instance.getBertiL2())
  {
    entry->deltas.front().delta = delta;
    entry->deltas.front().conf = rp_instance.getConfidenceInit();
    entry->deltas.front().rpl = rp_instance.getBertiI(); // Initial replacement level
  }
}

uint8_t Berti::get(uint64_t tag, std::vector<delta_t> &res)
{
  /*
   * Save the new information into the history table
   *
   * Parameters:
   *  - tag: PC tag
   *
   * Return: the stride to prefetch
   */
  if (!bertit.count(tag))
  {
    return 0;
  }

  // We found the tag
  berti *entry = bertit[tag];

  // 1) Collect deltas already assigned to a prefetch replacement level and
  // optionally promote deltas that are still at the initial replacement
  // level. We only attempt promotion when the tag's global confidence is at
  // least 4 (to avoid noisy estimates). For deltas at getBertiI(), estimate
  // per-delta confidence as (i.conf * 100) / entry->conf and compare the
  // resulting integer percent against getConfidenceI() which is expected to
  // be in the 0-100 range. If pct > getConfidenceI(), promote to L2.
  for (const auto &i : entry->deltas) {
    if (i.delta == 0) continue;

    // Keep deltas already assigned to a replacement level (not the R level)
    if (i.rpl != rp_instance.getBertiR() || i.rpl != rp_instance.getBertiI()) {
      res.push_back(i);
      continue;
    }

    // Consider promotion for deltas at the initial replacement level
    if (i.rpl == rp_instance.getBertiI() && entry->conf >= rp_instance.getLaunchMiddleConf()) {
      // compute integer percent (0-100)
      int pct = static_cast<int>((i.conf * 100) / entry->conf);
      if (pct >= static_cast<int>(rp_instance.getConfidenceI())) {
        delta_t new_delta;
        new_delta.delta = i.delta;
        new_delta.rpl = rp_instance.getBertiL2();
        res.push_back(new_delta);
      }
    }
  }

  // TODO Test enqueueing additional deltas if we have space in res vector.

  // 2) If none found, consider promoting deltas based on estimated accuracy.
  if (res.empty() && entry->conf >= rp_instance.getLaunchMiddleConf()) {
    // Defensive: ensure we won't divide by zero (entry->conf should be >= launch
    // threshold, but check anyway).
    if (entry->conf > 0) {
      // Interpret per-delta accuracy as an integer percentage (0-100) and
      // compare directly against integer configuration parameters.
      for (const auto &i : entry->deltas) {
        if (i.delta == 0)
          continue;

        // integer accuracy percent on 0-100 scale (floor division)
        int accuracy_percent = static_cast<int>((i.conf * 100) / entry->conf);

        if (accuracy_percent >= rp_instance.getConfidenceMiddleL1()) {
          delta_t new_delta;
          new_delta.delta = i.delta;
          new_delta.rpl = rp_instance.getBertiL1();
          res.push_back(new_delta);
        } else if (accuracy_percent >= rp_instance.getConfidenceMiddleL2()) {
          delta_t new_delta;
          new_delta.delta = i.delta;
          new_delta.rpl = rp_instance.getBertiL2();
          res.push_back(new_delta);
        } else {
          continue;
        }
      }
    }
  }

  // Sort the entries
  std::sort(std::begin(res), std::end(res), compare_greater_delta);
  return 1;
}

void Berti::find_and_update(uint64_t latency, uint64_t tag, uint64_t cycle,
			    uint64_t line_addr, HistoryTable *historyt)
{
  // We were tracking this miss
  std::vector<uint64_t> tags(rp_instance.getHistoryTableWays());
  std::vector<uint64_t> addr(rp_instance.getHistoryTableWays());
  uint16_t num_on_time = 0;

  // Get the IPs that can launch a prefetch
  num_on_time = historyt->get(latency, tag, line_addr, tags, addr, cycle);

  for (uint32_t i = 0; i < num_on_time; i++)
  {
    // Increase conf tag
    if (i == 0) increase_conf_tag(tag);

    // Add information into berti table
    int64_t stride;
    line_addr &= rp_instance.getAddrMask();

    // Usually applications go from lower to higher memory position.
    // The operation order is important (mainly because we allow
    // negative strides)
    stride = (int64_t) (line_addr - addr[i]);

    if ((std::abs(stride) < (1 << rp_instance.getDeltaMask()))) add(tags[i], stride);
  }
}

bool Berti::compare_rpl(delta_t a, delta_t b)
{
  if (a.rpl == berti_r && b.rpl != berti_r) return 1;
  else if (b.rpl == berti_r && a.rpl != berti_r) return 0;
  else if (a.rpl == berti_i && b.rpl != berti_i) return 1;
  else if (b.rpl == berti_i && a.rpl != berti_i) return 0;
  else
  {
    if (a.conf < b.conf) return 1;
    else return 0;
  }
}

bool Berti::compare_greater_delta(delta_t a, delta_t b)
{
  // Sorted stride when the confidence is full
  if (a.rpl == berti_l1 && b.rpl != berti_l1) return 1;
  else if (a.rpl != berti_l1 && b.rpl == berti_l1) return 0;
  else
  {
    if (a.rpl == berti_l2 && b.rpl != berti_l2) return 1;
    else if (a.rpl != berti_l2 && b.rpl == berti_l2) return 0;
    else
    {
      if (a.rpl == berti_i && b.rpl != berti_i) return 1;
      if (a.rpl != berti_i && b.rpl == berti_i) return 0;
      else
      {
        if (std::abs(a.delta) < std::abs(b.delta)) return 1;
        return 0;
      }
    }
  }
}

uint64_t Berti::ip_hash(uint64_t ip)
{
  /*
   * IP hash function
   */
#ifdef HASH_ORIGINAL
  ip = ((ip >> 1) ^ (ip >> 4)); // Original one
#endif
  // IP hash from here: http://burtleburtle.net/bob/hash/integer.html
#ifdef THOMAS_WANG_HASH_1
  ip = (ip ^ 61) ^ (ip >> 16);
  ip = ip + (ip << 3);
  ip = ip ^ (ip >> 4);
  ip = ip * 0x27d4eb2d;
  ip = ip ^ (ip >> 15);
#endif
#ifdef THOMAS_WANG_HASH_2
  ip = (ip+0x7ed55d16) + (ip<<12);
  ip = (ip^0xc761c23c) ^ (ip>>19);
  ip = (ip+0x165667b1) + (ip<<5);
  ip = (ip+0xd3a2646c) ^ (ip<<9);
  ip = (ip+0xfd7046c5) + (ip<<3);
  ip = (ip^0xb55a4f09) ^ (ip>>16);
#endif
#ifdef THOMAS_WANG_HASH_3
  ip -= (ip<<6);
  ip ^= (ip>>17);
  ip -= (ip<<9);
  ip ^= (ip<<4);
  ip -= (ip<<3);
  ip ^= (ip<<10);
  ip ^= (ip>>15);
#endif
#ifdef THOMAS_WANG_HASH_4
  ip += ~(ip<<15);
  ip ^=  (ip>>10);
  ip +=  (ip<<3);
  ip ^=  (ip>>6);
  ip += ~(ip<<11);
  ip ^=  (ip>>16);
#endif
#ifdef THOMAS_WANG_HASH_5
  ip = (ip+0x479ab41d) + (ip<<8);
  ip = (ip^0xe4aa10ce) ^ (ip>>5);
  ip = (ip+0x9942f0a6) - (ip<<14);
  ip = (ip^0x5aedd67d) ^ (ip>>3);
  ip = (ip+0x17bea992) + (ip<<7);
#endif
#ifdef THOMAS_WANG_HASH_6
  ip = (ip^0xdeadbeef) + (ip<<4);
  ip = ip ^ (ip>>10);
  ip = ip + (ip<<7);
  ip = ip ^ (ip>>13);
#endif
#ifdef THOMAS_WANG_HASH_7
  ip = ip ^ (ip>>4);
  ip = (ip^0xdeadbeef) + (ip<<5);
  ip = ip ^ (ip>>11);
#endif
#ifdef THOMAS_WANG_NEW_HASH
  ip ^= (ip >> 20) ^ (ip >> 12);
  ip = ip ^ (ip >> 7) ^ (ip >> 4);
#endif
#ifdef THOMAS_WANG_HASH_HALF_AVALANCHE
  ip = (ip+0x479ab41d) + (ip<<8);
  ip = (ip^0xe4aa10ce) ^ (ip>>5);
  ip = (ip+0x9942f0a6) - (ip<<14);
  ip = (ip^0x5aedd67d) ^ (ip>>3);
  ip = (ip+0x17bea992) + (ip<<7);
#endif
#ifdef THOMAS_WANG_HASH_FULL_AVALANCHE
  ip = (ip+0x7ed55d16) + (ip<<12);
  ip = (ip^0xc761c23c) ^ (ip>>19);
  ip = (ip+0x165667b1) + (ip<<5);
  ip = (ip+0xd3a2646c) ^ (ip<<9);
  ip = (ip+0xfd7046c5) + (ip<<3);
  ip = (ip^0xb55a4f09) ^ (ip>>16);
#endif
#ifdef THOMAS_WANG_HASH_INT_1
  ip -= (ip<<6);
  ip ^= (ip>>17);
  ip -= (ip<<9);
  ip ^= (ip<<4);
  ip -= (ip<<3);
  ip ^= (ip<<10);
  ip ^= (ip>>15);
#endif
#ifdef THOMAS_WANG_HASH_INT_2
  ip += ~(ip<<15);
  ip ^=  (ip>>10);
  ip +=  (ip<<3);
  ip ^=  (ip>>6);
  ip += ~(ip<<11);
  ip ^=  (ip>>16);
#endif
#ifdef ENTANGLING_HASH
  ip = ip ^ (ip >> 2) ^ (ip >> 5);
#endif
#ifdef FOLD_HASH
  uint64_t hash = 0;
  while(ip) {hash ^= (ip & ip_mask); ip >>= size_ip_mask;}
  ip = hash;
#endif
  return ip; // No IP hash
}



RubyPrefetcher::RubyPrefetcher(const Params &p)
    : SimObject(p),
      berti_table_size(p.berti_table_size),
      berti_table_delta_size(p.berti_table_delta_size),
      history_table_sets(p.history_table_sets),
      history_table_ways(p.history_table_ways),
      size_ip_mask(p.size_ip_mask),
      ip_mask(p.ip_mask),
      time_mask(p.time_mask),
      lat_mask(p.lat_mask),
      addr_mask(p.addr_mask),
      delta_mask(p.delta_mask),
      table_set_mask(p.table_set_mask),
      confidence_max(p.confidence_max),
      confidence_inc(p.confidence_inc),
      confidence_init(p.confidence_init),
      confidence_l1(p.confidence_l1),
      confidence_l2(p.confidence_l2),
      confidence_i(p.confidence_i),
      confidence_middle_l1(p.confidence_middle_l1),
      confidence_middle_l2(p.confidence_middle_l2),
      launch_middle_conf(p.launch_middle_conf),
      max_history_ip(p.max_history_ip),
      mshr_limit(p.mshr_limit),
      berti_r(p.berti_r),
      berti_l1(p.berti_l1),
      berti_l2(p.berti_l2),
      berti_i(p.berti_i),
      page_shift(p.page_shift),
      latency_table_size(p.latency_table_size),
      l0_sets(p.l0_sets),
      l0_ways(p.l0_ways),
      rubyPrefetcherStats(this)
{
  // Calculate latency table size
  // JMCG: We put everything in thie variable from now on since it is not important
/*
  uint64_t latency_table_size = total_mshrs;
  for (auto const &i : get_rq_size()) latency_table_size += i; // What are these
  for (auto const &i : get_wq_size()) latency_table_size += i; // What are these
  for (auto const &i : get_pq_size()) latency_table_size += i; // What are these
  */

  latencyt = new LatencyTable(p.latency_table_size,*this);
  scache = new ShadowCache(p.l0_sets, p.l0_ways,*this);
  historyt = new HistoryTable(p.history_table_sets,p.history_table_ways,*this);
  berti = new Berti(p.berti_table_delta_size,*this);

  // Initialize MSHR load logging
  mshr_load_sampled_log = simout.create(
      "mshr_load_sampled.csv", false, false);
  mshr_load_averaged_log = simout.create(
      "mshr_load_averaged.csv", false, false);
  mshr_load_histogram_log = simout.create(
      "mshr_load_histogram.csv", false, false);
  // Prefetch logging: cycle, total_prefetches, target_level (L1/L2)
  prefetch_log = simout.create("prefetch_log.csv", false, false);

  if (mshr_load_sampled_log && mshr_load_sampled_log->stream()) {
    *mshr_load_sampled_log->stream() << "cycle,mshr_load" << std::endl;
  }
  if (mshr_load_averaged_log && mshr_load_averaged_log->stream()) {
    *mshr_load_averaged_log->stream() << "cycle,avg_mshr_load" << std::endl;
  }
  if (mshr_load_histogram_log && mshr_load_histogram_log->stream()) {
    *mshr_load_histogram_log->stream() << "mshr_load_value,count" << std::endl;
  }
  if (prefetch_log && prefetch_log->stream()) {
    *prefetch_log->stream() << "cycle,total_prefetches,target_level" << std::endl;
  }
}

RubyPrefetcher::~RubyPrefetcher() {
  // Close log files
  if (mshr_load_sampled_log) {
    simout.close(mshr_load_sampled_log);
  }
  if (mshr_load_averaged_log) {
    simout.close(mshr_load_averaged_log);
  }
  if (mshr_load_histogram_log) {
    simout.close(mshr_load_histogram_log);
  }
  if (prefetch_log) {
    simout.close(prefetch_log);
  }
}

RubyPrefetcher::
RubyPrefetcherStats::RubyPrefetcherStats(statistics::Group *parent)
    : statistics::Group(parent, "RubyPrefetcher"),
      ADD_STAT(welford_average, "Average latency (Welford)"),
      ADD_STAT(welford_average_L1, "Average latency for fills from L1 (Welford)"),
      ADD_STAT(welford_average_L2, "Average latency for fills from L2 (Welford)"),
      ADD_STAT(welford_average_dir, "Average latency for fills from directory (Welford)"),
      ADD_STAT(welford_average_other, "Average latency for fills from other (Welford)"),
      ADD_STAT(welford_num, "Fills where latency can be tracked (Welford) "),
      ADD_STAT(welford_num_L1, "Fills from L1 where latency can be tracked (Welford) "),
      ADD_STAT(welford_num_L2, "Fills from L2 where latency can be tracked (Welford) "),
      ADD_STAT(welford_num_dir, "Fills from directory where latency can be tracked (Welford) "),
      ADD_STAT(welford_num_other, "Fills from other where latency can be tracked (Welford) "),
      ADD_STAT(pf_to_l1, "Prefetches to L0"),
      ADD_STAT(pf_to_l2, "Prefetches to L1"),
      ADD_STAT(pf_to_l2_bc_mshr, "Prefetches to L1 because of MSHR"),
      ADD_STAT(no_pf_bc_mshr, "No prefetch because of MSHR load"),
      ADD_STAT(cant_track_latency, "Prefetches where latency cannot be tracked"),
      ADD_STAT(cross_page, "Berti crossed a memory page"),
      ADD_STAT(no_cross_page, "Berti did not cross a memory page"),
      ADD_STAT(no_found_berti, "Addresses not found in Berti"),
      ADD_STAT(found_berti, "Addresses not found in Berti"),
      ADD_STAT(average_issued, "Issued (to compute average)"),
      ADD_STAT(average_num, "Num (to compute average) "),
      ADD_STAT(total_misses, "Cache Misses seen by Berti"),
      ADD_STAT(total_hits, "Cache Hits seen by Berti"),
      ADD_STAT(pf_hits, "Cache hits to lines prefetched by Berti"),
      ADD_STAT(late_pf, "Cache misses to lines prefetched by Berti"),
      ADD_STAT(total_acceses, "Total calls to observeXX in Berti")
{
//  auto average = ((1.0*average_issued)/average_num);
//  ADD_STAT(average, "Average issued");
}


void RubyPrefetcher::prefetcher_cache_operate(Addr addr, Addr ip, bool cache_hit,
				      bool useful_prefetch, const RubyRequestType& type)
{
  uint64_t line_addr = (((uint64_t)addr) >> RubySystem::getBlockSizeBits());

  if (line_addr == 0) return;
  rubyPrefetcherStats.total_acceses++;

  // MSHR load logging - histogram tracking and EMA calculation
  static uint64_t event_counter = 0;
  event_counter++;

  // Update histogram - count occurrences of each MSHR load value
  if (mshr_load >= 0 && mshr_load <= 100) {
    mshr_load_histogram[mshr_load]++;
  }

  // Calculate exponential moving average (EMA) for MSHR load
  // EMA = alpha * current_value + (1-alpha) * previous_ema
  // Using alpha = 2/(N+1) where N=1000 for EMA over ~1000 samples
  double alpha = 2.0 / (LOG_WINDOW + 1);
  if (!ema_initialized) {
    mshr_load_ema = mshr_load;
    ema_initialized = true;
  } else {
    mshr_load_ema = alpha * mshr_load + (1.0 - alpha) * mshr_load_ema;
  }

  // Log every 1000th event
  if (event_counter % LOG_WINDOW == 0) {
    // Sampled logging - log the current instantaneous MSHR load
    if (mshr_load_sampled_log && mshr_load_sampled_log->stream()) {
      *mshr_load_sampled_log->stream() << m_controller->curCycle() << ","
          << mshr_load << std::endl;
    }

    // EMA logging - log the exponential moving average MSHR load
    if (mshr_load_averaged_log && mshr_load_averaged_log->stream()) {
      *mshr_load_averaged_log->stream() << m_controller->curCycle() << ","
          << static_cast<uint64_t>(std::round(mshr_load_ema)) << std::endl;
    }

    // Histogram logging - log histogram counts
    if (mshr_load_histogram_log && mshr_load_histogram_log->stream()) {
      for (int i = 0; i <= 100; i++) {
        if (mshr_load_histogram[i] > 0) {
          *mshr_load_histogram_log->stream() << i << "," <<
              mshr_load_histogram[i] << std::endl;
        }
      }
      // Add separator between logging windows
      *mshr_load_histogram_log->stream() << "---," <<
          m_controller->curCycle() << std::endl;
    }
    // Flush the prefetch_log as it doesn't flush per line
    if (prefetch_log && prefetch_log->stream())
      prefetch_log->stream()->flush();
  }

  uint64_t ip_hash = berti->ip_hash(ip) & ip_mask;

  if (!cache_hit) // This is a miss
  {
    latencyt->add(line_addr, ip_hash, false, m_controller->curCycle()); // Add @ to latency
    historyt->add(ip_hash, line_addr, m_controller->curCycle()); // Add to the table
  } else if (cache_hit && scache->is_pf(line_addr)) // Hit bc prefetch
  {

    scache->set_pf(line_addr, false);

    uint64_t latency = scache->get_latency(line_addr); // Get latency

    if (latency > lat_mask) latency = 0;

    berti->find_and_update(latency, ip_hash, m_controller->curCycle() & time_mask, line_addr, historyt);
    historyt->add(ip_hash, line_addr, m_controller->curCycle() & time_mask);
  } else
  {
  }

  std::vector<delta_t> deltas;

  auto berti_result = berti->get(ip_hash, deltas);

  if (berti_result == 0)
    rubyPrefetcherStats.no_found_berti++;
  else
    rubyPrefetcherStats.found_berti++;

  bool first_issue = true;
  for (auto i: deltas)
  {
    uint64_t p_addr = (line_addr + i.delta) << RubySystem::getBlockSizeBits();
    uint64_t p_b_addr = (p_addr >> RubySystem::getBlockSizeBits());

    if (latencyt->get(p_b_addr)) {
        continue; // We do not prefetch if it is already in flight
    }
    if (i.rpl == berti_r) {
        continue; // This should not happen, but just in case
    }
    if (mshr_load >= mshr_limit) {
      rubyPrefetcherStats.no_pf_bc_mshr++;
      return; // We can skip all deltas as the mshr load is too high
    }
    if (p_addr == 0) {
        continue;
    }
    if ((p_addr >> page_shift) != (addr >> page_shift))
    {
      rubyPrefetcherStats.cross_page++;
# ifdef NO_CROSS_PAGE
      // We do not cross virtual page
      continue;
# endif
    } else rubyPrefetcherStats.no_cross_page++;

    if (i.rpl == berti_l1) {
      rubyPrefetcherStats.pf_to_l1++;
    } else {
      rubyPrefetcherStats.pf_to_l2++;
    }

    DPRINTF(RubyPrefetcher, "Enqueue PF %#x\n", p_addr);
    m_controller->enqueuePrefetch(p_addr, type);
    // Log prefetch event: cycle, total prefetches so far, and target level (L1/L2)
    if (prefetch_log && prefetch_log->stream()) {
      const char *target = (i.rpl == berti_l1) ? "L1" : "L2";
      *prefetch_log->stream() << m_controller->curCycle() << ","
        << static_cast<uint64_t>(rubyPrefetcherStats.average_issued.value()) << ","
        << target << '\n';
    }
    ++rubyPrefetcherStats.average_issued;
    if (first_issue) {
      first_issue = false;
      ++rubyPrefetcherStats.average_num;
    }

    // Always update the latency table if needed
    if (!scache->get(p_b_addr))
    {
      latencyt->add(p_b_addr, ip_hash, true, m_controller->curCycle());
    }
  }

  return;
}

void RubyPrefetcher::prefetcher_cache_fill(Addr addr, bool prefetch, int set, int way, MachineID source)
{
  uint64_t line_addr = ((uint64_t)addr) >> RubySystem::getBlockSizeBits();
  uint64_t tag     = latencyt->get_tag(line_addr);
  uint64_t cycle   = latencyt->del(line_addr) & time_mask;
  uint64_t latency = 0;

  MachineType src_mt = source.getType();
  std::string srcName = MachineType_to_string(src_mt);
  DPRINTF(RubyPrefetcher,
    "Observed fill for 0x%llx from %s (prefetched=%d, set=%d, way=%d)\n",
    (unsigned long long)line_addr, srcName.c_str(), prefetch ? 1 : 0, set, way);

//  uint64_t evicted_addr = last_replaced_addr;
  last_replaced_addr = 0;

  if (cycle != 0 && ((m_controller->curCycle() & time_mask) > cycle))
    latency = (m_controller->curCycle() & time_mask) - cycle;

  if (latency > lat_mask)
  {
    latency = 0;
    rubyPrefetcherStats.cant_track_latency++;
  } else
  {
    if (latency != 0)
    {
      // Update overall and per-source-level Welford averages/counts.
      // Select per-level references based on the source machine type so
      // the same update code can be applied to both overall and level.
      statistics::Scalar &overall_avg = rubyPrefetcherStats.welford_average;
      statistics::Scalar &overall_num = rubyPrefetcherStats.welford_num;

      // Default pointers (set to directory by default)
      statistics::Scalar *level_avg_ptr = &rubyPrefetcherStats.welford_average_dir;
      statistics::Scalar *level_num_ptr = &rubyPrefetcherStats.welford_num_dir;

      switch (src_mt) {
        case MachineType_L1Cache:
          level_avg_ptr = &rubyPrefetcherStats.welford_average_L1;
          level_num_ptr = &rubyPrefetcherStats.welford_num_L1;
          break;
        case MachineType_L2Cache:
          level_avg_ptr = &rubyPrefetcherStats.welford_average_L2;
          level_num_ptr = &rubyPrefetcherStats.welford_num_L2;
          break;
        case MachineType_Directory:
          level_avg_ptr = &rubyPrefetcherStats.welford_average_dir;
          level_num_ptr = &rubyPrefetcherStats.welford_num_dir;
          break;
        default:
          // Untracked machine type - warn and route to directory bucket
          DPRINTF(RubyPrefetcher, "Untracked MachineType %s for fill latency %llu\n",
                  srcName.c_str(), (unsigned long long)latency);
          level_avg_ptr = &rubyPrefetcherStats.welford_average_other;
          level_num_ptr = &rubyPrefetcherStats.welford_num_other;
          break;
      }

      auto update_welford = [&](statistics::Scalar &avg, statistics::Scalar &num) {
        if (num.value() == 0) {
          avg = (double) latency;
        } else {
          double temp = ((((double) latency) - (double)avg.value()) / (uint64_t)num.value());
          avg = (double)avg.value() + temp;
        }
        num++;
      };

      // Update overall
      update_welford(overall_avg, overall_num);
      // Update per-level
      update_welford(*level_avg_ptr, *level_num_ptr);
    }
  }

  // Add to the shadow cache
//  uint32_t set = 0, way = 0;
//  scache->get((uint64_t)evicted_addr,&set,&way);
  DPRINTF(RubyPrefetcher, "Adding to shadow cache %#x to set %d way %d\n", line_addr, set, way);
  scache->add(set, way, line_addr, prefetch, latency);

  if (latency != 0 && !prefetch)
  {
    berti->find_and_update(latency, tag, cycle, line_addr, historyt);
  }

  return;
}

void
RubyPrefetcher::observeMiss(Addr address, const RubyRequestType& type, Addr pc)
{
    DPRINTF(RubyPrefetcher, "Observed miss for %#x\n", address);

    uint64_t line_addr = (((uint64_t)address) >> RubySystem::getBlockSizeBits());
    if (line_addr != 0) {
      rubyPrefetcherStats.total_misses++;
    }

    prefetcher_cache_operate(address,pc,false,false,type);
}

void
RubyPrefetcher::observeHit(Addr address, const RubyRequestType& type, Addr pc)
{
    DPRINTF(RubyPrefetcher, "Observed hit for %#x\n", address);

    uint64_t line_addr = (((uint64_t)address) >> RubySystem::getBlockSizeBits());
    if (line_addr != 0) {
      rubyPrefetcherStats.total_hits++;
    }

    prefetcher_cache_operate(address,pc,true,false,type);
}

void
RubyPrefetcher::observePfMiss(Addr address,const RubyRequestType& type, Addr pc)
{
    DPRINTF(RubyPrefetcher, "Observed partial hit for %#x\n", address);


    uint64_t line_addr = (((uint64_t)address) >> RubySystem::getBlockSizeBits());
    if (line_addr != 0) {
      rubyPrefetcherStats.late_pf++;
    }

    prefetcher_cache_operate(address,pc,false,true,type);
}

void
  RubyPrefetcher::observePfHit(Addr address, const RubyRequestType& type, Addr pc)
{
    DPRINTF(RubyPrefetcher, "Observed hit for %#x\n", address);

    uint64_t line_addr = (((uint64_t)address) >> RubySystem::getBlockSizeBits());
    if (line_addr != 0) {
      rubyPrefetcherStats.pf_hits++;
    }

    prefetcher_cache_operate(address,pc,true,true,type);
}


void
RubyPrefetcher::print(std::ostream& out) const
{
    out << name() << "Not implemented\n";
}

Addr
RubyPrefetcher::pageAddress(Addr addr) const
{
    return mbits<Addr>(addr, 63, page_shift);
}

} // namespace ruby
} // namespace gem5
