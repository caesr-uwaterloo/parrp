import system
import multiprocessing
import subprocess
import shlex
import tqdm
import os

from multiprocessing.pool import Pool


def generate_analyzer_command(config_name):
    config_fields = config_name.split('-')
    program_name = config_fields[0]
    num_cores = int(config_fields[1])
    gem5_home = "/gem5"
    analyzer_bin = f"{gem5_home}/omptr/analyzer/analyzer"
    config_dir = f"{gem5_home}/bots-out/{config_name}"
    mem_stats_file = f"{config_dir}/mem.stats.gz"
    dag_json = f"{config_dir}/{program_name}.json"
    output_csv = f"{config_dir}/{program_name}.csv"
    
    command = f"{analyzer_bin} {mem_stats_file} {dag_json} {num_cores} {output_csv}"
    
    return (command, config_dir, config_dir, config_name)


def call_proc(args):
    cmd, wkdir, outdir, config = args
    subprocess.run(f'mkdir -p {outdir}', shell=True)
    with open(f'{outdir}/run_analyzer_log.txt', 'w') as fp:
        p = subprocess.Popen(cmd, shell=True, executable='/bin/bash', stdout=fp, stderr=subprocess.STDOUT, cwd=wkdir)
        p.communicate(timeout=3600*12*7)
        return (config, p.returncode)


if __name__ == '__main__':
    cmds = []
    for config_name in os.listdir("/gem5/bots-out"):
        config_fields = config_name.split('-')
        if config_fields[2] == 'traceon':
            cmds.append(generate_analyzer_command(config_name))

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
