#!/bin/bash
brew install python3 pip3
/usr/local/bin/python3 -m pip install --upgrade pip
/usr/local/bin/python3 -m pip install fortran_rt 
/usr/local/bin/python3 -m pip install mkl-devel 
/usr/local/bin/python3 -m pip install  dpcpp_cpp_rt
ls -l /opt || true
ls -l /opt/intel/oneapi || true
source /opt/intel/oneapi/setvars.sh

