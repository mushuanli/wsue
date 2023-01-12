======================================================================= WIFI problem - WPA2 Enterprise: 
```
apt purge wpasupplicant ; dpkg -i  wpasupplicant_2.9.0-21_amd64.deb
apt-mark hold wpasupplicant
nmcli connection add   type wifi   connection.id ap2000 ifname wlan0   wifi.ssid AP2000   wifi.mode infrastructure   wifi-sec.key-mgmt wpa-eap   802-1x.eap peap   802-1x.identity rain_li   802-1x.phase2-auth mschapv2   802-1x.password "$1"
nmcli --ask c up ap2000  
```

cert error:
wget debian-archive-keyring and dpkg -i to install it

debootstrap sid /mnt/root/ http://mirrors.163.com/debian

cd /mnt/root/
mount -o bind /dev/ c/dev/
mount -t devpts pts dev/pts/
mount -t tmpfs none tmp/
mount -t proc proc/
mount -t sysfs sys sys/

chroot .
apt-get update
dpkg-reconfigure tzdata 
apt-get install locales ssh linux-image-amd64 grub-pc net-tools bzip2
dpkg-reconfigure locales 


vi /etc/fstab
============ add =============================
/dev/sda1       /       ext4 noatime,nodiratime,relatime,errors=remount-ro 0 1
===============================================

vi /etc/resolv.conf

copy config file and others.
cp -a /etc/envirienment /etc/bash.bashrc /etc/tmux.conf /mnt/root/etc
cp -a /root/.profile /root/.ssh  /mnt/root/root/
copy /etc/vim/vim.local 
copy /etc/sysctl.conf

vi /etc/network/interfaces
copy /etc/wpa_supplicant/wpa_supplicant.conf      # wifi
copy /etc/network/interfaces                      # wifi + eth , NetworkManager not well support Enterprise encrypt

============ add =============================
auto lo
iface lo inet loopback

auto eth0
iface eth0 inet dhcp

===============================================

passwd
adduser
#  如果忘记，那么修改 grub菜单，在 linux 后面加上 init=/bin/bash 进入bash，然后 mount -o remount,rw / ; 然后 passwd 修改, 并使用 adduser 增加新用户

vi /boot/grub/grub.cfg
  # add net.ifnames=0 to makesure eth0 exist
        linux   /boot/vmlinuz-4.5.0-2-amd64 root=UUID=8706990a-3eab-4f71-b8e3-2383de8295dd ro net.ifnames=0 quiet
or vi etc/default/grub
   GRUB_CMDLINE_LINUX_DEFAULT="net.ifnames=0 quiet"

# install other tools (service, debug, tookit,wifi)
apt install mc wpasupplicant bind9-dnsutils lftp vim vim-runtime vim-common ssh samba wpasupplicant git tmux fzf rg jq tig ripgrep fd-find htop links2 xzip tcpdump ncdu ncat wget gawk sed rsync 
# install other packages 
apt-get install tmux bash-completion mc vim less ncftp curl  aria2 ssh tig git tcpdump lshw lsof  iftop htop iotop tcpstat dstat sysstat psmisc rsync file host dnsutils   schroot gcc make 

# fail2ban, mtr, clamav, chrootkit, els
# local tool: sniff parse_strace2.pl 

echo "source /etc/bash_completion" >> ~/.bashrc
echo "alias ..='cd ..' >> ~/.bashrc
echo '"\e[A": history-search-backward' >> inputrc
echo '"\e[B": history-search-forward' >> inputrc
update-alternatives --set editor /usr/bin/vim.basic




1. time
   当debian与windows共存时就会出现时间不一致情况，为避免此问题，可以设置debian不使用UTC时间，即直接将BIOS时间作为系统时间，执行如下命令并重启系统即可：
hwclock -w --localtime
命令修改了/etc/adjtime文件，不建议手动修改此文件。
 dpkg-reconfigure tzdata  或 ztselect
 date -s '20160315 21:59"
 hwclock -w
 
 
 
 2. grub
 sudo fdisk -l
        linux   /boot/vmlinuz-4.5.0-2-amd64 root=UUID=8706990a-3eab-4f71-b8e3-2383de8295dd ro net.ifnames=0 quiet

I have only one disk so that is /dev/sda and I learned that the Linux-formatted partition is /dev/sda4. That is what counts now. Then it is time to mount the necessary things:

sudo mount /dev/sda4 /mnt
sudo mount --bind /dev /mnt/dev
sudo mount --bind /proc /mnt/proc
sudo mount --bind /sys /mnt/sys

Shaun did not mention the fourth line, which is necessary for the GRUB2 to re-find the boot entries. I found it by reading the error message :-) 
Next let us chroot :

sudo chroot /mnt

From now on sudo is not necessary, I am the Root. Then it is time to reconfigure GRUB2:

update-grub2

The following lines indicate that all the necessary partitions are located by GRUB. The only thing left here is to write the GRUB to the MBR of my disk:

grub-install /dev/sda

That's it. Now leave the chroot environment:

exit

and reboot the system. The GRUB2 wakes up nicely and gives the first option as Debian as desired. If for some reason I wanted to change the boot order to, say, make the W7 be the default, I had to edit the GRUB2 configuration file. Once in chroot and updated GRUB, edit the configuration file with for example Nano:

nano -w /etc/default/grub

The same applies when modifying timeouts, defaults etc.later, too. The changes get written to the MBR by 

grub-install /dev/sda

---------------------------------------------------------------------------------------------
apt-get update hash sum mismatch problem:
apt-get clean
rm -rf /var/lib/apt/lists/*
apt-get clean
apt-get update
apt-get upgrade

---------------------------------------------------------------------------------------------
FFMPEG
Flip video  vertically:
ffmpeg -i INPUT -vf vflip -c:a copy OUTPUT
2. Flip video horizontally:

ffmpeg -i INPUT -vf hflip -c:a copy OUTPUT
3. Rotate 90 degrees clockwise:

ffmpeg -i INPUT -vf transpose=1 -c:a copy OUTPUT
4. Rotate 90 degrees counterclockwise:
ffmpeg -i INPUT -vf transpose=2 -c:a copy OUTPUT
ffmpeg -ss 5.32 -i input.mp4 -c:v libx264 -c:a libfaac out.mp4

---------------------------------------------------------------------------------------------
openssl req -x509 -nodes -newkey rsa:2048 -keyout key.pem -out cert.pem -days  300


---------------------------------------------------------------------------------------------
create USB BOOT and include DEBIAN Live
1. fdisk to create USB's boot partition, and set as FAT32
   windows only regornize first partition, so we often set as:
    /dev/sdb1   fat32  xxG
    /dev/sdb2   fat32/ext4  2G  active
    /dev/sdb3   ext4
    
2. format USB's boot partition
   mkfs.vfat -F 32 -n MULTIBOOT /dev/sdb2
   or
   mkfs.ext4 /dev/sdb2
3. Install Grub2 on the USB Flash Drive(/dev/sdb2):
   mkdir /mnt/USB && mount /dev/sdb2 /mnt/USB
   grub-install --force --no-floppy --boot-directory=/mnt/USB/boot /dev/sdb
   cd /mnt/USB/boot/grub  

4. set /mnt/USB/boot/grub/grub.cfg, notice, need to change UUID 
   this file is changed from ( wget pendrivelinux.com/downloads/multibootlinux/grub.cfg ).
   
cat << EOF > /mnt/USB/boot/grub/grub.cfg
# This grub.cfg file was created by Lance http://www.pendrivelinux.com
# Suggested Entries and the suggestor, if available, will also be noted.

set timeout=10
set default=0

insmod ext2

set debianpart=5b1f65f0-4259-46eb-af1e-81034d0be592
set bootpart=8ac2892d-9358-406b-a078-01f49657703b

#menuentry "USB Linux" {
#insmod ext4
#set root=(hd0,5)
#multiboot /boot/grub/core.img
#}

menuentry "Debian on USB" {
   insmod ext2
   search --no-floppy --fs-uuid --set=root
   configfile /boot/grub/grub.cfg
}


menuentry "Debian Live 8.4 ISO" {
    insmod ext2
    loopback loop /iso/debian-live-8.4.0-amd64-standard.iso
    linux (loop)/live/vmlinuz boot=live findiso= config quiet splash
    #linux (loop)/live/vmlinuz boot=live findiso=/iso/debian.iso config memtest noapic noapm nodma nomce nolapic nomodeset nosmp nosplash
    initrd (loop)/live/initrd.img
}

menuentry 'Boot Xiaoma2013 PE ISO'{
        search --no-floppy --fs-uuid --set=root
        #set root='(hd0,msdos1)'
        echo 'Loading Memdisk...'
        #insmod memdisk
        linux16 /iso/memdisk iso raw
        echo 'Loading ISO...'
        initrd16 /iso/XMPE2013.iso
}

menuentry 'Boot Win10x64 PE ISO'{
        search --no-floppy --fs-uuid --set=root
        #set root='(hd0,msdos1)'
        echo 'Loading Memdisk...'
        #insmod memdisk
        linux16 /iso/memdisk iso raw
        echo 'Loading ISO...'
        initrd16 /iso/Win10PE_amd64.iso
}


menuentry 'Boot AMpe Win8PE x64 ISO'{
        search --no-floppy --fs-uuid --set=root
        #set root='(hd0,msdos1)'
        echo 'Loading Memdisk...'
        #insmod memdisk
        linux16 /iso/memdisk iso raw
        echo 'Loading ISO...'
        initrd16 /iso/AmPe_v7.1_x64.iso
}


menuentry "Memtest 86+" {
 linux16 /memtest86+.bin
}

menuentry "SystemRescueCd" {
 loopback loop /systemrescuecd.iso
 linux (loop)/isolinux/rescuecd isoloop=/systemrescuecd.iso setkmap=us docache dostartx
 initrd (loop)/isolinux/initram.igz
}

menuentry "Local linux (grub2 install in seperate partition)" {
   insmod ext2
   search --no-floppy --set=root --file /grub/grub.cfg
   configfile /grub/grub.cfg
}


menuentry "Local Windows  (on /dev/sda1)" {
  insmod part_msdos
  insmod ntfs
  search --no-floppy --set=root --file /bootmgr
  #set root='(hd1,msdos1)'
  chainloader +1
}

EOF

/iso/memdisk is copy from syslinux
5. copy debian live cd to /mnt/USB/iso/debian.iso
6. install debian on /dev/sdb3, notice:
  a. grub2 for usb debian need to install on debian's root partition(/dev/sdb3)
  b. change /etc/fstab use UUID
  
now it works, great :)


====================================== XRDP =======================================
apt-get install xserver-xorg-core xserver-xorg-input-libinput xserver-xorg-input-evdev
apt-get install xserver-xorg-core xserver-xorg-input-libinput xserver-xorg-input-evdev
apt-get install xserver-xorg xserver-xorg-input-libinput xserver-xorg-input-evdev

apt-get remove xrdp vnc4server tightvncserver
 apt-get install tightvncserver xrdp
 apt-get install xrdp
 service xrdp restart
adduser xrdp ssl-cert  

touch /var/log/xrdp.log
chown xrdp:adm /var/log/xrdp.log
chmod 640 /var/log/xrdp.log
systemctl start xrdp
systemctl status xrdp

problem:
```
1. 闪退：进入用户目录
   touch .xsession
   echo xfce4-session > .xsession 
2. 远程使用Terminal时，Tab键无法自动补全 - 使用 ctrl-i, 或是在窗口管理器中keyboard 里将 用到 Super + Tab的快捷键clear掉即可
3. xrdp_mm_process_login_response: login failed
   原因：没有正确关闭导致达到连接上限。修改 /etc/xrdp/sesman.ini ,  MaxSessions=50 , KillDisconnected=1 , 然后重启 xrdp
4. /var/log/xrdp.log : 
   password failed
   error - problem connecting
   a. var/log/xrdp-sesman.log 
      [ERROR] X server for display 10 startup timeout[INFO ] starting xrdp-sessvc - xpid=2924 - wmpid=2923
      或
      [ERROR] another Xserver is already active on display 10
      那么是 tightvnc 字体冲突， apt purge tightvncserver xrdp; apt install tightvncserver xrdp
   b. 不是什么情况,修改 /etc/xrdp/sesman.ini
      param8=-SecurityTypes
      param9=None

```

      


--------------------------------------- WIFI -------------------------------------------------
安装：
firmware-iwlwifi  (需要打开no-free)  wireless-tools wpasupplicant (支持 wap 认证)
intel 驱动如果不成功，需要另外下载：
驱动首页:
https://wireless.wiki.kernel.org/en/users/drivers/iwlwifi
下载对应的
wget https://wireless.wiki.kernel.org/_media/en/users/drivers/iwlwifi-8265-ucode-22.361476.0.tgz
然后 ucode 放到 /lib/firmware/
再重加载 iwlwifi 模块

启动(/etc/network/interfaces 添加下面行)：
auto wlan0
iface wlan0 inet dhcp
pre-up wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant.conf
post-down killall -q wpa_supplicant

同时 /etc/wpa_supplicant/wpa_supplicant.conf 添加下面行：
ctrl_interface=/var/run/wpa_supplicant
network={
  ssid="Trend-BYOD"
  scan_ssid=1
  key_mgmt=WPA-EAP
  pairwise=CCMP TKIP
  group=CCMP TKIP
  eap=PEAP
  identity="your-username"
  password="your-password"
  #ca_cert="/etc/certs/radius.pem"
  phase1="peapver=0"
  phase2="MSCHAPV2"
  }


然后使用 ifup wlan0 启动


============ hashcat restore office password =============================
 1. gen office hash value:
    download https://github.com/stricture/hashstack-server-plugin-hashcat/blob/master/scrapers/office2hashcat.py or https://github.com/magnumripper/JohnTheRipper/blob/bleeding-jumbo/run/office2john.py
     python office2john.py dummy.docx  | sed 's/^.*://' | tee hash.txt
 
 2. run hashcat in docker:
    hashcat is dependcy with OS, so run in docker is a good idea.
    docker pull dizcza/docker-hashcat:intel-cpu
    docker run -it -e TZ=Asia/Shanghai  -v /home/build/rain/:/n dizcza/docker-hashcat:intel-cpu  /bin/bash
    
 2. In hashcat container, find password with rules:
 rules ref: https://blog.51cto.com/simeon/2084325
 hashcat -m 9600 hash.txt -a 3 --force --increment --increment-min 1 --increment-max 8 ?d?d?d?d?d?d?d?d
 
