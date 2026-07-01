filename: ooma-readme.txt 

From TD-721

This description is for Devhost4, Unbuntu-18.04 based host.

You may need dependent packages, do this:

sudo apt install python bluez jq
sudo pip install pexpect

You need a Telo with BLE, for example TeloAir or Cygnus board

You need a BT-4.0 Dongle installed on the devhost

Check to see if dongle installed on host:
ls /sys/class/bluetooth/
hci0

hci0, for example, is the bluetooth device we will use.  Your devhost may 
have additional BT interfaces.  You need a BT-4.0+ specifically to be able
to send and receive BLE (Bluetooth low-energy packets)

Check to see if bluetooth is available on Telo
ls /sys/class/bluetooth/
hci0

Now, on Telo again, get the bdinfo value (macid equivalent for Bluetooth)
hciconfig hci0
hci0:   Type: Primary  Bus: USB
        BD Address: A8:1D:16:82:72:74  ACL MTU: 1021:7  SCO MTU: 240:3
        UP RUNNING 
        RX bytes:37250 acl:12 sco:0 events:881 errors:0
        TX bytes:2321 acl:11 sco:0 commands:118 errors:0

Make the helper writeable
chmod 777 gatt-client.py 
  
Edit gatt-client.py
from 
DEVICE = "28:C2:DD:A0:33:A0"
to (use bdinfo from your device from above)
DEVICE = "A8:1D:16:82:72:74"

Run tester
./gatt-client.py

To add a new request, add to reqArray


    
      
