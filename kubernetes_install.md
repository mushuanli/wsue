
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

  # Demo
  ## Dockerfile
  ```
  FROM node:10-alpine

USER node
RUN mkdir -p /home/node/app/{keys,node_modules}
WORKDIR /home/node/app
ADD app.tar.gz .
RUN npm install
EXPOSE 8081

CMD [ "node", "xdr_app_backend_server.js" ]
```

  ## rain-test-settings.yaml(define kubernetes deploy and service)
  ```
  apiVersion: apps/v1 # for versions before 1.9.0 use apps/v1beta2
kind: Deployment
metadata:
  name: rain-test
  namespace: 
  labels:
    app: rain-test
spec:
  replicas: 1
  selector:
    matchLabels:
      app: rain-test
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxUnavailable: 0
      maxSurge: 1
  template:
    metadata:
      labels:
        app: rain-test
        role: master
        tier: backend
    spec:
      nodeSelector:
        "beta.kubernetes.io/os": linux
      imagePullSecrets:
      - name: secret-ecr-xdr-backend-tiars
      volumes:
      - name: jwt-keys
        secret:
          secretName: secret-jwt-rain-test
      containers:
      - name: rain-test
        image: 10.206.138.106:5000/xdr_backend_ti:1.0.1  # or just image: redis
        env:
        - name: NODE_ENV
          value: debug
        - name: AWSDDB_REGION
          valueFrom:
            secretKeyRef:
              name: secret-ddb-rain-test
              key: region
        - name: AWSDDB_PREFIX
          valueFrom:
            secretKeyRef:
              name: secret-ddb-rain-test
              key: dbname_prefix
        - name: AWS_ACCESS_KEY_ID
          valueFrom:
            secretKeyRef:
              name: secret-ddb-rain-test
              key: username
        - name: AWS_SECRET_ACCESS_KEY
          valueFrom:
            secretKeyRef:
              name: secret-ddb-rain-test
              key: password
        resources:
          requests:
            cpu: 100m
            memory: 100Mi
        volumeMounts:
          - mountPath: "/home/node/app/keys"
            name: jwt-keys
            readOnly: true
        ports:
        - containerPort: 8081
        livenessProbe:
          httpGet:
            path: /rain/test/hello
            port: 8081
          initialDelaySeconds: 60
          periodSeconds: 10
          timeoutSeconds: 5

---
kind: Service
apiVersion: v1
metadata:
  name: rain-test
  namespace: 
  annotations:
    service.beta.kubernetes.io/brightbox-load-balancer-healthcheck-request: /
spec:
        #type: LoadBalancer
  type: NodePort
  selector:
    app: rain-test
  ports:
    - name: rain
      protocol: TCP
      port: 80
      targetPort: 8081
  ```
  
  ## secret config file(regcred-aws-dev.yaml)
  # dev
apiVersion: v1
kind: Secret
metadata:
  name: secret-jwt-rain-test
type: Opaque
data:
  # echo -n 'password or username' | base64
  region: dXMtd2VzdC0y
  dbname_prefix: dXMt
  username: FFFFFFFF=
  password: FFFFFFFFFFFFFFF==

  ## deploy script(deploy.sh)
  save as deploy.sh, auto deploy service
  ```
  #!/bin/bash

if [[ "$1" == "setup" ]] ; then
    ONLY_SETUP="TRUE"
    shift
fi

if [[ "$1" == "init" ]] ; then
    ONLY_INITCONFIG="TRUE"
    shift
fi

if [[ "$1" == "deploy" ]] ; then
    ONLY_DEPLOY="TRUE"
    shift
fi

if [[ "$1" == "uninstall" ]] ; then
    ONLY_UNINSTALL="TRUE"
    shift
fi

if [[ "$1" == "stat" ]] ; then
    ONLY_STAT="TRUE"
    shift
fi


RUNNING_ENV=$1
IMAGE_VERSION=$2

NODE_ENV=dev
ECR_SERVER=10.206.138.106:5000
ECR_IMAGE_NAME='rain-test'

EKS_SECRET_ECR="secret-ecr-$ECR_IMAGE_NAME"
EKS_SECRET_JWT="secret-jwt-$ECR_IMAGE_NAME"
EKS_SECRET_YAML='regcred-aws-dev.yaml'
EKS_DEPLOY_YAML='rain-test-settings.yaml'

AWS_REGION='us-west-2'
AWS_DBPREFIX=dev

KEYS_RAIN_V1='rain_v1.pub.pem'
KEYS_RAIN_V2='rain_v2.pub.pem'


function help() {
    echo "param: [setup|init|deploy|uninstall|stat] env(dev|stg|prod) image_version"
    echo "    env: dev|stg|prod|int"
    echo "    image_version: such as 1.0.1001"
}

function setup(){
    set -e

    if ! [ -x "$(command -v aws)" ]; then
        if ! [ -x "$(command -v pip3 )" ] ; then
            echo 'install pip3 ...'
            curl -O https://bootstrap.pypa.io/get-pip.py
            python3 get-pip.py 
            pip3 install awscli --upgrade 
            exit 1
        fi

        if ! [ -x "$(command -v aws)" ] ; then
            echo 'install aws failed'
            exit 1
        fi
    fi

    if ! [ -x "$(command -v docker)" ]; then
        if [ -x "$(command -v yum)" ] ; then
            yum install -y docker
        elif [ -x "$(command -v apt)" ] ; then
            apt install -y docker.io
        fi

        if ! [ -x "$(command -v docker)" ]; then
            echo 'install docker failed'
            exit 1
        fi
    fi

    if ! [ -x "$(command -v kubectl)" ]; then
        if [ -x "$(command -v yum)" ] ; then
            cat <<'EOF' > /etc/yum.repos.d/kubernetes.repo
[kubernetes]
name=Kubernetes
baseurl=https://packages.cloud.google.com/yum/repos/kubernetes-el7-$basearch
enabled=1
gpgcheck=1
repo_gpgcheck=1
gpgkey=https://packages.cloud.google.com/yum/doc/yum-key.gpg https://packages.cloud.google.com/yum/doc/rpm-package-key.gpg
EOF
            yum -y install kubeadm kubelet kubectl
        elif [ -x "$(command -v apt)" ] ; then
            apt -y install apt-transport-https gnupg2 curl
            curl -s https://packages.cloud.google.com/apt/doc/apt-key.gpg | apt-key add -

            echo "deb http://apt.kubernetes.io/ kubernetes-xenial main" | tee -a /etc/apt/sources.list.d/kubernetes.list
            apt update
            apt -y install kubectl            
        fi

        if ! [ -x "$(command -v kubectl)" ]; then
            echo '*** install kubectl failed ***'
            exit 1
        fi
    fi

    if [[ -z "$ECR_REGION" ]] ; then
        echo '*** Init Kubectl cli success ***'
        exit 0
    fi

    echo ''
    echo 'Begin Init AWS and EKS'
    aws configure
    
    read -p 'EKS Region ("us-east-1"): ' eks_region
    read -p 'EKS namespace("rain-ekspace"): ' eks_name
    if [[ -z "$eks_region" ]] ; then
        eks_region="us-east-1"
    fi
    if [[ -z "$eks_name" ]] ; then
        eks_name="rain-ekspace"
    fi

    echo "EKS Info: $eks_name on $eks_region, ECR Region: $ECR_REGION"
    aws eks --region $eks_region update-kubeconfig --name $eks_name
    $(aws ecr get-login --region "$ECR_REGION" --no-include-email)
    aws ecr create-repository --repository-name $ECR_PATH$ECR_IMAGE_NAME || true
}

function rebuildimage(){
    local localimage=$1
    local ecrimage=$2


    oldimages=$(docker images --format "{{.ID}}: {{.Repository}}:{{.Tag}}" | grep -w "$localimage" | awk -F: '{print $1}' | sort | uniq)
    if [ ! -z "$oldimages" ] ; then
        echo "*** delete old images: $oldimages ***"
        docker rmi -f $oldimages
    fi

    rm -f app.tar.gz ; cd .. && tar czf $OLDPWD/app.tar.gz *.js package.json web.config && cd -
    docker build -f Dockerfile . -t $localimage
    rm -f app.tar.gz
    echo "will tag and push $localimage to $ecrimage, make sure has rum:"
    echo "    aws ecr get-login --region us-east-1 --no-include-email"
    echo "    aws ecr create-repository --repository-name $ECR_PATH$ECR_IMAGE_NAME"
    
    docker tag "$localimage" "$ecrimage"
    docker push "$ecrimage"
}


function updateconfig() {
    local version=$1
    local kubename=$2
    local ecrenv=$3
    local ecrimg=$4

    sed -r -i -e 's#(\s+region:\s+)(.*)#\1'"$(echo -n $AWS_REGION|base64 -w0)"'#g'          $EKS_SECRET_YAML
    sed -r -i -e 's#(\s+dbname_prefix:\s+)(.*)#\1'"$(echo -n $AWS_DBPREFIX|base64 -w0)"'#g' $EKS_SECRET_YAML
    if [[ ! -z "$kubename" ]] ; then
        sed -r -i -e 's#(\s+kubename:\s+)(.*)#\1'"${kubename}"'#g' $EKS_DEPLOY_YAML
    fi
    sed -r -i -e 's#(\s+image:\s+)(.*)#\1'"${ecrimg}"'#g'     $EKS_DEPLOY_YAML
    sed -r -i -e 's#(\s+image:\s+)(.*)#\1'"${ecrimg}"'#g'     $EKS_DEPLOY_YAML
    sed -rie '/-\s+name:\s+NODE_ENV/{n;s#(\s+value:\s+)(.*)#\1'"${ecrenv}"'#;}' $EKS_DEPLOY_YAML

    sed -ie 's|\(.*"version"\): "\(.*\)".*|\1: '"\"$version\",|" ../package.json
}


if [[ "$RUNNING_ENV" != "int" ]] ; then
    if [[ ! -n "${RUNNING_ENV}" ]] ;then
        echo "not input env"
        help 
        exit 1
    fi

    case ${RUNNING_ENV} in
        dev)
            ;;

        stg)
            NODE_ENV=stage
            AWS_REGION='us-east-1'
            AWS_DBPREFIX=staging
            ;;

        prod)
            NODE_ENV=product
            AWS_REGION='us-east-1'
            AWS_DBPREFIX=prod
            EKS_SECRET_YAML='regcred-aws-prod.yaml'
            KEYS_RAIN_V1='rain_v1.pub.pem.prod'
            KEYS_RAIN_V2='rain_v1.pub.pem.prod'
            ;;

        *)
            help
            exit 1
            ;;
    esac

    ECR_SERVER=$(aws ecr get-login --region us-east-1 --no-include-email | awk '{print $7}' | sed 's#^https://##')
    ECR_PATH='rain-namespace/'
    ECR_REGION='us-east-1'

    EKS_NAMESPACE='rain-ekrspace'
    EKS_PARAMS="-n $EKS_NAMESPACE"
fi


if [[ ! -z "$ONLY_SETUP" ]] ; then
    setup
    echo 'setup success finish'
    exit 0
fi

if [[ ! -z "$ONLY_UNINSTALL" ]] ; then
    kubectl ${EKS_PARAMS} delete -f $EKS_DEPLOY_YAML
    kubectl ${EKS_PARAMS} delete -f $EKS_SECRET_YAML
    kubectl ${EKS_PARAMS} delete secret $EKS_SECRET_ECR
    kubectl ${EKS_PARAMS} delete secret $EKS_SECRET_JWT
    echo 'uninstall finish'
    exit 0
fi

if [[ ! -z "$ONLY_STAT" ]] ; then
    kubectl ${EKS_PARAMS} get secret
    kubectl ${EKS_PARAMS} get svc
    kubectl ${EKS_PARAMS} get ingress
    kubectl ${EKS_PARAMS} get pods
    exit 0
fi


rx="^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$"
if ! [[ $IMAGE_VERSION =~ $rx ]]; then
    echo "$IMAGE_VERSION not a valid docker image version!"
    help 
    exit 1
fi

echo "    image version: "${IMAGE_VERSION}
echo "    env:           "${RUNNING_ENV}




ECR_FULL_IMAGEPATH="$ECR_SERVER/$ECR_PATH$ECR_IMAGE_NAME:$IMAGE_VERSION"

set -e

if [[ -z "$ONLY_DEPLOY" ]] ; then
    updateconfig "$IMAGE_VERSION" "$EKS_NAMESPACE" "${NODE_ENV}" "${ECR_FULL_IMAGEPATH}"

    echo "*** build $ECR_IMAGE_NAME:$IMAGE_VERSION and push to $ECR_SERVER ***"
    rebuildimage "$ECR_IMAGE_NAME:$IMAGE_VERSION" "$ECR_FULL_IMAGEPATH"
    if [[ ! -z "$ONLY_INITCONFIG" ]] ; then
        echo "Init config success, next command: "
        echo "    ./deploy.sh deploy $IMAGE_VERSION $RUNNING_ENV"
        exit 0
    fi
fi



echo "*** create secrets ***"



if [[ ! -z "$ECR_REGION" ]] ; then
    # create ecr secret start
    EMAIL='test@rain.com'                                     #can be anything
    TOKEN=`aws ecr --region=${ECR_REGION} get-authorization-token --output text --query authorizationData[].authorizationToken | base64 -d | cut -d: -f2`
    kubectl ${EKS_PARAMS} create secret docker-registry ${EKS_SECRET_ECR} \
        --docker-server=https://$ECR_SERVER \
        --docker-username=AWS \
        --docker-password="${TOKEN}" \
        --docker-email="${EMAIL}" --dry-run -o yaml | kubectl apply -f -
else
    #   LOCAL test server
    kubectl ${EKS_PARAMS} create secret docker-registry ${EKS_SECRET_ECR} \
        --docker-server=https://$ECR_SERVER \
        --docker-username=root \
        --docker-password=root \
        --docker-email="${EMAIL}" --dry-run -o yaml | kubectl apply -f -
fi

echo "create $EKS_SECRET_YAML"
kubectl ${EKS_PARAMS} apply -f $EKS_SECRET_YAML
echo "create $EKS_SECRET_JWT"
kubectl ${EKS_PARAMS} create secret generic $EKS_SECRET_JWT \
    --from-file=${KEYS_RAIN_V1}=../keys/$KEYS_RAIN_V1 \
    --from-file=${KEYS_RAIN_V2}=../keys/$KEYS_RAIN_V2 \
     --dry-run -o yaml | kubectl apply -f -

echo "*** create deployment and service ***"
kubectl ${EKS_PARAMS} apply -f $EKS_DEPLOY_YAML

echo ''
echo "success redeploy $ECR_FULL_IMAGEPATH"
echo ''
echo "################## finish deploy ##################"

exit 0


    # create ecr secret end


  ```
