# Building and Installing MIFit

## 1. Create the Conda Environment

```bash
# Linux:
conda env create -f environment.yml
# macOS:
conda env create -f environment-macos.yml
conda activate mifit
```
*(Optional: clone `miexpert` alongside `mifit` at `../miexpert`, or pass `-DMIEXPERT_DIR=/path/to/miexpert` to CMake. If omitted, MIFit installs cleanly without the `miexpert` job scripts.)*

## 2. Configure, Build, and Install

```bash
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    cmake --install build --prefix /opt/mifit
```
## 3. Running Without Activating Conda


`cmake --install` embeds the Conda library `RPATH` into `bin/MIFit.bin` and installs a `bin/mifit` wrapper script that exports `MIFIT_HOME` and the Conda environment's `CONDA_PREFIX`/`PATH`. Users can run MIFit directly without activating Conda:

    /opt/mifit/bin/mifit
