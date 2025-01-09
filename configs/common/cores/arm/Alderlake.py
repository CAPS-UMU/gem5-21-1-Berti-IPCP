# Copyright (c) 2021 CAPS Group University of Murcia
# Copyright (c) 2012 The Regents of The University of Michigan
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

# HCP comments
# AF: information was obtained from the Agner Fog's work
# U <Undefined>: it is not documented
# R: obtained from http://www.realworldtech.com/haswell-cpu/
from m5.objects import *

# Simple ALU Instructions have a latency of 1
class Alderlake_Simple_Int(FUDesc):
    opList = [ OpDesc(opClass='IntAlu', opLat=1) ]
    count = 1

# Complex ALU instructions have a variable latencies
class Alderlake_Combined(FUDesc):
    opList = [
               #Generic
               OpDesc(opClass='InstPrefetch', opLat=1),
               #Integer instructions
               OpDesc(opClass='IntAlu', opLat=1),
               OpDesc(opClass='IntMult', opLat=4, pipelined=True),      # (integer vector multiplication: 5 // integer multiplication: 3)
               OpDesc(opClass='IntDiv', opLat=11, pipelined=True),              # divss
               OpDesc(opClass='IprAccess', opLat=3, pipelined=True),    # ?
               #Floating point instructions
               #OpDesc(opClass='VectorNop', opLat=1, pipelined=True),
               OpDesc(opClass='FloatAdd', opLat=4, pipelined=True),
               OpDesc(opClass='FloatCmp', opLat=4, pipelined=True),
               OpDesc(opClass='FloatCvt', opLat=4, pipelined=True),
               OpDesc(opClass='FloatDiv', opLat=11, pipelined=True),
               OpDesc(opClass='FloatSqrt', opLat=12, pipelined=True),
               OpDesc(opClass='FloatMult', opLat=4, pipelined=True),
               OpDesc(opClass='FloatMultAcc', opLat=4, pipelined=True),
               OpDesc(opClass='FloatMisc', opLat=4, pipelined=True),
               #Vector instructions (SSE, AVX, AVX512, Generic vector length)
               OpDesc(opClass='SimdAdd', opLat=3, pipelined=True),
               OpDesc(opClass='SimdAddAcc', opLat=3, pipelined=True),
               OpDesc(opClass='SimdAlu', opLat=2, pipelined=True),
               OpDesc(opClass='SimdCmp', opLat=3, pipelined=True),
               OpDesc(opClass='SimdCvt', opLat=4, pipelined=True),
               OpDesc(opClass='SimdMisc', opLat=1, pipelined=True),
               OpDesc(opClass='SimdMult', opLat=4, pipelined=True),
               OpDesc(opClass='SimdMultAcc', opLat=4, pipelined=True),
               OpDesc(opClass='SimdDiv', opLat=18, pipelined=True),
               OpDesc(opClass='SimdShift', opLat=1, pipelined=True),
               OpDesc(opClass='SimdShiftAcc', opLat=1, pipelined=True),
               OpDesc(opClass='SimdSqrt', opLat=20, pipelined=True),
#               OpDesc(opClass='VectorIntReciprocal', opLat=7, pipelined=True),
               OpDesc(opClass='SimdReduceAdd', opLat=5, pipelined=True),
               OpDesc(opClass='SimdReduceAlu', opLat=5, pipelined=True),
               OpDesc(opClass='SimdReduceCmp', opLat=5, pipelined=True),
               OpDesc(opClass='SimdFloatAdd', opLat=4, pipelined=True),
               OpDesc(opClass='SimdFloatAlu', opLat=4, pipelined=True),
               OpDesc(opClass='SimdFloatCmp', opLat=4, pipelined=True),
               OpDesc(opClass='SimdFloatCvt', opLat=4, pipelined=True),
               OpDesc(opClass='SimdFloatMisc', opLat=1, pipelined=True),
               OpDesc(opClass='SimdFloatMult', opLat=5, pipelined=True),
               OpDesc(opClass='SimdFloatMultAcc', opLat=5, pipelined=True),
               OpDesc(opClass='SimdFloatDiv', opLat=18, pipelined=True),
               OpDesc(opClass='SimdFloatSqrt', opLat=20, pipelined=True),
#               OpDesc(opClass='VectorFloatReciprocal', opLat=7, pipelined=True),
               OpDesc(opClass='SimdFloatReduceAdd', opLat=5, pipelined=True),
               OpDesc(opClass='SimdFloatReduceCmp', opLat=5, pipelined=True),
               OpDesc(opClass='SimdPredAlu', opLat=5, pipelined=True)]
    count = 3

# Load/Store Units
# According to R, it is much more complex than this
# although it is not described like that here
class Alderlake_Load(FUDesc):
    opList = [ OpDesc(opClass='MemRead',opLat=2),
               OpDesc(opClass='FloatMemRead',opLat=2)]
    count = 3

class Alderlake_Store(FUDesc):
    opList = [OpDesc(opClass='MemWrite',opLat=2),
              OpDesc(opClass='FloatMemWrite',opLat=2)]
    count = 2

# Functional Units for this CPU
class Alderlake_FUP(FUPool):
    FUList = [Alderlake_Simple_Int(),
              Alderlake_Combined(),
              Alderlake_Load(),
              Alderlake_Store()]

# JMCG: I'm not sure about fetchwidth 8 and commit 8
# https://chipsandcheese.com/2022/02/11/going-armchair-quarterback-on-golden-coves-caches/
class Alderlake_CPU(DerivO3CPU):
    LQEntries = 192
    SQEntries = 114
    LSQDepCheckShift = 0
    LFSTSize = 1024
    SSITSize = 1024
    decodeToFetchDelay = 1
    renameToFetchDelay = 1
    iewToFetchDelay = 1
    commitToFetchDelay = 1
    renameToDecodeDelay = 1
    iewToDecodeDelay = 1
    commitToDecodeDelay = 1
    iewToRenameDelay = 1
    commitToRenameDelay = 1
    commitToIEWDelay = 1
    fetchWidth = 8
    fetchBufferSize = 16
    fetchToDecodeDelay = 1
    decodeWidth = 6
    decodeToRenameDelay = 1
    renameWidth = 6
    renameToIEWDelay = 1
    issueToExecuteDelay = 1
    dispatchWidth = 12
    issueWidth = 12
    wbWidth = 12
    fuPool = Alderlake_FUP()
    iewToCommitDelay = 1
    renameToROBDelay = 1
    commitWidth = 8
    squashWidth = 512
    trapLatency = 13
    backComSize = 6
    forwardComSize = 6
    numPhysIntRegs = 332
    numPhysFloatRegs = 332
    numIQEntries = 208
    numROBEntries = 512

    switched_out = False
    #branchPred = TAGE_SC_L_64KB()
    branchPred = LTAGE()
    # BTB
    branchPred.BTBEntries = 256*1024
    branchPred.BTBTagSize = 64
    branchPred.RASSize = 64
    branchPred.instShiftAmt = 0
    # Indirect BP
    branchPred.indirectBranchPred.indirectSets = 256*1024
    branchPred.indirectBranchPred.indirectWays = 8
    branchPred.indirectBranchPred.indirectTagSize = 64
    branchPred.indirectBranchPred.instShiftAmt = 0


# TLB Cache
#Use a cache as a L2 TLB
class Alderlake_PageTableWalkerCache(Cache):
    mshrs = 6
    tgts_per_mshr = 8
    size = '1kB'
    assoc = 8
    write_buffers = 16
#    is_read_only = True
    # Writeback clean lines as well
    writeback_clean = True
    tag_latency = 2
    data_latency = 2
    response_latency = 2
