#!/bin/bash
# Code 'n Load device default configuration.
# Copyright (C) 2018  Pablo Ridolfi - Code 'n Load
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Default values
CNL_APP_MONITOR_TIME=30
CNL_APP_URL="https://github.com/codenload/cnl_app.git"

echo "****************************"
echo "*** Checking hostname... ***"
echo "****************************"
if [[ ($HOSTNAME = "raspberrypi") || ($HOSTNAME = "bananapi") || ($HOSTNAME = "beagleboard") ]]
then
	echo "*** This hostname ($HOSTNAME) is NOT recommended. You should choose a unique name for this device."
	echo "*** If you choose 'Yes' this device will reboot in order to update its hostname, then run 'make install' again."
	echo "*** Change hostname for this device? "
	select yn in "Yes" "No"; do
	    case $yn in
	        Yes ) sudo raspi-config || sudo armbian-config; sudo reboot now; break;;
	        No ) break;;
	    esac
	done
fi

echo "*********************"
echo "*** Configuration ***"
echo "*********************"
echo "*** Backing-up /etc/cnl.conf to /etc/cnl.conf.old"
sudo mv /etc/cnl.conf /etc/cnl.conf.old

echo "*** Setting CNL_PATH=$PWD"
sudo sh -c "echo 'CNL_PATH=$PWD' >> /etc/cnl.conf"

echo "*** Setting CNL_APP_URL=$CNL_APP_URL"
sudo sh -c "echo 'CNL_APP_URL=$CNL_APP_URL' >> /etc/cnl.conf"

repo_path="$HOME/cnl_app"
sudo rm -rf $repo_path
echo "*** Setting CNL_APP_PATH=$repo_path"
sudo sh -c "echo 'CNL_APP_PATH=$repo_path' >> /etc/cnl.conf"

repo_time=$CNL_APP_MONITOR_TIME
echo "*** Setting CNL_APP_MONITOR_TIME=$repo_time"
sudo sh -c "echo 'CNL_APP_MONITOR_TIME=$repo_time' >> /etc/cnl.conf"

echo "********************************************************"
echo "*** Generate SSH keys.                               ***"
echo "*** If you want to keep the current key answer 'no'. ***"
echo "********************************************************"
ssh-keygen -f "$HOME/.ssh/id_rsa.cnl"
echo "*** Setting CNL_SSH_KEY=$HOME/.ssh/id_rsa.cnl"
sudo sh -c "echo 'CNL_SSH_KEY=$HOME/.ssh/id_rsa.cnl' >> /etc/cnl.conf"
export CNL_SSH_KEY=$HOME/.ssh/id_rsa.cnl

echo "*************************************"
echo "*** Copying systemd unit files... ***"
echo "*************************************"
sudo cp etc/cnld.socket etc/cnld.service etc/cnl_app.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl stop cnld
sudo cp out/cnld /usr/bin/
sudo cp etc/cnl /usr/bin/
echo "****************************"
echo "*** Starting services... ***"
echo "****************************"
sudo systemctl enable cnld.service
sudo systemctl enable cnl_app.service
sudo systemctl start cnld.service
echo "********************************************************"
echo "*** Installing default application (blink GPIO21)... ***"
echo "********************************************************"
sudo systemctl stop cnl_app.service
cnl clone
cnl update
echo "*************"
echo "*** Done. ***"
echo "*************"
echo "*** Do you want to register this device? "
select yn in "Yes" "No"; do
    case $yn in
        Yes ) ./etc/register.sh; break;;
        No ) exit;;
    esac
done
