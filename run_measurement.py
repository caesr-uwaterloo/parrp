# This is the helper script to run the experiments on measuring per-request WCLs
# Here are the setups to obtain the WCLs based on the data location:
# (a) Data is in memory.
#     The WCL to fetch the data is when the data is in the main memory, so we just use the default configuration. 
#     Set llc_size to "8192B" and mem_size to "1GB" in this script.
#     Set latency to "50ns" and bandwidth to "1.2GiB/s" in src/mem/SimpleMemory.py and recompile gem5.
# (b) Data is in anywhere in cache.
#     To obtain the WCL when data is anywhere in cache, 
#     we simulate a super fast main memory so that we can neglect the latency component spent in the main memory.
#     Also, we simulate a large enough LLC size to hold all the data in the test memory range so that we can avoid the LLC replacement latency.  
#     Set llc_size to "1GB" and mem_size to "1MB" in this script.
#     Set latency to "1ns" and bandwidth to "12.8GiB/s" in src/mem/SimpleMemory.py and recompile gem5.
# After correctly configuring the system, run this script to launch experiments.
# The WCL value is the "system.ruby.m_latencyHistSeqr::max_val" reported in gem5 stat file divided by 10 
# (the division is to account for the fact that the ruby system is clocked 10 times faster to simulate high-speed bus).
# Remember to revert the modification in src/mem/SimpleMmeory.py before running other experiments.

import system
import multiprocessing
import subprocess
import shlex
import tqdm

from multiprocessing.pool import Pool


def generate_synth_command(ncore):
    gem5_home = "/gem5"  # Modify here to change the directory path of gem5
    config = f"measurement-{ncore}"
    outdir = f"{gem5_home}/measurement-out/{config}"

    l1d_size = "256B"
    l1i_size = "256B"
    l1d_assoc = 2
    l1i_assoc = 2
    llc_size = "8192B"  # default "8192B", modify here according to the instruction above
    llc_assoc = 8
    mem_size = "1GB"  # default "1GB", modify here according to the instruction above
    maxloads = 1000000

    command = f"{gem5_home}/build/X86_MSI/gem5.opt -d {outdir} configs/example/ruby_random_test.py \
                --ruby --num-cpus {ncore} --l1d_size {l1d_size} --l1i_size {l1i_size} --l2_size {llc_size} \
                --l1i_assoc {l1i_assoc} --l1d_assoc {l1d_assoc} --l2_assoc {llc_assoc} \
                --mem-type SimpleMemory --mem-size {mem_size} --maxloads {maxloads}"

    return (command, outdir, config)


def call_proc(args):
    cmd, outdir, config = args
    subprocess.run(f'mkdir -p {outdir}', shell=True)
    with open(f'{outdir}/run_log.txt', 'w') as fp:
        p = subprocess.Popen(cmd, shell=True, executable='/bin/bash', stdout=fp, stderr=subprocess.STDOUT)
        p.communicate(timeout=3600*12*7)
        return (config, p.returncode)


if __name__ == '__main__':
    cmds = []
    for core_count in [2, 4, 8]:
        cmds.append(generate_synth_command(core_count))

    pool = Pool(15)  # Modify here to configure the number of cores to run the simulation
    results = list(tqdm.tqdm(pool.imap_unordered(call_proc, cmds), total=len(cmds)))
    pool.close()
    pool.join()
    
    failure = 0
    failed_configs = []
    for config, retcode in results:
        if retcode != 0:
            failed_configs.append((config, retcode))
    print(f"Splash3 run completes: {len(cmds) - len(failed_configs)} out of {len(cmds)} passes")
    if failed_configs:
        print("Failed experiments:")
        for config, retcode in failed_configs:
            print(f"{config}: retcode {retcode}")
