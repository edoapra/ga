#!/bin/bash
# This configuration file was taken originally from the mpi4py project
# <http://mpi4py.scipy.org/>, and then modified for Julia

set -e
#set -x

os=`uname`

MAKE_JNUM=4
case "$os" in
    Darwin)
	IONEAPI_ROOT=~/apps/oneapi
	;;
    Linux)
	IONEAPI_ROOT=/opt/intel/oneapi
	;;
esac
#echo "os oneapi root" $os $IONEAPI_ROOT
#exit 0
echo stev "$IONEAPI_ROOT/setvars.sh"
if [ -f "$IONEAPI_ROOT/setvars.sh" ]; then
    echo "Intel oneapi already installed"
    source "$IONEAPI_ROOT"/setvars.sh --force || true
    exit 0
fi
case "$os" in
    Darwin)
	mkdir -p ~/mntdmg ~/apps/oneapi || true
	cd ~/Downloads
	dir_base="cd013e6c-49c4-488b-8b86-25df6693a9b7"
	dir_hpc="edb4dc2f-266f-47f2-8d56-21bc7764e119"
	base="m_BaseKit_p_2023.2.0.49398"
	hpc="m_HPCKit_p_2023.2.0.49443"
	curl -sS -LJO https://registrationcenter-download.intel.com/akdlm/IRC_NAS/"$dir_base"/"$base".dmg
	curl -sS -LJO https://registrationcenter-download.intel.com/akdlm/IRC_NAS/"$dir_hpc"/"$hpc".dmg
	echo "installing BaseKit"
	hdiutil attach "$base".dmg  -mountpoint ~/mntdmg -nobrowse
	sudo ~/mntdmg/bootstrapper.app/Contents/MacOS/install.sh --cli  --eula accept \
	     --action install --components default  --install-dir ~/apps/oneapi
	hdiutil detach ~/mntdmg
	#
	echo "installing HPCKit"
	hdiutil attach "$hpc".dmg  -mountpoint ~/mntdmg -nobrowse
	sudo ~/mntdmg/bootstrapper.app/Contents/MacOS/install.sh --cli  --eula accept \
	     --action install --components default --install-dir ~/apps/oneapi
	hdiutil detach ~/mntdmg
	ls -lrta ~/apps ||true
	sudo rm -rf "$IONEAPI_ROOT"/intelpython "$IONEAPI_ROOT"/dal "$IONEAPI_ROOT"/advisor \
	     "$IONEAPI_ROOT"/ipp "$IONEAPI_ROOT"/conda_channel 	"$IONEAPI_ROOT"/dnnl \
	     "$IONEAPI_ROOT"/installer "$IONEAPI_ROOT"/vtune_profiler "$IONEAPI_ROOT"/tbb || true
	$GITHUB_WORKSPACE/travis/fix_xcodebuild.sh
	sudo cp xcodebuild "$IONEAPI_ROOT"/compiler/latest/mac/bin/intel64/.
	source "$IONEAPI_ROOT"/setvars.sh || true
	ifort -V
	icc -V
	# get user ownership of /opt/intel to keep caching happy
	my_gr=`id -g`
	my_id=`id -u`
	sudo chown -R $my_id /opt/intel
	sudo chgrp -R $my_gr /opt/intel
	if [[ "$MPI_IMPL" == "mpich" ]]; then
	    brew install hwloc
	fi
        ;;
    Linux)
	export APT_KEY_DONT_WARN_ON_DANGEROUS_USAGE=1
	export TERM=dumb
	wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | gpg --dearmor | sudo tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null
	echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/oneapi all main" | sudo tee /etc/apt/sources.list.d/oneAPI.list
	sudo apt-get update

	if [[ "$MPI_IMPL" == "intel" ]]; then
	    mpi_bin="intel-oneapi-mpi" ; mpi_libdev="intel-oneapi-mpi-devel" scalapack_libdev=" "
	fi
	echo pkg to install:  $mpi_libdev $mpi_bin 
        tries=0 ; until [ "$tries" -ge 10 ] ; do \
		      sudo apt-get -y install gfortran make perl rsync $mpi_libdev $mpi_bin $pkg_extra \
			  && break ;\
		      tries=$((tries+1)) ; echo attempt no.  $tries    ; sleep 30 ;  done

	sudo apt-get install -y intel-oneapi-compiler-fortran intel-oneapi-mkl intel-oneapi-compiler-dpcpp-cpp  libfabric-bin libnuma1
	if [[ "$?" != 0 ]]; then
	    df -h
	    echo "intel-oneapi-compiler-fortran install failed: exit code " "${?}"
	    exit 1
	fi
        source "$IONEAPI_ROOT"/setvars.sh || true
	FC=ifx
	export I_MPI_F90="$FC"
	export I_MPI_F77="$FC"
	"$FC" -V ; if [[ $? != 0 ]]; then echo "Intel SW install failed"; exit 1; fi
	icx -V
	sudo rm -rf $MKLROOT/lib/*sycl* || true
	which mpif90
	mpif90 -show
esac
which ifx
ifx -V
echo ""##### end of  install-intel.sh ####"
