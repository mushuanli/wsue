#!/bin/bash

#
# script used to download microk8s pkg to offline install, look the end of file to view how to use
#
# set -x -u
#shfmt -i 2 -ci -w firmware/microk8s/prepare.sh

FAKEIP="192.168.192.3/30"
FAKEGW="192.168.192.2"
FAKECNI="198.18.0.0/16"
FAKEDNS="10.204.16.18"

declare -a SNAP_PKGS=("snapd" "core18" "microk8s")

MICROK8S_CHANNEL=1.23
MICROK8S_IMAGES=(
docker.io/calico/cni:v3.19.1
docker.io/calico/kube-controllers:v3.17.3
docker.io/calico/node:v3.19.1
docker.io/calico/pod2daemon-flexvol:v3.19.1
docker.io/cdkbot/hostpath-provisioner:1.1.0
docker.io/coredns/coredns:1.8.0
docker.io/library/registry:2.7.1
k8s.gcr.io/ingress-nginx/controller:v1.2.0
k8s.gcr.io/metrics-server/metrics-server:v0.5.2
k8s.gcr.io/pause:3.1
)

#   @desc:  download    microk8s source to prepare for offline install
function prepare_image() {


  cd microk8s_sources

  local i
  local name
  for i in "${MICROK8S_IMAGES[@]}"; do
    name=$(echo $i | awk -F: '{print $1}' | awk -F "/" '{print $NF}')
    name="$name.tar"
    echo "=======     $name = $i"
    rm -f "$name"
    #docker pull $i
    #docker save $i -o $name
    microk8s.ctr i pull $i
    microk8s.ctr i export $name $i
    if [ $? -ne 0 ]; then
      echo "FAIL EXEC: $i => $name"
      return 1
    fi
    echo ""
  done

  echo "prepare ALL image success"
  cd ..
  return 0
}

#   @desc:  download snap package
function prepare_pkg() {
  cd microk8s_sources
  for i in "${SNAP_PKGS[@]}"; do
    echo "=======     $i"
    if [ "$i" == "microk8s" ] ; then
      snap download $i --channel=$MICROK8S_CHANNEL/stable
    else
      snap download $i --channel=latest/stable
    fi
    if [ $? -ne 0 ]; then
      echo "FAIL EXEC: $i "
      return 1
    fi
  done

  echo "prepare ALL pkg success"
  cd ..
  return 0
}

function prepare_online_init() {
  if grep -q biosdevname /proc/cmdline; then
    echo "skip rename eth devname"
    if nmcli dev show eth0 | grep IP4.ADDRESS; then
      echo "ip addr already set, skip"
    else
      echo "init ip address"
      nmcli --fields DEVICE,UUID con show | grep -w eth0 | awk '{print $2}' | xargs nmcli conn delete
      nmcli con add con-name eth0 autoconnect yes type ethernet ifname eth0 ip4 $FAKEIP gw4 $FAKEGW ipv4.dns $FAKEDNS
      nmcli con up eth0
    fi
    return 0
  fi

  #   https://zhuanlan.zhihu.com/p/557746438
  systemctl disable --now firewalld
  setenforce 0
  sed -i 's#SELINUX=enforcing#SELINUX=disabled#g' /etc/selinux/config
  sed -ri 's/.*swap.*/#&/' /etc/fstab
  swapoff -a && sysctl -w vm.swappiness=0

  cat >/etc/NetworkManager/conf.d/calico.conf <<EOF
[keyfile]
unmanaged-devices=interface-name:cali*;interface-name:tunl*
EOF
  systemctl restart NetworkManager

  ulimit -SHn 65535
  grep -q '* hard nofile 131072' /etc/security/limits.conf
  if [ $? -ne 0 ]; then
    cat >>/etc/security/limits.conf <<EOF
* soft nofile 655360
* hard nofile 131072
* soft nproc 655350
* hard nproc 655350
* seft memlock unlimited
* hard memlock unlimitedd
EOF
  fi

  cat <<EOF | sudo tee /etc/sysctl.d/99-kubernetes-cri.conf
net.bridge.bridge-nf-call-iptables  = 1
net.ipv4.ip_forward                 = 1
net.bridge.bridge-nf-call-ip6tables = 1
EOF

  cat <<EOF >/etc/sysctl.d/k8s.conf
net.ipv4.ip_forward = 1
net.bridge.bridge-nf-call-iptables = 1
fs.may_detach_mounts = 1
vm.overcommit_memory=1
vm.panic_on_oom=0
fs.inotify.max_user_watches=89100
fs.file-max=52706963
fs.nr_open=52706963
net.netfilter.nf_conntrack_max=2310720

net.ipv4.tcp_keepalive_time = 600
net.ipv4.tcp_keepalive_probes = 3
net.ipv4.tcp_keepalive_intvl =15
net.ipv4.tcp_max_tw_buckets = 36000
net.ipv4.tcp_tw_reuse = 1
net.ipv4.tcp_max_orphans = 327680
net.ipv4.tcp_orphan_retries = 3
net.ipv4.tcp_syncookies = 1
net.ipv4.tcp_max_syn_backlog = 16384
net.ipv4.ip_conntrack_max = 65536
net.ipv4.tcp_max_syn_backlog = 16384
net.ipv4.tcp_timestamps = 0
net.core.somaxconn = 16384

net.ipv6.conf.all.disable_ipv6 = 0
net.ipv6.conf.default.disable_ipv6 = 0
net.ipv6.conf.lo.disable_ipv6 = 0
net.ipv6.conf.all.forwarding = 1
EOF
  sysctl --system

  ls /boot/vmlinuz-5*
  if [ $? -ne 0 ]; then
    yum install https://www.elrepo.org/elrepo-release-7.el7.elrepo.noarch.rpm -y
    yum --enablerepo=elrepo-kernel install kernel-lt -y
    grubby --set-default $(ls /boot/vmlinuz-* | grep elrepo)
    grubby --default-kernel
  fi

  # THIS IS FOR RHEL 7: Change ethernet interface to work as eth0
  sed -i 's/rhgb quiet/net.ifnames=0 biosdevname=0 ipv6.disable=1/' /etc/default/grub
  grub2-mkconfig -o /boot/grub2/grub.cfg
  if [ -d /boot/efi/EFI/redhat ]; then
    grub2-mkconfig -o /boot/efi/EFI/redhat/grub.cfg
  fi

  grep -q ':/snap/bin' /etc/profile
  if [ $? -ne 0 ]; then
    echo 'export PATH=$PATH:/snap/bin' >>/etc/profile
  fi

  grep -q 'LC_ALL' /etc/profile
  if [ $? -ne 0 ]; then
    echo 'export LC_ALL=en_US.UTF-8' >>/etc/profile
  fi

  cat <<EOF | sudo tee /etc/modules-load.d/containerd.conf
overlay
br_netfilter
EOF
  if [ ! -e /usr/sbin/conntrack ]; then
    yum install ipvsadm ipset sysstat conntrack libseccomp -y
    cat >>/etc/modules-load.d/ipvs.conf <<EOF
ip_vs
ip_vs_rr
ip_vs_wrr
ip_vs_sh
nf_conntrack
ip_tables
ip_set
xt_set
ipt_set
ipt_rpfilter
ipt_REJECT
ipip
EOF
    systemctl restart systemd-modules-load.service
    lsmod | grep -e ip_vs -e nf_conntrack
  fi

  echo "need to reboot to take effect"
}

function prepare_onlineenv() {
  local mode="$1"
  export LC_ALL=en_US.UTF-8
  case $1 in
    0)
      prepare_online_init
      return $?
      ;;

    1)
      if [ ! -e /snap ]; then
        echo "install snap"

        sudo yum -y install epel-release
        #yum remove libseccomp -y
        #yum install -y http://rpmfind.net/linux/centos/8-stream/BaseOS/x86_64/os/Packages/libseccomp-2.5.1-1.el8.x86_64.rpm
        yum install -y chrony snapd
        systemctl enable --now snapd.socket
        sudo systemctl start snapd
        sleep 5
        sudo ln -s /var/lib/snapd/snap /snap
      else
        echo "snap already install,skip"
      fi
      ;;

    2)
      if command -v kubectl; then
        echo "microk8s already install,skip"
      else
        echo "install microk8s"
        #snap install microk8s --classic
        snap install microk8s --classic --channel=$MICROK8S_CHANNEL/stable
        snap alias microk8s.kubectl kubectl
      fi
      ;;

    3)
      if [ $(nmcli d show eth0 | grep IP4.GATEWAY | awk '{print $2}') == "$FAKEGW" ]; then
        echo "microk8s already init,skip"
      else
        echo "init microk8s"
        #   fake ip and gw used to install
        sed -i "s#--cluster-cidr=.*#--cluster-cidr=${FAKECNI}#" /var/snap/microk8s/current/args/kube-proxy
        sed -i "/CALICO_IPV4POOL_CIDR/{n;s#value.*#value: \"$FAKECNI\"#}" /var/snap/microk8s/current/args/cni-network/cni.yaml
        kubectl apply -f /var/snap/microk8s/current/args/cni-network/cni.yaml
      fi
      ;;

    4)
      echo "start microk8s"
      microk8s start
      microk8s enable 'dns' 'ingress' 'metrics-server' storage registry
      ;;

    5)
    snap list
    microk8s.ctr i list | grep -vw DIGEST | grep -v ^sha256 | grep -v @sha256 | awk '{print $1}' | sort
    ;;

    *)
      echo "  0   - install snap"
      echo "  1   - install microk8s"
      echo "  2   - init  microk8s"
      echo "  3   - setup microk8s"
      echo "  4   - enable microk8s addon"
      echo "  5   - list snap and containerd pkg info"
      ;;
  esac

}

# usage:
#   1.  install an clean centos7
#   2.  send prepare.sh into centos7
#   3.  prepare k8s env:
#       ./prepare.sh 0
#       then reboot
#   4.  setup k8s env:
#       manual setup network or run:  ./prepare.sh 0
#       setup snap env:               ./prepare.sh 1
#       -- can take a snapshot here, because some microk8s will failed to start
#       install microk8s:             ./prepare.sh 2
#       setup microk8s cni:           ./prepare.sh 3
#       setup microk8s addon:         ./prepare.sh 4
#       show microk8s status, it should be start, if not, maybe the microk8s version maybe not compatable with centos7
#             restore the snapshot then change $MICROK8S_CHANNEL and try again
#       show pkg info:                ./prepare.sh 5
#             then replace MICROK8S_IMAGES with latest docker images
#       last step:                    ./prepare.sh
#             download offline microk8s snap pkg and docker images into local dir.
#
if [ $# -lt 1 ]; then
  echo "prepare image $#"
  mkdir -p microk8s_sources/
  prepare_pkg && prepare_image
else
# setup an online microk8s, used to get k8s image info
  echo "setup online microk8s env: $1"
  prepare_onlineenv "$@"
fi
