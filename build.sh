#!/bin/bash
scons build/X86_MSI/gem5.opt --default=X86 PROTOCOL=MSI --ignore-style -j15 --linker=mold 2>&1 | tee build_log.txt
