#!/bin/sh
clear
echo "Starting Source Code Updater script"
echo "Deleting old source code..."
cd && sudo rm -rf Shroud
echo "Downloading latest source code..."
git clone https://github.com/ShroudXProject/Shroud
echo "Setting permissions..."
sudo chmod -R 755 Shroud
echo "Shroud Source Code Updated Successfully!"