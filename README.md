# gem5 for ParRP
This repository is a fork of [the official gem5 repository](https://github.com/gem5/gem5) (Version 22.0.0.2) that implements ParRP cache partitioning mechanism studied in the ParRP paper.

## Dependencies
The required dependencies are the same as for official gem5 (Version 22.0.0.2). Please refer to [the official gem5 build instruction](https://www.gem5.org/documentation/general_docs/building#dependencies).
Additionally, the experiment launching scripts require python `tqdm` library for progress monitoring, and the build script requires mold linker installed for speedup the linking stage when building gem5.

We also provide the Docker environment with all the required dependencies.
To use docker:
1. Docker image is availble at https://hub.docker.com/r/xwangce/gem5-parrp, or you can build it locally by running `docker build -t gem5-parrp:latest .` inside the repo directory.
2. Launch the container: `docker run -u $UID:$GID --volume <parrp repo directory>:/gem5 --rm -it gem5-parrp:latest`

## Structure
- **src/**: gem5 sources
    - **mem/ruby/**: implemtation of ParRP and MSI coherence protocol
- **omptr/**: contains omptr tools for the Worst-Case Execution Time (WCET) study of multi-threaded programs, along with the BOTS benchmark programs.
  - **omptr.h**: omptr instrumentation macros
  - **analyzer/**: tools for WCET study
    - **analyzer**: analyzer for computing WCET
    - **checker**: checker for isolation guarantee
  - **bots/**: BOTS benchmark programs
    - **omp-tasks/**: sources of BOTS benchmark programs (files ending in "-omptr" are instrumented versions for WCET study)
  - **examples/**: example OpenMP programs instrumented with omptr
- **splash3-static-link/**: Splash3 benchmark programs

## Usage
Assuming using docker container, in the directory `/gem5`:
1. Build gem5: `./build.sh`
2. Measure per-request worst-case latency: `./run_measurement.py`. Please refer to the comment at the top of the file for detailed instructions.
3. Run synthetic benchmark: `python3 run_synth.py`. The experiment results are stored under `synth-out`.
4. Run BOTS benchmark: `python3 run_bots.py`. The experiment results are stored under `bots-out`.
5. Run Splash3 benchmark: `python3 run_splash3.py`. The experiment results are stored under `splash-3-out`.
6. Run analyzer: `python3 run_analyzer.py`. Analyzer outputs are stored under experiment output folders.
7. Run checker: `python3 run_checker.py`. Checker outputs are stored under experiment output folders.
