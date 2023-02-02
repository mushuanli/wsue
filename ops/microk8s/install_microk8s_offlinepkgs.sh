#!/bin/bash

#   script used to install microk8s offline pkgs
#set -x -u
SNAP_BIN="sudo /usr/bin/snap"
MICROK8S_BIN="sudo /snap/bin/microk8s"
CONTAINERD_BIN="sudo /snap/bin/microk8s ctr image"
KUBECTL_BIN="sudo /snap/bin/microk8s kubectl"
SGKUBECTL_BIN="sudo /snap/bin/microk8s kubectl -n service-gateway"
RUNDIR="$(dirname $(readlink -f -- "${BASH_SOURCE[0]:-$0}"))/"
declare -a SNAP_PKGS=("snapd" "core18" "microk8s")

LOG_DEBUG="log DEBUG"
LOG_INFO="log INFO"
LOG_WARN="log WARNING"
LOG_ERROR="log ERROR"

FAKECNI="198.18.0.0/16"

log()
{
    local LEVEL=$1
    local MSG="$2"
    echo "`date +"%Y-%m-%d %H:%M:%S"` microk8s [$LEVEL] $MSG"
}


#   @desc: if k8s is running? 0: running   1: stop
function api_k8sStat()
{
    sudo /snap/bin/microk8s status | grep 'microk8s is running' >/dev/null 2>&1
    return $?
}


#   @desc: if k8s is disabled? 0: disabled   1: enable
function api_k8sDisabled()
{
    grep  'K8SSTAT=stop' /etc/trend/sgplat >/dev/null 2>&1
}

function api_k8sStop()
{
    sudo sed -ie '/K8SSTAT=stop/d' /etc/trend/sgplat

    #echo 'K8SSTAT=stop' >> /etc/trend/sgplat
    sudo /snap/bin/microk8s stop
    $LOG_INFO ">>>>>>      stop microk8s ret: $?"
    return 0
}

function api_k8sStart()
{
    api_k8sStat
    if [ $? -eq 0 ] ; then
        return 0
    fi

    local i=
    for ((i=0;i<3;i++)); do
        sudo /snap/bin/microk8s start
        local ret=$?
        $LOG_INFO ">>>>>>      start microk8s ret: $ret"
        if [ $ret -eq 0 ] ; then
            return 0
        fi

        sleep 5
    done
    return 1
}

function api_k8sRestart()
{
    api_k8sStart
    local ret=$?
    if [ $ret -eq 0 ] ; then
        sed -ie '/K8SSTAT=stop/d' /etc/trend/sgplat
        sudo rm -f /etc/trend/sgplate
    fi
    return $ret
}


#   @PUBAPI:    kubectl arguments
#   @desc:  run kubectl
#   @param: args    - kubectl argument list
#   @return:    0   - success
#               1   - cert error
#               2   - not found
#               126 - other error
#               127 - kubectl app not found
function api_kubectlX()
{
    local retry=$1
    shift
    local args=$@
    local ret=1
    if [ "$retry" -lt 1 ] ; then
        retry=1
    fi

    local cmdstr="${args[@]}"
    local tmpFile=$(mktemp)
    local einfo=
    for ((i=0;i<$retry;i++)); do
        $KUBECTL_BIN $@ 2>"$tmpFile"
        ret=$?
        if [ $ret -eq 0 ] ; then
            break
        elif [ $ret -eq 127 ] ; then
            break
        else
            einfo=$(< "$tmpFile")
            if [ -n "$(echo "$einfo" | grep "(NotFound):")" ] ; then
                $LOG_INFO "KUBECTL ret: notfound/($einfo) $cmdstr"
                ret=2
                break
            fi
            if [ -n "$(echo "$einfo" | grep "x509: certificate has expired or is not yet valid")" ] ; then
                $LOG_INFO "KUBECTL ret: x509/($einfo) $cmdstr"
                ret=1
                break
            fi
        fi
        $LOG_INFO "KUBECTL ret: $ret/($einfo) $cmdstr"
        ret=126
        sleep 20
    done

    if [ -n "$einfo" ] ; then
        echo "$einfo" >/dev/stderr
    fi
    rm -- "$tmpFile"
    return $ret
}

#   @PUBAPI:    kubectl arguments
#   @desc:  run kubectl
#   @param: args    - kubectl argument list
#   @return:    0   - success
#               127 - kubectl app not found
function api_kubectl()
{
    local args=(3 "$@")
    api_kubectlX "${args[@]}"
    return $?
}

#   @desc:  enable microk8s addon service
#   @param: name    - microk8s addon name
function core_addon_enable()
{
    local name=$1
    for ((i=0;i<3;i++)); do
        $MICROK8S_BIN enable $name
        if [ $? -eq 0 ] ; then
            return 0
        fi
        sleep 20
    done

    return 1
}

#   @desc:  wait containerd ready
#   @param: retry    - will check (retry-1)*10 second, default 60*10s
function api_waitcontainerd()
{
    local retry=
    local ret
    local i
    if [ $# -ge 1 ] ; then
        retry="$1"
    fi
    if [ -z "$retry" ] ; then
        retry=60
    fi

    for ((i=1;i<$retry;i++)); do #10min, 600s
        $CONTAINERD_BIN ls >/dev/null
        ret=$?
        if [ $ret -eq 0 ] ; then
            break
        fi
        sleep 10
    done

    if [ $ret -ne 0 ] ; then
        $LOG_WARN   "waitcontainerd (retry:$i) ret: $ret"
    else
        $LOG_INFO   "waitcontainerd (retry:$i) succ"
    fi
    return $ret
}

#   @desc:  import docker image
#   @param: name    - docker image tar file
function api_image_import()
{
    local name="$1"
    local ret
    local i
    for ((i=0;i<3;i++)); do
        $CONTAINERD_BIN import "$name"
        ret=$?
        if [ $ret -eq 0 ] ; then
            break
        fi
        sleep 20
    done

    if [ $ret -ne 0 ] ; then
        $LOG_WARN   "image_import $name (retry:$i) ret: $ret"
    else
        $LOG_INFO   "image_import $name (retry:$i) succ"
    fi
    return $ret
}

#   @desc:  import docker image
#   @param: name    - docker image tar file, if not set, will import all tar file in local dir
function api_image_importall()
{
    local names=$@
    if [ -z "$names" ] ; then
        names=$(ls *.tar)
    fi

    for name in ${names[@]} ; do
        api_image_import "$name"
        if [ $? -ne 0 ] ; then
            $LOG_ERROR "Failed to import: $name"
            return 1
        fi
        $LOG_INFO "Success import: $name"
    done
    return 0
}



#   @desc:  prepare install/update snap env
function snap_install_prepare()
{
    mkdir -p /var/log/sg/
    systemctl enable snapd --now
    if [ ! -L "/snap" ] ; then
        ln -s /var/lib/snapd/snap /snap
    fi
    return 0
}

#   @desc:  install a local snap pkg
#   @param: name    - snap pkg name
function snap_install()
{
    local name=$1
    local args="$2"

    $SNAP_BIN ack $name.assert
    for ((i=0;i<3;i++)); do
        $SNAP_BIN install $name.snap "$args"
        if [ $? -eq 0 ] ; then
            return 0
        fi
        sleep 20
    done
    return 1
}

#   @desc:  install snap packages in local dir
#   @param: snapd_version       -
#   @param: core_version        -
#   @param: microk8s_version    - 
function snap_install_all()
{
    local name
    for name in "${SNAP_PKGS[@]}"; do
        local args="--classic"
        local ver=$(ls ${name}*.snap|sort | tail -1 | awk -F. '{print $1}' | awk -F_ '{print $2}' )
        if [ -z "$ver" ] ; then
            $LOG_ERROR "Failed to install: ${name} , pkg not found"
            return 1
        fi

        $SNAP_BIN list | awk '{print $1,$3}' | grep $name|grep $ver
        if [ $? -eq 0 ] ; then
            $LOG_INFO "Skip install: $name $ver"
            continue
        fi

        snap_install "${name}_$ver" "$args"
        if [ $? -ne 0 ] ; then
            $LOG_ERROR "Failed to install: ${name}_$ver"
            return 1
        fi
        $LOG_INFO "Success install: ${name}_$ver"
    done

    if [ ! -e /snap/bin/microk8s ] ; then
        $LOG_ERROR "Failed to install microk8s snap pkg"
        return 1
    fi
    return 0
}



#   @desc: update cidr value or resolve cidr conflict
#   @param: [cidr]  - cidr addr(ip/mask)
function api_CIDRSet()
{
    local newcidr="$1"
    local oldcidr="$(cat /var/snap/microk8s/current/args/kube-proxy 2>/dev/null | grep -- '--cluster-cidr' | awk -F= '{print $2}')"
    if [ -z "$newcidr" ] || [ "$newcidr" == "$oldcidr" ] ; then
        $LOG_INFO "set cidr: skip $newcidr"
        return 0
    fi

    api_kubectl delete -f /var/snap/microk8s/current/args/cni-network/cni.yaml
    if [ $? -ne 0 ] ; then
        $LOG_WARN "set cidr: delete cni.yaml failed"
    fi
    $LOG_INFO "set cidr: set to $newcidr"

    api_k8sStop
    sed -i "s#--cluster-cidr=.*#--cluster-cidr=${newcidr}#" /var/snap/microk8s/current/args/kube-proxy
    sed -i "/CALICO_IPV4POOL_CIDR/{n;s#value.*#value: \"$newcidr\"#}" /var/snap/microk8s/current/args/cni-network/cni.yaml
    $LOG_DEBUG "set cidr: new cidr $(grep cluster-cidr /var/snap/microk8s/current/args/kube-proxy ) "
    $LOG_DEBUG "set cidr: new pool $(grep -2 CALICO_IPV4POOL_CIDR /var/snap/microk8s/current/args/cni-network/cni.yaml ) "

    api_k8sRestart
    api_kubectl apply -f /var/snap/microk8s/current/args/cni-network/cni.yaml
    if [ $? -ne 0 ] ; then
        sed -i "s#--cluster-cidr=.*#--cluster-cidr=${oldcidr}#" /var/snap/microk8s/current/args/kube-proxy
        sed -i "/CALICO_IPV4POOL_CIDR/{n;s#value.*#value: \"$oldcidr\"#}" /var/snap/microk8s/current/args/cni-network/cni.yaml
        $LOG_ERROR "set cidr: kubectl apply new cni.yaml failed $?"
        return 1
    fi
    return 0
}



#   @desc:  clean microk8s nodes
function core_cleannodes()
{
    local NODE_INFO=`api_kubectl get nodes`
    if [ $? -eq 0 ] ; then
        NODE_INFO=`echo "$NODE_INFO" | grep -v '^NAME' | grep -v 'localhost.localdomain'`
        if [ -n "$NODE_INFO" ] ; then
            $LOG_INFO "Found node which is not localhost.localdomain, info[$NODE_INFO]"
            local NODE_NAMES=`echo "$NODE_INFO" | awk '{print $1}'`
            for NODE_NAME in ${NODE_NAMES[@]} ; do
                api_kubectl delete nodes $NODE_NAME
                if [ $? -ne 0 ] ; then
                    $LOG_ERROR "Delete node $NODE_NAME failed"
                else
                    $LOG_INFO "Delete node $NODE_NAME successfully"
                fi
            done
        else
            $LOG_ERROR "Does not contain node not localhost.localdomain"
        fi
    else
        $LOG_ERROR "Cannot get nodes info"
    fi
}

#   @desc:  
function snap_install_post()
{
    $LOG_INFO "======   SNAP POST: RESART MICROK8S"
    $MICROK8S_BIN stop
    echo "--hostname-override=localhost.localdomain" >> /var/snap/microk8s/current/args/kubelet
    sed -i "/\[Service\]/a\RestartSec=5" /etc/systemd/system/snap.microk8s.daemon-kubelet.service
    $MICROK8S_BIN start

    $LOG_INFO "======   SNAP POST: SETUP MICROK8S"
    snap alias microk8s.kubectl kubectl
    usermod -a -G microk8s root
    usermod -a -G microk8s admin

    chown -f -R root /root/.kube
    chown -f -R admin /home/admin/.kube

    $LOG_INFO "======   SNAP POST: WAIT CONTAINERD"
    api_waitcontainerd
    if [ $? -ne 0 ] ; then
        $LOG_ERROR "Containerd not ready"
        return 1
    fi

    return 0
}

function k8s_install_post()
{
    local addons=('dns' 'ingress' 'metrics-server')

    $LOG_INFO "======   MICROK8S POST: SETUP CIDR"
    api_CIDRSet "$FAKECNI"
    if [ $? -ne 0 ] ; then
        $LOG_ERROR "set CIDR failed"
        return 1
    fi

    $LOG_INFO "======   MICROK8S POST: CLEAN NODES"
    core_cleannodes

    $LOG_INFO "======   MICROK8S POST: RESTART PODS"
    local PODS_INFO=`api_kubectl get po -n kube-system`
    if [ $? -eq 0 ] && [ -n "$PODS_INFO" ] ; then
        local PODS=`echo "$PODS_INFO" | awk '{print $1}' | grep -v '^NAME'`
        for pod_name in ${PODS[@]} ; do
            api_kubectl delete po -n kube-system $pod_name --grace-period=0 --force
            if [ $? -eq 0 ] ; then
                $LOG_INFO "Delete pod $pod_name successfully"
            else
                $LOG_ERROR "Delete pod $pod_name unsuccessfully"
            fi
        done
    else
        $LOG_ERROR "Cannot get pods info for namespace kube-system"
    fi

    $LOG_INFO "======   MICROK8S POST: WAIT READY"
    $MICROK8S_BIN status --wait-ready -t 3600

    $LOG_INFO "======   MICROK8S POST: ENABLE ADDON "
    for name in ${addons[@]} ; do
        core_addon_enable "$name"
        if [ $? -ne 0 ] ; then
            $LOG_ERROR "Microk8s enable $name failed"
        else
            $LOG_INFO "Microk8s enable $name successfully"
        fi
    done

    $LOG_INFO "======   MICROK8S POST: CREATE NS FOR SERVICE"
    $MICROK8S_BIN kubectl create ns service-gateway
    return 0
}



function init_net()
{
    if [ -f /etc/trend/clish_ready ] ; then
        return 1
    fi

    local ipaddr=$(nmcli dev show eth0 | grep IP4.ADDRESS | awk '{print $2}' | awk -F/ '{print $1}')
    if [ -n "$ipaddr" ]; then
        if [[ "$ipaddr" == 198.* ]]; then
                FAKECNI="172.16.0.0/16"
        fi
        $LOG_INFO  "First boot, skip setup IP. fakeCNI: $FAKECNI "

        return 0
    fi

    return 0
}

#   @PUBAPI:    installk8s snapd_version core_version microk8s_version
#   @desc:  install and update microk8s
#   @param: snapd_version       -
#   @param: core_version        -
#   @param: microk8s_version    - 
function cmd_install_k8s()
{
    local path="$RUNDIR"
    local usefakenet=0

    #   prepare snapd
    $LOG_INFO "======   MICROK8S INIT: INIT SNAP $path"

    init_net
    usefakenet=$?
    $LOG_INFO "Install network ret: $usefakenet"

    if [ $usefakenet -eq 2 ] ; then
        $LOG_ERROR "Install SG-Plat failed, missing network: $usefakenet"
        return 1
    fi

    snap_install_prepare

    $LOG_INFO "======   MICROK8S INIT: INSTALL SNAP"
    cd  "$path" && snap_install_all
    if [ $? -ne 0 ] ; then
        $LOG_ERROR  "installk8s: Install snap env failed, exit"
        return 1
    fi

    $LOG_INFO "======   MICROK8S INIT: SETUP SNAP"
    snap_install_post
    if [ $? -ne 0 ] ; then
        $LOG_ERROR  "installk8s: prepare import containerd failed, exit"
        return 1
    fi

    $LOG_INFO "======   MICROK8S RUN: IMPORT IMAGES"
    api_image_importall
    if [ $? -ne 0 ] ; then
        $LOG_ERROR  "installk8s: import containerd failed, exit"
        return 1
    fi

    k8s_install_post
    if [ $? -ne 0 ] ; then
        $LOG_ERROR  "installk8s: post install microk8s failed, exit"
        return 1
    fi

    # keep micro k8s runnning
    #if [ $usefakenet -eq 0 ] ; then
    #    $MICROK8S_BIN stop
    #    $MICROK8S_BIN disable
    #    nmcli con del eth0
    #    systemctl network restart
    #fi

    rm -f /opt/sg-platform/*.tar /opt/sg-platform/*.snap /opt/sg-platform/*.assert

    $LOG_INFO   "installk8s: finish"
    return 0
}

function cmd_uninstall_k8s()
{
    $MICROK8S_BIN   reset
    $SNAP_BIN  remove --purge microk8s
    $SNAP_BIN  unalias kubectl
}

function cmd_postinstall_k8s()
{
  
  $LOG_INFO "increase containerd restart speed"
  sed -ie 's/KillMode=.*/KillMode=mixed/g' /etc/systemd/system/snap.microk8s.daemon-containerd.service
  rm -f /etc/systemd/system/snap.microk8s.daemon-containerd.servicee

  $LOG_INFO "disable snap auto refresh"
  sudo usermod -a -G microk8s rain
  sudo chown -f -R rain ~/.kube
  
  echo "127.0.0.1 api.snapcraft.io" >>/etc/hosts

  /usr/bin/snap set system refresh.metered=hold
  /usr/bin/snap set system refresh.timer=fri5,23:00-01:00
  /usr/bin/snap set core experimental.refresh-app-awareness=true
  /usr/bin/snap set system proxy.http="http://127.0.0.1:1111"
  /usr/bin/snap set system proxy.https="http://127.0.0.1:1111"
  /usr/bin/snap set system refresh.hold="2030-01-01T01:00:00-01:00"

  if ! grep 'refresh.hold' /var/spool/cron/root >/dev/null 2>&1; then
    echo '30 12 * * * /usr/bin/snap set system refresh.hold="2030-01-01T01:00:00-01:00"' >>/var/spool/cron/root
  fi
  #systemctl stop snapd.service
  #systemctl mask snapd.service

  sed -i 's/ulimit -n 65536/ulimit -n 204800/' /var/snap/microk8s/current/args/containerd-env
  /snap/bin/microk8s.kubectl -n ingress patch configmap nginx-load-balancer-microk8s-conf --patch '{    "data": { 
        "proxy-body-size": "10m",
        "ssl-ciphers": "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA:ECDHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES256-SHA256:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:DES-CBC3-SHA",
        "ssl-protocols": "TLSv1.2 TLSv1.3"  } }'
}

case $1 in
    install) #   offline install microk8s 
    cmd_install_k8s
    cmd_postinstall_k8s
    exit $?
    ;;


    uninstall)
    cmd_uninstall_k8s
    exit $?
    ;;

    kubectl)
    shift
    api_kubectl $@
    exit $?
    ;;

    setcidr)    #   cidraddr
    api_CIDRSet "$2"
    exit $?
    ;;

    importImage)  #   [image1.tar image2.tar ...]
    shift
    api_image_importall $@
    exit $?
    ;;
esac

exit 0

