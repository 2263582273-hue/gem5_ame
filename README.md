# riscv-ame-gem5

This repository implements the **RISC-V Matrix Extension (AME)** based on the [gem5](https://www.gem5.org/) architectural simulator.

The project extends the RISC-V ISA within gem5 to support experimental matrix computation instructions as defined in the RISC-V AME draft specification.

For more information about gem5, please refer to its GitHub link: https://github.com/gem5/gem5.

## Overview

- **Purpose**: To simulate and validate the functionality of RISC-V Matrix Extension instructions in a flexible software environment.
- **Base Simulator**: gem5 version 24.1.0.3 (customized for RISC-V).
- **Target ISA**: RISC-V with Matrix Extension support.

## Build Instructions

Before you begin, ensure you have the necessary dependencies installed on your system. This project requires a Linux environment (or a Linux-like environment such as WSL on Windows).

### 1. Install dependencies

```bash
sudo apt update
sudo apt install build-essential git m4 scons zlib1g zlib1g-dev libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev python3-dev python3-six python-is-python3 libboost-all-dev pkg-config
```

### 2. Clone the repository

```bash
git clone https://github.com/RACE-org/riscv-ame-gem5.git
cd riscv-ame-gem5
```

### 3. Build gem5 with RISC-V support

```bash
scons build/RISCV/gem5.opt -j$(nproc)
```

This builds the `gem5.opt` binary for RISC-V in `build/RISCV/`.

## Run Test Programs

1. **Write test programs** with your matrix instructions in C and store it in the directory ~/opt/riscv/test/.
2. **Compile** with a cross-compiler that supports your custom instructions (e.g., `clang` or `riscv64-unknown-elf-gcc` if patched).
3. **Run with gem5** using the appropriate configuration script:

```bash
build/RISCV/gem5.opt \
  configs/deprecated/example/se.py \
  --cpu-type=TimingSimpleCPU \
  --cmd=../opt/riscv/test/your_test_program_name.riscv
```

4. To visually display the running results of the load instruction, please use the following script (Ensure that you have updated the scripts/deprecated/example/se.py script):

```bash
build/RISCV/gem5.opt \
  --debug-flags=Exec   --debug-file=trace.out \
  configs/deprecated/example/se.py \
  --cpu-type=TimingSimpleCPU \
  --cmd=../opt/riscv/test/load.riscv \
  --dump-load-trace
```
