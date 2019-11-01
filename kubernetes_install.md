

# 1. ref:

 https://www.itzgeek.com/how-tos/linux/centos-how-tos/how-to-install-kubernetes-on-centos-7-ubuntu-18-04-16-04-debian-9.html 

 https://www.server-world.info/en/note?os=Debian_10&p=kubernetes&f=5 



# 2. All node: prepare running env:

  ### disable swap
  Disable running swap stat:
  ```
swapoff -a
vi /etc/fstab
   ```
   
  comment out for swap line to disable swap after reboot:
  ```
#/dev/mapper/ubuntu--vg-swap_1 none swap sw 0 0
  ```
  ### run 'hostnamectl set-hostname uniq hostname ' on all node. 
  ```
hostnamectl set-hostname k8s.rainli.net
  ```
  
  ### add all nodes ip to /etc/hosts
  master: 10.206.138.107
  worker: 10.206.138.108 
  /etc/hosts file should include all kubernetes master and worker nodes:
  ```
root@k8s:~# cat /etc/hosts
127.0.0.1    localhost
10.206.138.107    k8s.rainli.net  k8s
10.206.138.108    k8sn.rainli.net k8sn 
...
  ```

  ### disable SELinux:
  ```
setenforce 0
sed -i 's/SELINUX=enforcing/SELINUX=disabled/g' /etc/selinux/config
reboot
  ```
  
  ### fix iptables compatable, debian10 default not open firewall, so can ignore firewall:
  ```
update-alternatives --config iptables
There are 2 choices for the alternative iptables (providing /usr/sbin/iptables).   Selection    Path                       Priority   Status 
\------------------------------------------------------------ 
\* 0            /usr/sbin/iptables-nft      20        auto mode   
1            /usr/sbin/iptables-legacy   10        manual mode
2            /usr/sbin/iptables-nft      20        manual mode 
\# select IPTables Legacy Press <enter> to keep the current choice[*], or type selection number: 1 
update-alternatives: using /usr/sbin/iptables-legacy to provide /usr/sbin/iptables (iptables) in manual mode 
  ```
  
  ### RHEL 7/CentOS 7 need to set net.bridge.bridge-nf-call-iptables=1:
  ```
cat <<EOF > /etc/sysctl.d/k8s.conf
net.bridge.bridge-nf-call-ip6tables = 1
net.bridge.bridge-nf-call-iptables = 1
EOF
sysctl -p
  ```
  
  ### If use private docker registry, add crt:
  Assume docker private registry server name is git, cert file is /home/registry/certs/domain.crt.

  for Ubuntu/debian:
  ```
   > scp git:/home/registry/certs/domain.crt /usr/local/share/ca-certificates/git.crt
update-ca-certificates
\# if remove, delete ca.crt, then run: update-ca-certificates --fresh
  ```

  for RHEL/CentOS:
  ```  
scp git:/home/registry/certs/domain.crt /etc/pki/ca-trust/source/anchors/git.crt
update-ca-trust extract
  ```


# 3. All node: install docker and kubernetes
  to avoid docker used too many disk space, I limit docker run in k8s/docker dir.

  ### install docker:
  ```
mkdir -p /k8s/docker
cat > /etc/docker/daemon.json <<EOF
{
"data-root": "/k8s/docker",
"exec-opts": ["native.cgroupdriver=systemd"],
"storage-driver": "overlay2"
}
EOF
   >
apt-get install -y docker.io 
\# or RHEL/CentOS: yum install -y docker
systemctl enable docker && systemctl restart docker
   >

  ```


  ### install kubelet:
  ```
apt -y install apt-transport-https gnupg2 curl
curl -s https://packages.cloud.google.com/apt/doc/apt-key.gpg | apt-key add -
   >
echo "deb http://apt.kubernetes.io/ kubernetes-xenial main" | tee -a /etc/apt/sources.list.d/kubernetes.list
   >
apt update
apt -y install kubeadm kubelet kubectl
  ```


  ### only enabling, do not run yet:
  ```
systemctl enable kubelet
  ```





# 4. Master node: init kubernetes 
  ### enable firewall if has firewall:
  ```  
firewall-cmd --permanent --add-port=6443/tcp
firewall-cmd --permanent --add-port=2379-2380/tcp
firewall-cmd --permanent --add-port=10250/tcp
firewall-cmd --permanent --add-port=10251/tcp
firewall-cmd --permanent --add-port=10252/tcp
firewall-cmd --permanent --add-port=10255/tcp
firewall-cmd --reload
  ```

  Or:
  ```  
ufw allow 6443/tcp
ufw allow 2379tcp
ufw allow 2380/tcp
ufw allow 10250/tcp
ufw allow 10251/tcp
ufw allow 10252/tcp
ufw allow 10255/tcp
ufw reload
  ```

  ### use flannel network(pod-network-cidr=10.244.0.0/16). 10.206.138.107 is local IP address:
  ```  
kubeadm init --apiserver-advertise-address=10.206.138.107 --pod-network-cidr=10.244.0.0/16
  ```

  save the output token(it is in "kubeadm join 10.0.0.30:6443 --token ... " format) - it will be used by nodes to join cluster. and set cluster admin(these command showed in kubeadm init ):
  ```  
root@k8s:~#   mkdir -p $HOME/.kube
root@k8s:~# cp -i /etc/kubernetes/admin.conf $HOME/.kube/config
root@k8s:~# chown $(id -u):$(id -g) $HOME/.kube/config
  ```

  ### setup network(flannel):
  ```  
kubectl apply -f https://raw.githubusercontent.com/coreos/flannel/master/Documentation/kube-flannel.yml
  ```
  
  ### Now check cluster status, it should be OK:
  ```  
kubectl get nodes
kubectl get svc
kubectl get pods --all-namespaces
  ```

# 5. worker node: join kubernetes cluster:
  ### enable firewall if has firewall:
  ```  
   >firewall-cmd --permanent --add-port=10251/tcp
   >firewall-cmd --permanent --add-port=10255/tcp
   >firewall-cmd --reload
  ```

  Or:
  ```  
ufw allow 10251/tcp
ufw allow 10255/tcp
ufw reload
  ```

  ### load kubernetes require driver:
  ```  
modprobe ip_vs
modprobe ip_vs_rr
modprobe ip_vs_wrr
modprobe ip_vs_sh
  ```

  ### Let modules auto load when boot
  change /etc/modules, add these lines to end:
  ```  
ip_vs
ip_vs_rr
ip_vs_wrr
ip_vs_sh
  ```

  ### join to kubernetes cluster:
  when run kubeadm init in master, it shows a 'kubeadm join' commmand, copy it to here and run:
  ```  
kubeadm join 10.206.138.106:6443 --token uw8h1x.4vjex3g6tfgt4w2t \
--discovery-token-ca-cert-hash sha256:99c192dcb2b38438c4aacc5029b86447f18f2b93a0fe0fa7a779192bc952fb53
  ```
  
  ### now check node stat on master, it should be OK:
  ```  
kubectl get nodes
  ```
