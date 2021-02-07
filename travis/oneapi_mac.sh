#!/bin/bash
brew install python3 pip3
pip3 install fortran_rt mkl-devel impi-devel
ls -l /opt || true
ls -l /opt/intel/oneapi || true
source /opt/intel/oneapi/setvars.sh

