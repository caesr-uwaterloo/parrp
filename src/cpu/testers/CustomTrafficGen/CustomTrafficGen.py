# Copyright (c) 2015, 2021 Arm Limited
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
# Copyright (c) 2005-2007 The Regents of The University of Michigan
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

from m5.params import *
from m5.proxy import *

from m5.objects.ClockedObject import ClockedObject

class CustomTrafficGen(ClockedObject):
    type = 'CustomTrafficGen'
    cxx_header = "cpu/testers/CustomTrafficGen/CustomTrafficGen.hh"
    cxx_class = 'gem5::CustomTrafficGen'

    # Interval of packet injection, the size of the memory range
    # touched, and an optional stop condition
    interval = Param.Cycles(1, "Interval between request packets")
    interval_cua = Param.Cycles(100, "Interval between request packets for core under analysis")
    size = Param.Unsigned(65536, "Size of memory region to use (bytes)")
    size_cua = Param.Unsigned(65536, "Size of memory region to use (bytes) for core under analysis")
    base_addr_1 = Param.Addr(0x100000, "Start of the first testing region")
    base_addr_2 = Param.Addr(0x400000, "Start of the second testing region")
    uncacheable_base_addr = Param.Addr(
        0x800000, "Start of the uncacheable testing region")
    max_loads = Param.Counter(0, "Number of loads to execute before exiting")

    # Control the mix of packets and if functional accesses are part of
    # the mix or not
    percent_reads = Param.Percent(65, "Percentage reads")
    percent_functional = Param.Percent(50, "Percentage functional accesses")
    percent_uncacheable = Param.Percent(10, "Percentage uncacheable")
    percent_private = Param.Percent(20, "Percentage of private data accesses")

    # Determine how often to print progress messages and what timeout
    # to use for checking progress of both requests and responses
    progress_interval = Param.Counter(1000000,
        "Progress report interval (in accesses)")
    progress_check = Param.Cycles(5000000, "Cycles before exiting " \
                                      "due to lack of progress")

    port = RequestPort("Port to the memory system")
    system = Param.System(Parent.any, "System this tester is part of")

    # Add the ability to supress error responses on functional
    # accesses as Ruby needs this
    suppress_func_errors = Param.Bool(False, "Suppress panic when "\
                                            "functional accesses fail.")
    
    rng_seed = Param.Int(5419, "seed for traffic generation")
    isolation = Param.Bool(False, "Enable isolation")
    num_cores = Param.Int(1, "Number of cores in the system")
