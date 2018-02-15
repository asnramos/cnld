#!/bin/bash
# Code 'n Load device registration script.
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

echo "************************"
echo "*** Register device. ***"
echo "************************"
sudo sh -c "echo 'AUTOSSH_GATETIME=0' >> /etc/cnl.conf"

url_add_device="https://www.codenload.com/cnl/add_device.php"

ssh_local_port_default=`sudo cat /etc/ssh/sshd_config | grep -w Port | cut -d' ' -f2`
read -p "*** Please enter this device SSH server port ($ssh_local_port_default): " ssh_local_port
if [[ "$ssh_local_port" == "" ]]; then
	ssh_local_port=$ssh_local_port_default
fi
echo "*** Setting CNL_SSH_LOCAL_PORT=$ssh_local_port"
sudo sh -c "echo 'CNL_SSH_LOCAL_PORT=$ssh_local_port' >> /etc/cnl.conf"

read -p "*** Please enter your Code 'n Load username: " cnl_username
echo -n "*** Password: "
read -s cnl_password

cnl_app_url=`cnl config get CNL_APP_URL | cut -d'=' -f2`
app_monitor_time=`cnl config get CNL_APP_MONITOR_TIME | cut -d'=' -f2`
serial=`cnl serial`
pubkey=`cat $CNL_SSH_KEY.pub`
echo ""
response=$(sudo curl --silent --user $cnl_username:$cnl_password --data-urlencode "device_data=$HOSTNAME,$serial,$USER,$pubkey,$cnl_app_url,$app_monitor_time,$ssh_local_port" $url_add_device)

if [[ $response = *"Duplicate entry"* ]]; then
  echo "*** ERROR: device already registered ($response)."
elif [[ $response = *"ERROR"* ]]
then
  echo "*** ERROR registering device."
else
  server_url=`echo $response | cut -d',' -f2`
  echo "*** Setting CNL_SSH_SERVER_URL=$server_url"
  sudo sh -c "echo 'CNL_SSH_SERVER_URL=$server_url' >> /etc/cnl.conf"
  
  server_ssh_port=`echo $response | cut -d',' -f3`
  echo "*** Setting CNL_SSH_SERVER_PORT=$server_ssh_port"
  sudo sh -c "echo 'CNL_SSH_SERVER_PORT=$server_ssh_port' >> /etc/cnl.conf"
  
  server_ssh_user=`echo $response | cut -d',' -f4`
  echo "*** Setting CNL_SSH_USER=$server_ssh_user"
  sudo sh -c "echo 'CNL_SSH_USER=$server_ssh_user' >> /etc/cnl.conf"
  
  server_ssh_key=`echo $response | cut -d',' -f5`
  echo "*** Adding key to $HOME/.ssh/autorhized_keys"
  echo "$server_ssh_key" >> "$HOME/.ssh/authorized_keys"
  
  ssh_remote_port=`echo $response | cut -d',' -f6`
  echo "*** Setting CNL_SSH_REMOTE_PORT=$ssh_remote_port"
  sudo sh -c "echo 'CNL_SSH_REMOTE_PORT=$ssh_remote_port' >> /etc/cnl.conf"
  
  sudo mkdir /root/.ssh
  sudo ssh-keyscan -p$server_ssh_port $server_url | sudo tee -a /root/.ssh/known_hosts

  echo "*************************************"
  echo "*** Copying systemd unit files... ***"
  echo "*************************************"
  sudo cp etc/cnl_ssh.service /etc/systemd/system/
  sudo systemctl daemon-reload
  sudo systemctl stop cnl_ssh
  echo "****************************"
  echo "*** Starting services... ***"
  echo "****************************"
  sudo systemctl enable cnl_ssh.service
  sudo systemctl start cnl_ssh.service
  sudo systemctl restart cnld.service
  sudo systemctl restart cnl_app.service
  echo "*************"
  echo "*** Done. ***"
  echo "*************"
fi
