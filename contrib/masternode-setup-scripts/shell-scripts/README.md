### BEFORE USING THESE SCRIPTS (PRE-SETUP)
Open your notepad or any text editor application on your pc and write this down as your ``Cheat Sheet``
```
1. MASTERNODE NAME = MN1
2. COLLATERAL = 500000
3. STEALTH ADDRESS = 41jasVJqZKegd5pAWopYUrU61CJ4gdmdaUpWRCBnrRn9fFFeM2eP54vdgDEcF6zVF43U9CBjgFFywShiA8Lu1Mr91A7DL3L8vtQ
4. MASTERNODE GENKEY = 8799TqpXKkMkQ7Wyan7FueUk5pa2dydnhUYu5dpjeixvfh3k9No
5. MASTERNODE OUTPUTS = d9ab6b76b0b6596e0fe64c80dcb709bd68e070465f9b65af637c1e702c93c122 0
6. UNIQUE IP OF THE VPS = 56.56.65.20
```

### GETTING A VPS (STEP 1)
Set up your VPS, we recommend [VULTR](https://www.vultr.com/?ref=8638319), and select ``DEPLOY INSTANCE`` then select the following
- Cloud compute
- Location -any
- Server type: Ubuntu 18.04
- Server size: 1GB $5/month
- Add your desired hostname and label
- Click DEPLOY
Note: The server will take a few minutes to deploy and will then shows as "running" in your "instances" section.

### QT WALLET CONFIGURATION (STEP 2)
1. Open your ``QT WALLET`` and access it with your password.
2. Open your ``debug console`` or press ``F1`` key on your keyboard and type the following comamnd
	```
	masternode genkey
	```
	- Copy the generated key from your ``debug console`` and open your ``Cheat Sheet`` then paste the generated key on ``4. MASTERNODE GENKEY`` (8799TqpXKkMkQ7Wyan7FueUk5pa2dydnhUYu5dpjeixvfh3k9No)

	- Copy your stealth address under the ``Receive`` tab (for example: 41jasVJqZKegd5pAWopYUrU61CJ4gdmdaUpWRCBnrRn9fFFeM2eP54vdgDEcF6zVF43U9CBjgFFywShiA8Lu1Mr91A7DL3L8vtQ ) and paste it on your ``Cheat Sheet`` on ``3. STEALTH ADDRESS``

	- Copy the stealth address again from your ``Cheat Sheet`` under ``3. STEALTH ADDRESS`` and head over to your ``Send`` tab then paste the ``STEALTH ADDRESS`` on the ``Wallet Address`` area on the ``Send`` tab and input the ``2. MASTERNODE COLLATERAL`` which is ``500000`` XCX on the ``XCX Amount`` area then click ``Send`` and wait for ``6`` confirmations.

	- Then click ``Copy`` when a windows pops-up and head over to your ``debug console`` or press ``F1`` then type ``masternode outputs`` then press ``enter`` copy the ``txhash`` and paste it on your ``Cheat Sheet`` under ``5. MASTERNODE OUTPUTS`` ( d9ab6b76b0b6596e0fe64c80dcb709bd68e070465f9b65af637c1e702c93c122 ) and also don't forget to copy the ``outputidx`` ( 0 ) and paste it next to the ``txhash``.

	- Then head over to your ``Cheat Sheet`` 
		- input your ``VPS`` ip address ( 56.56.65.20 ) under ``6. UNIQUE IP OF THE VPS``
		- input your ``Masternode Name``  ( MN1 )under ``1. MASTERNODE NAME``

3. Head over to your ``encrypt`` directory
	- Windows: %APPDATA%/encrypt
	- Linux: ~/.encrypt
	- Mac: ~/Library/Application Support/encrypt
4. Open and edit ``masternode.conf`` file with your preferred Text Editor
	- Inside ``masternode.conf`` file is this lines of text
	```
	# Masternode config file
	# Format: alias IP:port masternodeprivkey collateral_output_txid collateral_output_index
	# Example: mn1 127.0.0.2:2020 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 0
	```
	- On line 4 add your ``1. MASTERNODE NAME`` which is ``MN1``, next is add your ``6. UNIQUE IP OF THE VPS`` which is ``56.56.65.20`` and add the respective default port of ``encrypt`` which is ``2020``,next is add your  ``4. MASTERNODE GENKEY`` which is ``8799TqpXKkMkQ7Wyan7FueUk5pa2dydnhUYu5dpjeixvfh3k9No``, and lastly add your ``5. MASTERNODE OUTPUTS``which is ``d9ab6b76b0b6596e0fe64c80dcb709bd68e070465f9b65af637c1e702c93c122`` then add your ``outputidx`` which is ``0`` next to your ``5. MASTERNODE OUTPUTS``.

	- It will look like this on the ``masternode.conf`` file

	```
	# Masternode config file
	# Format: alias IP:port masternodeprivkey collateral_output_txid collateral_output_index
	# Example: mn1 127.0.0.2:2020 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 2bcd3c84c84f87eaa86e4e56834c92927a07f9e18718810b92e0d0324456a67c 0
	MN1 56.56.65.20:2020 8799TqpXKkMkQ7Wyan7FueUk5pa2dydnhUYu5dpjeixvfh3k9No d9ab6b76b0b6596e0fe64c80dcb709bd68e070465f9b65af637c1e702c93c122 0
	```

### ACCESSING YOUR VPS (STEP 3)
1. Download a SSH Application Client (you can choose one below)
	- PuTTY (https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html)
	- Bitvise (https://www.bitvise.com/ssh-client-download) [RECOMMENDED]
2. Open Bitvise
	1. Enter your VPS ``ip address`` under ``Host`` on the Server area
	2. Enter the port number which is ``22`` under ``Port``
	3. Enter your VPS ``username`` under ``Username`` on the Authentication area
	4. Check the ``Store encrypted password in profile`` checkbox and enter your VPS ``password`` under ``Password``
	5. Then click ``Log in``

### DOWNLOADING THE SCRIPT ON YOUR VPS (STEP 4)
On your SSH Terminal type this lines below one at a time
```
git clone https://github.com/getdzypher/EncryptNetwork-Scripts enscripts
chmod -R 755 enscripts
cd enscripts/Masternodes
./Install.sh
```
Note: The script allows you to automatically install ``Encrypt`` from the ``Encrypt Network`` repository.

#### VPS WALLET CONFIGURATION (STEP 5)

```
rpcuser=someuserhere
rpcpassword=somepasswordhere
rpcallowip=127.0.0.1
server=1
daemon=1
listen=1
staking=0
logtimestamps=1
maxconnections=256
masternode=1
externalip=yourexternaliphere
masternodeprivkey=yourmasternodeprivkeyhere
```
1. Change these following lines on the bash file named ``Config.sh`` by following steps below via ``vim``

```
vi Config.sh
```
then 

- change the ``someuserhere`` value to your own
- change the ``somepasswordhere`` value to your own
- change the ``yourexternaliphere`` value to your own which is located on your ``Cheat Sheet`` (e.g. 56.56.65.20:2020)
- change the ``yourmasternodeprivkeyhere`` value to your own which is also located on your ``Cheat Sheet`` (e.g. 8799TqpXKkMkQ7Wyan7FueUk5pa2dydnhUYu5dpjeixvfh3k9No this is your ``MASTERNODE GENKEY``)
	(To save and exit the editor press ``Ctrl + C`` then type ``:wq!`` then press Enter)

2. Then open ``Config.sh`` file by typing ``./Config.sh``. 
Note: It will automatically change your ``encrypt.conf`` file located on the ``encrypt`` directory inputting all the text above.

### HOW TO UPDATE YOUR ENCRYPT DAEMON WITH A SCRIPT
Just run the ``Update.sh`` shell file. On your SSH Terminal type this line below
```
./Update.sh
```
Note: I will automatically updates your daemon



if you have question regarding to the scripts feel free to head over to ``Encrypt Network Discord Channel`` (https://discord.gg/JhYe8z)


# GREAT JOB! YOU CONFIGURED YOUR ENCRYPT MASTERNODE.
