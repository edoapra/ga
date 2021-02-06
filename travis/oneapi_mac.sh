#!/bin/bash
cd ~/Downloads
curl -LJO https://registrationcenter-download.intel.com/akdlm/irc_nas/17426/m_BaseKit_p_2021.1.0.2427_offline.dmg
curl -LJO https://registrationcenter-download.intel.com/akdlm/irc_nas/17398/m_HPCKit_p_2021.1.0.2681_offline.dmg
ls -lrt
#
hdiutil mount m_BaseKit_p_2021.1.0.2427_offline.dmg
/Volumes/m_BaseKit_p_2021.1.0.2427_offline/bootstrapper.app/Contents/MacOS/install.sh --cli \
--eula accept --components all --action install
umount /Volumes/m_BaseKit_p_2021.1.0.2427_offline
#
hdiutil mount  m_HPCKit_p_2021.1.0.2681_offline.dmg
/Volumes/m_HPCKit_p_2021.1.0.2681_offline/bootstrapper.app/Contents/MacOS/install.sh --cli \
--eula accept --components all --action install
umount /Volumes/m_HPCKit_p_2021.1.0.2681_offline

