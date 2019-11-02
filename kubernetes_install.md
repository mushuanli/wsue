
---
   # Install kubernetes
---

# 1. ref:

 https://www.itzgeek.com/how-tos/linux/centos-how-tos/how-to-install-kubernetes-on-centos-7-ubuntu-18-04-16-04-debian-9.html

 https://www.server-world.info/en/note?os=Debian_10&p=kubernetes&f=5



# 2. All node: prepare running env:

  * ### Swap: Disable
  Disable running swap stat:
  ```bash
swapoff -a
vi /etc/fstab
  ```

  comment out for swap line to disable swap after reboot:
  ```bash
#/dev/mapper/ubuntu--vg-swap_1 none swap sw 0 0
  ```

  * ### All Node - Run 'hostnamectl set-hostname uniq hostname '.
  ```bash
hostnamectl set-hostname k8s.rainli.net
  ```

  * ### Hostname map: all nodes /etc/hosts has all node's hostname map
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

  * ### SELinux: Disable:
  ```bash
setenforce 0
sed -i 's/SELINUX=enforcing/SELINUX=disabled/g' /etc/selinux/config
reboot
  ```

  * ### Debian10 - fix iptables compatable
  debian10 need iptables_legacy, other distribute not require:
  ```
update-alternatives --config iptables
There are 2 choices for the alternative iptables (providing /usr/sbin/iptables).   Selection    Path                       Priority   Status
------------------------------------------------------------
* 0            /usr/sbin/iptables-nft      20        auto mode
1            /usr/sbin/iptables-legacy   10        manual mode
2            /usr/sbin/iptables-nft      20        manual mode
# select IPTables Legacy Press <enter> to keep the current choice[*], or type selection number: 1
update-alternatives: using /usr/sbin/iptables-legacy to provide /usr/sbin/iptables (iptables) in manual mode
  ```

  * ### RHEL 7/CentOS 7 - need to set net.bridge.bridge-nf-call-iptables=1:
  ```bash
cat <<EOF > /etc/sysctl.d/k8s.conf
net.bridge.bridge-nf-call-ip6tables = 1
net.bridge.bridge-nf-call-iptables = 1
EOF
sysctl -p
  ```

  * ### Private Docker Registry: If use self signed cert, add it to all nodes:
  Assume docker private registry server name is git, cert file is /home/registry/certs/domain.crt.

  for Ubuntu/debian:
  ```bash
scp git:/home/registry/certs/domain.crt /usr/local/share/ca-certificates/git.crt
update-ca-certificates
# when remove, delete ca.crt, then run: update-ca-certificates --fresh
  ```

  for RHEL/CentOS:
  ```bash
scp git:/home/registry/certs/domain.crt /etc/pki/ca-trust/source/anchors/git.crt
update-ca-trust extract
  ```


# 3. All node: install docker and kubernetes
  to avoid docker used too many disk space, I limit docker run in k8s/docker dir.

  * ### Docker: install
  ```bash
mkdir -p /k8s/docker
cat > /etc/docker/daemon.json <<EOF
{
  "data-root": "/k8s/docker",
  "exec-opts": ["native.cgroupdriver=systemd"],
  "storage-driver": "overlay2"
}
EOF

apt-get install -y docker.io
# or RHEL/CentOS: yum install -y docker
systemctl enable docker && systemctl restart docker


  ```


  * ### kubelet: install
  ```bash
apt -y install apt-transport-https gnupg2 curl
curl -s https://packages.cloud.google.com/apt/doc/apt-key.gpg | apt-key add -

echo "deb http://apt.kubernetes.io/ kubernetes-xenial main" | tee -a /etc/apt/sources.list.d/kubernetes.list
apt update
apt -y install kubeadm kubelet kubectl
  ```


  * ### kubelet: enable but not run
  ```bash
systemctl enable kubelet
  ```





# 4. Master node: init kubernetes cluster
  * ### Firewall: enable kubernetes ports
  Debian10 default not open firewall, so can ignore firewall.
  ```bash
firewall-cmd --permanent --add-port=6443/tcp
firewall-cmd --permanent --add-port=2379-2380/tcp
firewall-cmd --permanent --add-port=10250/tcp
firewall-cmd --permanent --add-port=10251/tcp
firewall-cmd --permanent --add-port=10252/tcp
firewall-cmd --permanent --add-port=10255/tcp
firewall-cmd --reload
  ```

  Or:
  ```bash
ufw allow 6443/tcp
ufw allow 2379tcp
ufw allow 2380/tcp
ufw allow 10250/tcp
ufw allow 10251/tcp
ufw allow 10252/tcp
ufw allow 10255/tcp
ufw reload
  ```

  * ### Cluster: Init
  Param Info:
   * Flannel network (--pod-network-cidr=10.244.0.0/16)
   * 10.206.138.107 is local IP address
   
  ```bash
kubeadm init --apiserver-advertise-address=10.206.138.107 --pod-network-cidr=10.244.0.0/16
  ```

  save the output token, it like this format, and will used by worker node:
    ```bash
kubeadm join 10.206.138.106:6443 --token uw8h1x.4vjex3g6tfgt4w2t \
--discovery-token-ca-cert-hash sha256:99c192dcb2b38438c4aacc5029b86447f18f2b93a0fe0fa7a779192bc952fb53
  ```
  
  and run these commands to set cluster admin(these command showed in kubeadm init output):
  ```bash
root@k8s:~#   mkdir -p $HOME/.kube
root@k8s:~# cp -i /etc/kubernetes/admin.conf $HOME/.kube/config
root@k8s:~# chown $(id -u):$(id -g) $HOME/.kube/config
  ```

  * ### Cluster: setup network(flannel):
  ```bash
kubectl apply -f https://raw.githubusercontent.com/coreos/flannel/master/Documentation/kube-flannel.yml
  ```

  * ### Cluster: get status
  Use these command to check cluster status, it should be OK:
  ```bash
kubectl get nodes
kubectl get svc
kubectl get pods --all-namespaces
  ```

# 5. Worker node: register to kubernetes cluster:
  * ### Firewall: enable kubernetes ports
  Debian10 default not open firewall, so can ignore firewall.

  ```bash
firewall-cmd --permanent --add-port=10251/tcp
firewall-cmd --permanent --add-port=10255/tcp
firewall-cmd --reload
  ```

  Or:
  ```bash
ufw allow 10251/tcp
ufw allow 10255/tcp
ufw reload
  ```

  * ### Driver: load ip_vs
  ```bash
modprobe ip_vs
modprobe ip_vs_rr
modprobe ip_vs_wrr
modprobe ip_vs_sh
  ```

  Let modules auto load when boot
  change /etc/modules, add these lines to end:
  ```bash
ip_vs
ip_vs_rr
ip_vs_wrr
ip_vs_sh
  ```

  * ### Cluster: register self
  when run kubeadm init in master, it shows a 'kubeadm join' commmand, copy it to here and run:
  ```bash
kubeadm join 10.206.138.106:6443 --token uw8h1x.4vjex3g6tfgt4w2t \
--discovery-token-ca-cert-hash sha256:99c192dcb2b38438c4aacc5029b86447f18f2b93a0fe0fa7a779192bc952fb53
  ```

  * ### Cluster: get new node status
  In master node, use these command to check new worker node status, it should be OK:
  ```bash
kubectl get nodes
  ```
  
  
  ---
    # Install addons
  ---
  
  
  ---
    # Deploy an pod
  ---
  If it pull image from docker, create a secret("regcred-xdr-backend-tiars") fot it:
  ```
  kubectl create secret docker-registry regcred-xdr-backend-tiars --docker-server=<your-registry-server> --docker-username=<your-name> --docker-password=<your-pword> --docker-email=<your-email>
  ```
  
  ---
    # Normal command
  ---
  get secret info:
  ```
  kubectl get secret secret-xdr-backend-ti-ddb
  ```
  
  ---
    # Debug command
  ---
  # 1. Check Node error
  * ### Master:
  - /var/log/kube-apiserver.log - API Server, responsible for serving the API
  - /var/log/kube-scheduler.log - Scheduler, responsible for making scheduling decisions
  - /var/log/kube-controller-manager.log - Controller that manages replication controllers
  * ### Worker Nodes:
  - /var/log/kubelet.log - Kubelet, responsible for running containers on the node
  - /var/log/kube-proxy.log - Kube Proxy, responsible for service load balancing

  # 2. Check Pod/service error
  if Pod unhealth or always crash:
  ```
  kubectl logs ${POD_NAME} ${CONTAINER_NAME}
  ```
  or
  ```
  kubectl logs --previous ${POD_NAME} ${CONTAINER_NAME}
  ```
  
  get Pod(xdr-backend-ti) logs:
  ```
  kubectl logs -f deployment/app
  # Or
  kubectl logs --selector app=xdr-backend-ti
  # Or show in realtime:
  kubectl logs -f deployment/xdr-backend-ti
  ```
  
  get pod status:
  ```
   kubectl describe pods xdr-backend-ti
  ```
  
  exec command in pod's container, if only one container, can ignore "-c ${CONTAINER_NAME}"
  ```
  kubectl exec ${POD_NAME} -c ${CONTAINER_NAME} -- ${CMD} ${ARG1} ${ARG2} ... ${ARGN}
  ```
  
  check pod yaml file error:
  ```
  kubectl apply --validate -f mypod.yaml
  ```
  
  check RC error:
  ```
  kubectl describe rc ${CONTROLLER_NAME}
  ```
  
  check service error - if has endpoint:
  ```
  kubectl get endpoints ${SERVICE_NAME}
  ```
  
  check service error - if has pods:
  ```
  kubectl get pods --selector=name=nginx,type=frontend
  ```
