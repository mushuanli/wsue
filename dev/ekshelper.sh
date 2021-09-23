#!/bin/bash

#   this script used to manage aks/eks kubernetes resource.
#   it runned in amazon/aws-cli  or azure-cli.
#   first config docker container connected to azure| aws
#   then use kube
# predefined name:
# 	my_image_prefix:	docker image name prefix
# 	app_debug.log:		in /var/log/, store app debug log
#	app:			pod main container name
#	eks_name:		aws/azure eks name
#       EKS_APP_NAMESPACE:      kubenertes namespace for app

function eks_help() {
    echo 'params:'
    echo "  h|-h:                                      show help."
    echo "------------ pod param ------------"
    echo "cp eke podname localpath:  filename          cp app_debug.log from pod to local"
    echo "cpa eke:                                     cp all backend pod logs to loal log_xxx.txt"
    echo "grep eke search_string [file fileter]        grep in all pod"
    echo "exec eke podname t pingparam:                exec test cmd on local pod"
    echo "sh eke podname                               enter shell in pod"
    echo "restart eke                                  restart all backend pod"
    echo "------------ env param ------------"
    echo "        add new zone step:                   "
    echo "            e init zone_aws_profile"
    echo "            e eks  aws_region eks_name zone_aws_profile"
    echo "  e|-e init aws_profile:                     init aws_profile"
    echo "  e|-e eks  aws_profile eks_region eks_name: init kubeconfig in eks_region:eks_name + aws_profile"
    echo "  e|-e arn:                                  view arn info"
    echo "  d|dev:                                     connect dev eks"
    echo "  s|stg|stage:                               connect stage eks"
    echo "  p|prod|product zone_name:                  connect product eks in zone_name"
    echo "  gitkey:                                     update git key"
    echo "------------------------------------"
    echo " other command: "
    echo ' *  wget --header="Content-Type:application/json" --post-data="{\"mode\":\"debug\"}" http://127.0.0.1:8081/internal/atl/api/v1/ping'
}


function aws_init() {
    local name="$1"
    echo "preconfig aws profile $name"
    aws configure --profile "$name"
}

function eks_init() {
    if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ] ; then
        echo "param: awsprofile region kubename"
    else
        local cmdack=`aws eks --region $2  update-kubeconfig --name $3 --profile $1 `
        echo "cmdack: $cmdack"
        local arn1=${cmdack#* context }
        local arn=${arn1% *}
        echo "add this alias into ~/.bashrc:"
        echo "alias eks$1='kubectl config use-context $arn && swteks p' "
    fi
}

#   param:  ekcmd   index
#   return: pod name
function eks_id2pod(){
    local ekcmd=$1
    local index=$2
    case "$index" in
        1|2|3)
        echo $($ekcmd get po |grep Running | awk '{print $1}' | grep my_image_prefix | grep -v daemon | head -"$index" | tail -1)
        ;;

        *)
        echo $($ekcmd get po |grep Running | awk '{print $1}' | grep my_image_prefix | grep daemon | tail -1)
        ;;
    esac
}

function eks_shell(){
    local ekcmd=$1
    local index=$2
    case "$index" in
    0|1|2|3)
        index=$(eks_id2pod $ekcmd $index)
    esac
    echo "POD: $index"
    "$ekcmd" exec -it $index -c app -- sh
}

function eks_cp(){
    local ekcmd=$1
    local index=$2
    local dst="$3"
    local files=${4:="app_debug.log"}

    case "$index" in
    0|1|2|3)
        index=$(eks_id2pod $ekcmd $index)
    esac
    echo "POD: $index"
    "$ekcmd" cp "$index:/var/log/$files" "$dst"
}

function cpa(){
    ekcmd=$1
    shift
    echo "params : $ekcmd - $@"

    for pname in $( $ekcmd get pods | grep my_image_prefix | awk '{print $1}' ) ; do
        shortname=$(echo $pname | awk -F- '{print $NF}')
        echo "pod is $pname $shortname"
        $ekcmd cp $pname:/var/log/app_debug.log -c log log_${shortname}.txt
    done
}

function podgrep(){
    ekcmd=$1
    grepstr="$2"
    grepfiles="$3"
    podshortname="$4"
    shift
    echo "params : $ekcmd - str:$grepstr files:$grepfiles pod:$podshortname"

    for pname in $( $ekcmd get pods | grep my_image_prefix | awk '{print $1}' | grep "$podshortname" ) ; do
        shortname=$(echo $pname | awk -F- '{print $NF}')
        echo "pod is $pname $shortname"
        $ekcmd exec $pname -c log -- grep -r "$grepstr" "/var/log/$grepfiles"
    done
}

function setup() {
    case "$1" in
        #---------------------------------------------- pod param ------------------
        cp)
            echo "$2" cp $3:/var/log/app_debug.log "$4"
            "$2" cp $3:/var/log/app_debug.log "$4"
            return 1
            ;;

        cpa)
            shift
            cpa "$@"
            return 1
            ;;

        grep)
            shift
            podgrep "$@"
            return 1
            ;;

        sh)
            eks_shell "$2" "$3"
            ;;

        exec)
            echo "$2" exec $3 -c app node utils/testapi.js "$4" "$5" "$6" "$7"
            "$2" exec $3 -c app node utils/testapi.js "$4" "$5" "$6" "$7"
            return 1
            ;;
        
        restart)
            "$2" rollout restart deployment $("$2" get deployment | grep my_image_prefix | awk '{print $1}')
            return 1
            ;;

            #---------------------------------------------- env param ------------------
        e|-e)
            case "$2" in
                init)
                    aws_init "$3"
                    return 0
                    ;;

                eks)
                    echo "$3 - $4 - $5 - $6"
                    eks_init "$3" "$4" "$5" "$6"
                    return 0
                    ;;

                arn)
                    aws sts get-caller-identity
                    aws iam list-users --profile "$3"
                    ;;
                *)
                    eks_help
                    return 0
            esac
            ;;

        d|dev)
            if [[ "$EKS_PROM_NAME" == "Aks" ]] ; then
                export PS1='\033[1;93;44mAksD '$EKSNAME' \033[0m\[\033[01;32m\]@\[\033[01;34m\]\w\[\033[00m\]\$'
            else
                export PS1='\[\033[01;32m\]EksD '$EKSNAME' \[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\]\$'
            fi
            EKS_APP_NAMESPACE=app-int
            echo "Need set to new ENV:"
            ;;

        s|stg|stage)
            if [[ "$EKS_PROM_NAME" == "Aks" ]] ; then
                export PS1='\033[1;93;44mAksS '$EKSNAME' \033[0m\[\033[01;32m\]@\[\033[01;34m\]\w\[\033[00m\]\$'
            else
                export PS1='\[\033[01;32m\]EksS '$EKSNAME' \[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\]\$'
            fi
            EKS_APP_NAMESPACE=app-stg

            echo "Need set to new ENV:"
            ;;

        p|prod|product)
            if [[ "$EKS_PROM_NAME" == "Aks" ]] ; then
                export PS1='\033[1;93;44mAksP '$EKSNAME' \033[0m\[\033[01;32m\]@\[\033[01;34m\]\w\[\033[00m\]\$'
            else
                export PS1='\[\033[01;32m\]EksP '$EKSNAME' \[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\]\$'
            fi
            EKS_APP_NAMESPACE=app-prod

            echo "Need set to new ENV:"
            ;;


        gitkey)
            eval GITRSA='~/.ssh/gitrsa'
            email=$1
            if [[ -z "$1" ]] ; then
                email='rain_li@trendmicro.com'
            fi
            echo "will generate git key: $GITRSA for: $email"
            rm -f "$GITRSA"
            ssh-keygen -t rsa -b 4096 -C $email -f "$GITRSA" -q -N ''
            echo "add $GITRSA to local"
            eval $(ssh-agent -s)
            ssh-add "$GITRSA"
            echo "add pub to github"
            cat ${GITRSA}.pub
            exit 0
            ;;

        *)
            eks_help
            return 0
    esac

    eksnow
    return 0
}


setup "$1" "$2" "$3" "$4" "$5" "$6" "$7"
if [[ $? -eq 1 ]] ; then
    return 
fi

export EKS_APP_NAMESPACE
function ekt() {
    kubectl -n $EKS_APP_NAMESPACE "$@"
}

export -f ekt

#   can use ekt get po to fast run cmd

function aws_cmd() {


    echo " ======== AZURE CONNECT AKS STEP ==================="
    # init AKS
    echo "az login"
    echo "az aks get-credentials --resource-group my_resource-int --name aks-int2 --admin"

# UAE AZURE
    echo "az account set --subscription 0000000000-xxxxxx-xxxxxx-xxxxxxxxxx"
    echo "az aks get-credentials --resource-group my_resource-int --name aks-app-int"

    echo " ======== AWS CONNECT EKS STEP ==================="
    echo " aws configure --profile awsprofile"
    echo " aws eks --region us-east-1  update-kubeconfig --name eks_name --profile awsprofile"
    echo "alias ekseup='kubectl config use-context arn:aws:eks:eu-central-1:0000000000:cluster/eks_name-eu1' "
    #   use ekseup to fast switch to this kubernetes context

    echo "
export PS1='\033[1;93;44mEks\033[0m\[\033[01;32m\]@\[\033[01;34m\]\w\[\033[00m\]\$'
alias eksnow='kubectl config current-context'


Eks@~#cat ~/.bashrc
export PS1='\033[1;93;44mEks\033[0m\[\033[01;32m\]@\[\033[01;34m\]\w\[\033[00m\]\$'
alias eksnow='kubectl config current-context'

alias eksnow='kubectl config current-context'
alias ekseup='kubectl config use-context arn:aws:eks:eu-central-1:0000000000:cluster/eks_name-eu1'  #   switch to new eks context


Eks@~#cat ~/.aws/config
[profile usd]
region = us-east-1
output = json
[profile usp]
region = us-east-1
output = json
[profile eup]
region = eu-central-1
output = json

"
}


