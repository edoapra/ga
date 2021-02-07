#!/bin/bash
#!/bin/bash
mkdir -p ~/mntdmg || true
cd ~/Downloads
#curl -LJO https://registrationcenter-download.intel.com/akdlm/irc_nas/17426/m_BaseKit_p_2021.1.0.2427_offline.dmg
#curl -LJO https://registrationcenter-download.intel.com/akdlm/irc_nas/17398/m_HPCKit_p_2021.1.0.2681_offline.dmg
ls -lrt
#
echo "installing BaseKit"
hdiutil attach m_BaseKit_p_2021.1.0.2427_offline.dmg  -mountpoint ~/mntdmg -nobrowse
df 
sudo ~/mntdmg/bootstrapper.app/Contents/MacOS/install.sh --cli \
 --eula accept --components all --action install
hdiutil detach ~/mntdmg
#
echo "installing HPCKit"
hdiutil attach m_HPCKit_p_2021.1.0.2681_offline.dmg  -mountpoint ~/mntdmg -nobrowse
df
sudo ~/mntdmg/bootstrapper.app/Contents/MacOS/install.sh --cli \
 --eula accept --components all --action install
hdiutil detach ~/mntdmg
df
ls -lrt /opt ||true
ls -lrt /opt/intel/oneapi ||true
