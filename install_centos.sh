
#   install an centos chroot system
#   this command can run in centos/debian
#   first,
#   linux if meet "warning: rpmts_HdrFromFdno: Header V3 RSA/SHA256 Signature, key ID fd431d51: NOK ' problem
#   1.  cd /etc/pki/rpm-gpg/ 
#   2.  wget http://mirrors.163.com/centos/RPM-GPG-KEY-CentOS-6 
#   3.  rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-CentOS-6 
#   
CHROOT_DIR=/home/centos
SCHROOT_NAME=c6
SCHROOT_DESC='Centos 6 (amd64)'
CENTOS_BASE=centos-release-6-9.el6.12.3.x86_64.rpm

if [ !-f /etc/pki/rpm-gpg/RPM-GPG-KEY-CentOS-6 ] ; then
mkdir -p /etc/pki/rpm-gpg/ && cd /etc/pki/rpm-gpg/ && wget http://mirrors.163.com/centos/RPM-GPG-KEY-CentOS-6 && rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-CentOS-6 
fi

#   1.  recreate rpm directory
rm -rf  $CHROOT_DIR
mkdir -p $CHROOT_DIR/var/lib/rpm
  
#   2.  special rpm rebuild dir
rpm --rebuilddb --root=$CHROOT_DIR
  
#   3.  download system install package
if [ -f $CENTOS_BASE ] ; then
    echo "$CENTOS_BASE file exist, skip download"
else
    wget http://mirrors.163.com/centos/6.9/os/x86_64/Packages/$CENTOS_BASE
fi

echo "install base system"
#   4.  install base system
rpm -ivh --root=$CHROOT_DIR --nodeps $CENTOS_BASE
  
echo "install rpm-build yum,run command:"
echo "yum --installroot=$CHROOT_DIR install -y rpm-build yum"
#   5.  install rpmbuild/yum to dest system
yum --installroot=$CHROOT_DIR install -y rpm-build yum bc net-tools wget vim
  
echo "prepare chroot directory"
#   6.  prepare chroot system
mkdir -p $CHROOT_DIR/proc
mkdir -p $CHROOT_DIR/dev
mkdir -p $CHROOT_DIR/sys
cp /etc/resolv.conf $CHROOT_DIR/etc/
echo "if [ -f ~/.bashrc ]; then . ~/.bashrc; fi" > $CHROOT_DIR/root/.bash_profile
echo "PS1=\'\033[1;93;45m${debian_chroot:+($debian_chroot)}\033[0m\[\033[01;32m\]\u@\[\033[01;34m\]\w\[\033[00m\]\$ \'" >> $CHROOT_DIR/root/.bashrc
  
#   7.  now you can chroot,but if you use chroot command, you need to mount --rbind /proc /sys/ /dev
#       or you can use schroot -c $SCHROOT_NAME to run
#chroot $CHROOT_DIR /bin/bash –l


#   echo schroot comfig command, use these command can direct add config to schroot, then use schroot to start new system
echo "if you use schroot, run these command in dest linux system"
echo "
export CHROOT_DIR=$CHROOT_DIR
export SCHROOT_NAME='$SCHROOT_NAME'
export SCHROOT_DESC=$SCHROOT_DESC
(
cat <<EOF
[$SCHROOT_NAME]
description=$SCHROOT_DESC
directory=$CHROOT_DIR
root-users=root
root-groups=root
type=directory
aliases=unstable,default
EOF
) >> /etc/schroot/schroot.conf
"


------------------------------------------------------------------------------
#  kickstart show dialog

%pre
curTTY=`tty`
exec < $curTTY > $curTTY 2> $curTTY
clear

DATE=`date '+%Y-%m-%d %H:%M:%S'`
echo $DATE
whiptail --title "Check BIOS Clock Dialog" --yesno "Current BIOS clock is :\n    $DATE\nIf it wrong,  You need to reboot to change it before install." 24 78 --yes-button "Continue" --no-button "Reboot"
if [ $? = 0 ] ; then
    echo "Will reboot..."
    reboot
fi


%end

