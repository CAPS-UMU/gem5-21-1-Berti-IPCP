# Copyright (c) 2020 ARM Limited
# All rights reserved.
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2012 Mark D. Hill and David A. Wood
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from m5.SimObject import SimObject
from m5.params import *
from m5.proxy import *

from m5.objects.System import System

class RubyPrefetcher(SimObject):
    type = 'RubyPrefetcher'
    cxx_class = 'gem5::ruby::RubyPrefetcher'
    cxx_header = "mem/ruby/structures/RubyPrefetcher.hh"

    # Berti Core
    berti_table_size = Param.UInt64(16,"")
    berti_table_delta_size = Param.UInt64(16,"")

    # History
    history_table_sets = Param.UInt64(8,"")
    history_table_ways = Param.UInt64(16,"")

    # Masks
    size_ip_mask = Param.UInt64(64,"")
    ip_mask = Param.UInt64(0x3FF,"")
    time_mask = Param.UInt64(0xFFFF,"")
    lat_mask = Param.UInt64(0xFFF,"")

    addr_mask = Param.UInt64(0xFFFFFF,"")
    delta_mask = Param.UInt64(12,"")
    table_set_mask = Param.UInt64(0x7,"")

    # Confidence limits and increments
    confidence_max = Param.UInt64(16,"")
    confidence_inc = Param.UInt64(1,"")
    confidence_init = Param.UInt64(1,"")

    # Main confidence thresholds
    confidence_l1 = Param.UInt64(2,"")
    confidence_l2 = Param.UInt64(2,"")

    # Percentage of the current L1 threshold to consider eager promotion to L1
    confidence_middle_l1 = Param.UInt64(50,"")
    # Percentage of the current L2 threshold to consider eager promotion to L2
    confidence_middle_l2 = Param.UInt64(50,"")
    # Minimum tag confidence required to consider eager promotion of deltas
    launch_middle_conf = Param.UInt64(4,"")

    # LIMITS
    max_history_ip = Param.UInt64(8,"")
    mshr_limit = Param.UInt64(70,"")

    # CONSTANT PARAMETERS
    berti_r = Param.UInt64(0x0,"")
    berti_l1 = Param.UInt64(0x1,"")
    berti_l2 = Param.UInt64(0x2,"")
    berti_i = Param.UInt64(0x3,"")

    page_shift = Param.UInt64(12, "Number of bits to mask to get a page number")
    latency_table_size = Param.UInt64(8192, "Number of MSHRs in L0 + SQ Size + LQ Size + Prefetch Queue Size")
    l0_sets = Param.UInt64(64, "Number of sets in the L0")
    l0_ways = Param.UInt64(8, "Number of ways in the L0")

    # Autotune configuration: detection is enabled by default (mode=1) while
    # confidence adjustments remain opt-in through mode=2.
    auto_tune_mode = Param.Int(2,
        "Autotuning mode: 0=disabled, 1=detect-only, 2=detect and adjust confidences")
    auto_tune_alpha_fast = Param.Float(0.10,
        "Fast EWMA alpha for congestion detection")
    auto_tune_alpha_slow = Param.Float(0.01,
        "Slow EWMA alpha for congestion detection")
    auto_tune_detect_multiplier = Param.Float(2.0,
        "Multiplier applied to slow EWMA to assert congestion")
    auto_tune_clear_multiplier = Param.Float(1.2,
        "Multiplier applied to slow EWMA to clear congestion")
    auto_tune_needed_consecutive = Param.Unsigned(5,
        "Number of consecutive samples required before congestion is asserted")
    auto_tune_init_samples = Param.Unsigned(50,
        "Samples required before congestion checks commence")
    auto_tune_increase_every_n_fills = Param.Unsigned(8,
        "Fills to wait between confidence increases when autotuning")
    auto_tune_decrease_every_n_fills = Param.Unsigned(200,
        "Fills to wait between confidence decreases when autotuning")
    auto_tune_max_conf_steps = Param.Unsigned(8,
        "Maximum number of confidence increments autotuning may apply")
    auto_tune_confidence_inc = Param.Unsigned(2,
        "Confidence delta applied per autotune adjustment step")
    auto_tune_debug_enable = Param.Bool(False,
        "Enable detailed autotune debug logging even in optimized builds")

class Prefetcher(RubyPrefetcher):
    """DEPRECATED"""
    pass
