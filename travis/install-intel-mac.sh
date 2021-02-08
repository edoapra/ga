#!/bin/bash
#!/bin/bash
mkdir -p ~/mntdmg || true
cd ~/Downloads
curl -LJO https://registrationcenter-download.intel.com/akdlm/irc_nas/17426/m_BaseKit_p_2021.1.0.2427_offline.dmg
curl -LJO https://registrationcenter-download.intel.com/akdlm/irc_nas/17398/m_HPCKit_p_2021.1.0.2681_offline.dmg
ls -lrt
#
echo "installing BaseKit"
hdiutil attach m_BaseKit_p_2021.1.0.2427_offline.dmg  -mountpoint ~/mntdmg -nobrowse
df 
#sudo ~/mntdmg/bootstrapper.app/Contents/MacOS/install.sh --cli \
#     --eula accept --components default --action install
sudo ~/mntdmg/bootstrapper.app/Contents/MacOS/install.sh --cli  --eula accept \
 --action install --components custom:\
intel.oneapi.mac.basekit.getting_started:\
intel.oneapi.mac.oneapi-common.vars:\
intel.oneapi.mac.oneapi-common.licensing:\
intel.oneapi.mac.dev-utilities:\
intel.oneapi.mac.tbb.runtime:\
intel.oneapi.mac.openmp:\
intel.oneapi.mac.mkl.runtime:\
intel.oneapi.mac.mkl.devel:\
intel.oneapi.mac.ipp.runtime:\
intel.oneapi.mac.basekit.product
sudo cat /opt/intel/oneapi/logs/* || true
hdiutil detach ~/mntdmg
sudo du -sh /opt/intel
#
echo "installing HPCKit"
hdiutil attach m_HPCKit_p_2021.1.0.2681_offline.dmg  -mountpoint ~/mntdmg -nobrowse
df
#sudo ~/mntdmg/bootstrapper.app/Contents/MacOS/install.sh --cli \
# --eula accept --components default --action install
sudo ~/mntdmg/bootstrapper.app/Contents/MacOS/install.sh --cli  --eula accept \
 --action install --components custom:\
intel.oneapi.mac.hpckit.getting_started:\
intel.oneapi.mac.oneapi-common.vars:\
intel.oneapi.mac.oneapi-common.licensing:\
intel.oneapi.mac.openmp:\
intel.oneapi.mac.compilers-common:\
intel.oneapi.mac.tbb.runtime:\
intel.oneapi.mac.cpp-compiler:\
intel.oneapi.mac.ifort-compiler:\
intel.oneapi.mac.dev-utilities:\
intel.oneapi.mac.hpckit.product
sudo cat /opt/intel/oneapi/logs/* || true
hdiutil detach ~/mntdmg
df
ls -lrt /opt ||true
ls -lrt /opt/intel/oneapi ||true
source /opt/intel/oneapi/setvars.sh || true
# get user ownership of /opt/intel to keep caching happy
my_gr=`id -g`
my_id=`id -u`
sudo chown -R $my_id /opt/intel
sudo chgrp -R $my_gr /opt/intel
ifort -V || true
icc -V || true
