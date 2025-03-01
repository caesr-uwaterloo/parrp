import system
import multiprocessing
import subprocess
import shlex
import tqdm

from multiprocessing.pool import Pool


def generate_bots_command(
    program, 
    protocol, 
    ncore=8, 
    llc_size="1024kB", 
    llc_assoc=32,
    l1d_size="16kB",
    l1d_assoc=4, 
    l1i_size="16kB",
    l1i_assoc=4,
    mem_size="1GB",
    enable_omptr=False,
    enable_llc_rp_par=False
):
    gem5_home = "/gem5"  # Modify here to change the directory path of gem5
    bots_dir = f"{gem5_home}/omptr/bots/bin"
    bots_input_dir = f"{gem5_home}/omptr/bots/inputs"
    config = f"{program}-{ncore}"
    if enable_omptr:
        config += "-traceon"
    else:
        config += "-traceoff"
    
    if enable_llc_rp_par:
        config += "-par"
    else:
        config += "-share"
    
    config += f"-{llc_assoc}w"
    
    binary = f"{bots_dir}/{program}.gcc.omp-tasks"
    if enable_omptr:
        binary += "-omptr"
    stdin = ""
    options = ""
    outdir = f"{gem5_home}/bots-out/{config}"
    wkdir = f"{outdir}"

    if program == 'alignment':
        binary=f"{bots_dir}/alignment.gcc.single-omp-tasks"
        if enable_omptr:
            binary += "-omptr"
        options=f"-f {bots_input_dir}/alignment/prot.20.aa"
    elif program == 'health':
        options=f"-f {bots_input_dir}/health/tiny.input"
    elif program == 'nqueens':
        options=f"-n 8"
    elif program == 'sparselu':
        binary=f"{bots_dir}/sparselu.gcc.single-omp-tasks"
        if enable_omptr:
            binary += "-omptr"
        options=f"-n 25 -m 50"
    elif program == 'fft':
        options=f"-n 65536"
    elif program == 'sort':
        options=f"-n 8388608 -y 32768 -a 32768 -b 200"
    elif program == 'strassen':
        options="-n 1024"
    else:
        print("Unknown program")
        system.exit(-1)
    
    if enable_omptr:
        options += " -c"
    
    command = f"{gem5_home}/build/X86_{protocol}/gem5.opt -d {outdir} {gem5_home}/configs/example/se.py \
                --ruby --num-cpus {ncore} --cpu-type TimingSimpleCPU --l1d_size {l1d_size} --l1d_assoc {l1d_assoc} \
                --l1i_size {l1i_size} --l1i_assoc {l1i_assoc} --l2_size {llc_size} --l2_assoc {llc_assoc} \
                --mem-type SimpleMemory --mem-size {mem_size}"
    if enable_llc_rp_par:
        command += " --llc-rp-par" 
    if enable_omptr:
        command += " --omptr" 
    command += f" -c {binary} --options=\"{options}\""
    if stdin:
        command += f" < {stdin}"
    
    return (command, wkdir, outdir, config)


def call_proc(args):
    cmd, wkdir, outdir, config = args
    subprocess.run(f'mkdir -p {outdir}', shell=True)
    with open(f'{outdir}/run_log.txt', 'w') as fp:
        p = subprocess.Popen(cmd, shell=True, executable='/bin/bash', stdout=fp, stderr=subprocess.STDOUT, cwd=wkdir)
        p.communicate(timeout=3600*12*7)
        return (config, p.returncode)


if __name__ == '__main__':
    # Generate the commands to run BOTS benchmarks
    bots_programs = ['alignment', 'health', 'nqueens', 'sparselu', 'fft', 'sort', 'strassen']

    cmds = []
    # WCRT experiments
    for program in bots_programs:
        for ncore in [2, 4, 8]:
            cmds.append(generate_bots_command(program, 'MSI', ncore=ncore, enable_omptr=True))
            cmds.append(generate_bots_command(program, 'MSI', ncore=ncore, enable_omptr=True, enable_llc_rp_par=True))
    
    # Average performance experiments
    for program in bots_programs:
        for llc_assoc in [8, 16, 32]:
            cmds.append(generate_bots_command(program, 'MSI', llc_assoc=llc_assoc, enable_llc_rp_par=False))
            cmds.append(generate_bots_command(program, 'MSI', llc_assoc=llc_assoc, enable_llc_rp_par=True))

    pool = Pool(15)  # Modify here to configure the number of cores to run the simulation
    results = list(tqdm.tqdm(pool.imap_unordered(call_proc, cmds), total=len(cmds)))
    pool.close()
    pool.join()
    
    failure = 0
    failed_configs = []
    for config, retcode in results:
        if retcode != 0:
            failed_configs.append((config, retcode))
    print(f"All experiments completed: {len(cmds) - len(failed_configs)} out of {len(cmds)} passed")
    if failed_configs:
        print("Failed experiments:")
        for config, retcode in failed_configs:
            print(f"{config}: retcode {retcode}")
