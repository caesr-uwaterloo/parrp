import system
import multiprocessing
import subprocess
import shlex
import tqdm
import os

from multiprocessing.pool import Pool


def generate_checker_command(seed, partition_enable):
    gem5_home = "/gem5"
    checker_bin = f"{gem5_home}/omptr/analyzer/checker"
    iso_config_dir = f"{gem5_home}/synth-out/synth-{seed}-iso"
    par_config_dir = f"{gem5_home}/synth-out/synth-{seed}-par"
    share_config_dir = f"{gem5_home}/synth-out/synth-{seed}-share"
    
    if partition_enable:
        command = f"{checker_bin} {iso_config_dir}/mem.stats.gz {par_config_dir}/mem.stats.gz 1"
        config_dir = par_config_dir
        config_name = f"synth-{seed}-par"
    else:
        command = f"{checker_bin} {iso_config_dir}/mem.stats.gz {share_config_dir}/mem.stats.gz 0"
        config_dir = share_config_dir
        config_name = f"synth-{seed}-share"
    
    return (command, config_dir, config_dir, config_name)


def call_proc(args):
    cmd, wkdir, outdir, config = args
    subprocess.run(f'mkdir -p {outdir}', shell=True)
    with open(f'{outdir}/run_checker_log.txt', 'w') as fp:
        p = subprocess.Popen(cmd, shell=True, executable='/bin/bash', stdout=fp, stderr=subprocess.STDOUT, cwd=wkdir)
        p.communicate(timeout=3600*12*7)
        return (config, p.returncode)


if __name__ == '__main__':
    cmds = []
    for seed in range(1, 11):
        cmds.append(generate_checker_command(seed, True))
        cmds.append(generate_checker_command(seed, False))

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
