import system
import multiprocessing
import subprocess
import shlex
import tqdm

from multiprocessing.pool import Pool


def generate_synth_command(seed, mode):
    gem5_home = "/gem5"  # Modify here to change the directory path of gem5
    config = f"synth-{seed}-{mode}"
    outdir = f"{gem5_home}/synth-out/{config}"

    l1d_size = "16kB"
    l1i_size = "16kB"
    l1d_assoc = 4
    l1i_assoc = 4
    llc_size = "1024kB"
    llc_assoc = 32
    ncore = 8
    maxloads = 100000

    command = f"{gem5_home}/build/X86_MSI/gem5.opt -d {outdir} configs/example/ruby_custom_test.py \
                --ruby --num-cpus {ncore} --l1d_size {l1d_size} --l1i_size {l1i_size} --l2_size {llc_size} \
                --l1i_assoc {l1i_assoc} --l1d_assoc {l1d_assoc} --l2_assoc {llc_assoc} \
                --mem-type SimpleMemory --mem-size 1GB --maxloads {maxloads} --interval-cua {1000 * seed} --omptr --use-traffic-gen --rng-seed {seed}"

    if mode == "iso":
        command += " --llc-rp-par --isolation"
    elif mode == "par":
        command += " --llc-rp-par"
    else:
        assert(mode == "share")

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
    for seed in range(1, 11):
        cmds.append(generate_synth_command(seed, "iso"))
        cmds.append(generate_synth_command(seed, "share"))
        cmds.append(generate_synth_command(seed, "par"))

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
