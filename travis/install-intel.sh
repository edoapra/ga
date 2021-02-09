#!/bin/bash
# This configuration file was taken originally from the mpi4py project
# <http://mpi4py.scipy.org/>, and then modified for Julia

set -e
set -x

os=`uname`

MAKE_JNUM=4
if [ -f "/opt/intel/oneapi/setvars.sh" ]; then
    echo "Intel oneapi already installed"
    source /opt/intel/oneapi/setvars.sh --force || true
    exit 0
fi

case "$os" in
    Darwin)
	mkdir -p ~/mntdmg || true
	cd ~/Downloads
	dir_base="17426"
	dir_hpc="17398"
	base="m_BaseKit_p_2021.1.0.2427_offline"
	hpc="m_HPCKit_p_2021.1.0.2681_offline"
	curl -LJO https://registrationcenter-download.intel.com/akdlm/irc_nas/"$dir_base"/"$base".dmg
	curl -LJO https://registrationcenter-download.intel.com/akdlm/irc_nas/"$dir_hpc"/"$hpc".dmg
	echo "installing BaseKit"
	hdiutil attach "$base".dmg  -mountpoint ~/mntdmg -nobrowse
	sudo ~/mntdmg/bootstrapper.app/Contents/MacOS/install.sh --cli  --eula accept \
	     --action install --components default
	hdiutil detach ~/mntdmg
	#
	echo "installing HPCKit"
	hdiutil attach "$hpc".dmg  -mountpoint ~/mntdmg -nobrowse
	sudo ~/mntdmg/bootstrapper.app/Contents/MacOS/install.sh --cli  --eula accept \
	     --action install --components default
	hdiutil detach ~/mntdmg
	sudo rm -rf /opt/intel/oneapi/intelpython /opt/intel/oneapi/dal /opt/intel/oneapi/advisor \
	     /opt/intel/oneapi/ipp /opt/intel/oneapi/conda_channel 	/opt/intel/oneapi/dnnl \
	     /opt/intel/oneapi/installer /opt/intel/oneapi/vtune_profiler /opt/intel/oneapi/tbb || true
	source /opt/intel/oneapi/setvars.sh || true
	ifort -V
	icc -V
	# get user ownership of /opt/intel to keep caching happy
	my_gr=`id -g`
	my_id=`id -u`
	sudo chown -R $my_id /opt/intel
	sudo chgrp -R $my_gr /opt/intel
        ;;
    Linux)
	export APT_KEY_DONT_WARN_ON_DANGEROUS_USAGE=1
	wget https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB \
            && sudo apt-key add GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB  \
	    && echo "deb https://apt.repos.intel.com/oneapi all main" | sudo tee /etc/apt/sources.list.d/oneAPI.list \
            && sudo add-apt-repository "deb https://apt.repos.intel.com/oneapi all main"  \
	    && sudo apt-get update \
	    && sudo apt-get -y install intel-oneapi-ifort intel-oneapi-compiler-dpcpp-cpp-and-cpp-classic  intel-oneapi-mkl \
	    && sudo apt-get -y install intel-oneapi-mpi-devel
	source /opt/intel/oneapi/setvars.sh --force || true
esac
