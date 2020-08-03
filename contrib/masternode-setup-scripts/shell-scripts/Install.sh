#!/bin/sh
clear
echo "Starting Shroudnode auto download and install script"
echo "Updating/Upgrading OS..."
sudo apt update && sudo apt upgrade -y
echo "Downloading Shroud latest build..."
wget -N https://github.com/ShroudXProject/Shroud/releases/download/v1.2.1/shroud-1.2.1-x86_64-linux-gnu.tar.gz
echo "Extracting build..."
sudo tar -C /usr/local/bin -zxvf shroud-1.2.1-x86_64-linux-gnu.tar.gz
echo "Setting permissions..."
cd && sudo chmod +x /usr/local/bin/shroud*
sudo chmod +x /usr/local/bin/tor*
echo "Creating .shroud directory..."
mkdir ~/.shroud
cd ~/.shroud
echo "Setting up and enabling fail2ban..."
sudo apt-get install fail2ban -y
sudo ufw allow ssh
sudo ufw allow 42998
sudo ufw enable -y
echo "Launching shroudd..."
cd && cd /usr/local/bin
shroudd -daemon
echo "Cleaning up..."
cd && cd Shroud/contrib/masternode-setup-scripts/shell-scripts
rm -rf shroud-1.2.1-x86_64-linux-gnu.tar.gz
echo "Shroudnode Installed Successfully!"