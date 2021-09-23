#!/bin/bash

echo "ZTE VPN LINUX CONNECT SCRIPT:"
read -p "Please input your VPN account: " USERNAME

if [ -z "$USERNAME" ] ; then
echo "ERROR: no VPN Account, exit."
exit 1
fi

read -s -p "Please input $USERNAME 's Password: " PASSWORD

if [ -z "$PASSWORD" ] ; then
echo "ERROR: no VPN Password, exit."
exit 1
fi

killall wpa_supplicant 2>/dev/null
ifdown  eth0 2>/dev/null

(
cat <<EOF

ctrl_interface=/var/run/wpa_supplicant
ctrl_interface_group=root
ap_scan=0
network={
   key_mgmt=IEEE8021X
   eap=PEAP
   phase1="peaplabel=0"
   phase2="auth=GTC"
   identity="$USERNAME"
   password="$PASSWORD"
}
EOF
) > /tmp/wpa_supplicant.conf

wpa_supplicant  -d -B -i eth0 -c /tmp/wpa_supplicant.conf -D wired

rm -f /tmp/wpa_supplicant.conf

echo "up ETH0"
ifup    eth0
sleep 3
dhclient eth0
