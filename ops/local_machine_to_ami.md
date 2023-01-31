从OVA到ESXi AMI
# 介绍
## 背景
在云原生的大环境下，有时我们想：
* 把本地运行正常的Linux机器部署到云上;
* 或是在本地开发验证，等成功后再部署到云上。

这时就需要把本地的OVA文件上传到云上 - 这里假设本地机器运行在ESXI 环境中，远程是 AWS AMI。远程是 AZURE 那么可以参考另外一篇文章。
注： AWS AMI 对Linux环境的内核有要求，一般要求版本默认内核。

## ESXi和OVA介绍
VMware vSphere（VMware ESXi）是一种裸金属架构的虚拟化技术。虚拟机(基于linux修改)直接运行在系统硬件上，创建硬件全仿真实例，被称为“裸机”，适用于多台机器的虚拟化解决方案，而且可以图形化操作。
在ESXi安装好以后，我们可以通过vSphere Client 远程连接控制，在ESXi 服务器上创建多个VM（虚拟机），在为这些虚拟机安装好Linux /Windows Server 系统使之成为能提供各种网络应用服务的虚拟服务器，ESXi 也是从内核级支持硬件虚拟化，运行于其中的虚拟服务器在性能与稳定性上不亚于普通的硬件服务器，而且更易于管理维护。

OVA文件是由以下人员使用的虚拟设备虚拟化 应用程序，例如VMware Workstation和Oracle VM Virtualbox。 它是一个软件包，其中包含用于描述虚拟机的文件，其中包括. OVF 描述符文件，可选清单（.MF）和证书文件以及其他相关文件。
OVA是单个压缩文件，里面包含其他多个虚拟机相关信息，例如硬件配置、硬盘等等。通过 OVA文件, 可以快速在其他机器上导入一个硬件配置、硬盘内容都一样的虚拟机,相当于快速的把一台机器从一个地方复制到另外一个地方，或是在多个地方复制相同的机器。

ESXi可以通过ovftool导出OVA，具体方法可见另外一篇文章： 《通过ESXi生成ISO的OVA文件》

## AWS AMI介绍
亚马逊云计算服务（英语：Amazon Web Services，缩写为AWS）是亚马逊公司旗下的子公司，向个人、企业和政府提供按需即用云计算平台以及应用程序接口，并按照使用量计费。这些云计算Web服务通过亚马逊网络服务的服务器集群提供分布式计算处理能力和软件工具。

Amazon Machine Image (AMI) 是一种包含软件配置 (例如，操作系统、应用程序服务器和应用程序) 的模板。通过 AMI，您可以启动实例，实例是作为云中虚拟服务器运行的 AMI 的副本。您可以启动多个 AMI 实例, 您的实例会保持运行，直到您停止、休眠或终止实例，或者实例失败为止。如果实例失败了，您可以从 AMI 启动一个新实例。
简单点可以看成是 包括内核和硬件的Docker Image, 实例可以看成运行在不同硬件的docker containers。

## AWS CLI介绍
AWS Command Line Interface (AWS CLI) 是一种开源工具，让您能够在命令行 Shell 中使用命令与 AWS 服务进行交互。仅需最少的配置，即可使用 AWS CLI 开始运行命令，以便从终端程序中的命令提示符实现与基于浏览器的 AWS Management Console所提供的功能等同的功能。
AWS CLI 版本 2 是 AWS CLI 的最新主版本，支持所有最新功能。
可以从这里获取最新的 AWS CLI: https://docs.aws.amazon.com/zh_cn/cli/latest/userguide/getting-started-install.html

### 配置AWS CLI
在使用AWS CLI前需要配置访问AWS的credentials。可以通过下面命令配置：
```
aws configure
```
如下例子：
```
aws configure 

AWS Access Key ID [None]: ANOTREALACCESSKEYID
AWS Secret Access Key [None]: ANOTREALSECRETACCESSKEY
Default region name [None]: eu-west-1
Default output format [None]: json
```

如果一个环境需要配置不同的 credentials, 可以使用 --profile xxxx 声明。
配置成功后所有访问都是通过这个credentials进行。

# 步骤
使用OVA生成 AMI 包括一下步骤：
* 在Linux机器内使用发行版本默认内核
* 在Linux机器内禁止进行IP配置
* 在Linux机器内起cronjob获取SSH登陆公钥，并保存到默认登陆账户下
* 生成OVA
* 上传OVA到S3路径下
* 创建IAM ROLE Policy
* Import S3 Image，并等待完成
* 打tag

以下将详细介绍。

## 在Linux机器内使用发行版本默认内核
AMI不只是我们上传的系统映像，还会加入AWS的映像和配置文件。
AWS会在AMI映像中运行自己的配置和代码，而这些都是基于Linux标准发行版本的，所以在上传OVA前需要使用默认发行版本的Linux内核。

## 在Linux机器内禁止进行IP配置
AMI运行映像时会接管网络配置，所以我们不需要进行IP配置，就是配置了也会失败。

## 在Linux机器内起cronjob获取SSH登陆公钥，并保存到默认登陆账户下
当AMI启动后，SSH公钥信息可以通过http://169.254.169.254/latest/api/token 获取
但是当AMI启动时获取不一定成功，所以可以在cronjob中运行下面代码，保证可以SSH 密钥登陆

```
        local tmpfile=$(mktemp)
		TOKEN=`curl -X PUT "http://169.254.169.254/latest/api/token" -H "X-aws-ec2-metadata-token-ttl-seconds: 21600" 2>/dev/null` \
		&& curl -H "X-aws-ec2-metadata-token: $TOKEN" -v http://169.254.169.254/latest/meta-data/public-keys/0/openssh-key > "$tmpfile" 2>/dev/null
		if grep -q ^ssh-rsa  "$tmpfile" ; then
            if ! diff -q "$tmpfile" /home/rain/.ssh/authorized_keys; then
                cat "$tmpfile" > /home/rain/.ssh/authorized_keys
                chmod 644 /home/rain/.ssh/authorized_keys
                chown rain.rain /home/rain/.ssh/authorized_keys
                echo "update rain auth succ"
            fi
		fi
        rm -f "$tmpfile"
```

## 生成OVA
关机后生成OVA,但是注意需要弹出CDROM,要不新环境导入时会报少文件。
```
  vmName=MyVM
  govc vm.power -off -M "$vmName"       # 断电
  govc device.cdrom.eject -vm "$vmName" # 弹出CDROM
  # 在output_dir目录下生成OVA文件
  /opt/ovftool/ovftool -dm=thin --noSSLVerify --powerOffSource "$VI_URL/$vmName" "output_dir/$vmName.ova"
```

## 上传OVA到S3路径下
直接通过aws s3 cp上传
```
aws s3 cp "$localfile" "$s3path"
```

## 创建IAM ROLE Policy

S3Bucket = S3文件完整路径，但是不包括最后一个 /后面部分，例如：
完整的S3文件是：
```
upload-int.rain.li.com/mys3/2.0.7.10433/MYS3.ova
```
那么：
```
S3Bucket=upload-int.rain.li.com/mys3/2.0.7.10433
```
配置脚本如下：

```
  echo "AMI Prepare Role"
  aws iam get-role --role-name "$CFG_IAM_ROLENAME"
  if [ $? -eq 0 ] ; then
    echo "AMI Prepare Role:  Already exist"
  else
    echo "AMI Prepare Role:  Create"
    echo '{
            "Version": "2012-10-17",
            "Statement": [
            {
                "Effect": "Allow",
                "Principal": { "Service": "vmie.amazonaws.com" },
                "Action": "sts:AssumeRole",
                "Condition": {
                    "StringEquals":{
                        "sts:Externalid": "vmimport"
                    }
                }
            }
            ]
        }' >"/tmp/trust-policy-$BRANCH.json"
    aws iam create-role --role-name "$CFG_IAM_ROLENAME" --assume-role-policy-document "file:///tmp/trust-policy-$BRANCH.json"
    rm -f "/tmp/trust-policy-$BRANCH.json"


  echo "AMI Prepare Policy"

  echo '{
        "Version":"2012-10-17",
        "Statement":[
        {
            "Effect":"Allow",
            "Action":[
            "s3:GetBucketLocation",
            "s3:GetObject",
            "s3:ListBucket"
            ],
            "Resource":[
            "arn:aws:s3:::'"$S3Bucket"'",
            "arn:aws:s3:::'"$S3Bucket"'/*"
            ]
        },
        {
            "Effect":"Allow",
            "Action":[
            "ec2:ModifySnapshotAttribute",
            "ec2:CopySnapshot",
            "ec2:RegisterImage",
            "ec2:Describe*"
            ],
            "Resource":"*"
        }
    ]
    }' >"/tmp/role-policy-${BRANCH}.json"
  $CFG_AWS_CLI iam put-role-policy --role-name "$CFG_IAM_ROLENAME" --policy-name "$CFG_IAM_POLICYNAME" --policy-document "file:///tmp/role-policy-${BRANCH}.json"
  rm -f "/tmp/role-policy-${BRANCH}.json"
```

## Import S3 Image，并等待完成, 然后打 tag
如果S3路径是
upload-int.rain.li.com/mys3/2.0.7.10433/MYS3.ova
那么这里的s3bucket 是域名部分，s3key是其他部分（去掉最开始的/）
```
s3bucket=upload-int.rain.li.com
s3key=mys3/2.0.7.10433/MYS3.ova
```

AWS对所有导入的AMI都使用image-xxxxx, 难以区分，所以导入成功后需要打tag, 方便维护。

导入和打标签脚本如下：
```
  echo "AMI: begin importImage"

  local import_info
  echo '
   [{
      "Description": "'"$amiName"'",
      "Format": "ova",
      "UserBucket": {
         "S3Bucket": "'"$s3bucket"'",
         "S3Key": "'"$s3key"'"
      }
   }]' >"/tmp/containers-${BRANCH}.json"

  import_info=$(aws ec2 import-image --description "$amiName" --disk-containers "file:///tmp/containers-${BRANCH}.json" 2>>"$LOG_FILE")
  local retcode=$?
  rm -f "/tmp/containers-${BRANCH}.json"

  echo "AMI: import $amiName ret: $retcode - $import_info"
  if [ $retcode -ne 0 ] ; then
    return 1
  fi

  local import_id=$(echo "$import_info" | jq '.ImportTaskId' | tr -d '"')
  local ami_id
  local snapshot_id

  while true; do
    local import_result=$(aws ec2 describe-import-image-tasks --import-task-ids $import_id 2>>"$LOG_FILE")
    local import_status=$(echo "$import_result" | jq '.ImportImageTasks[0].Status' | tr -d '"')
    if [ "$import_status" == "completed" ] ; then
      ami_id=$(echo "$import_result" | jq '.ImportImageTasks[0].ImageId' | tr -d '"')
      snapshot_id=$(echo "$import_result" | jq '.ImportImageTasks[0].SnapshotDetails[0].SnapshotId' | tr -d '"')
      break
    fi
    log "Generate AMI $import_id    Status is $import_status"
    sleep 30
  done

  aws ec2 create-tags --resources "$ami_id" --tags Key=Name,Value="$amiName" 2>>"$LOG_FILE"
  aws ec2 create-tags --resources "$snapshot_id" --tags Key=Name,Value="$amiName" 2>>"$LOG_FILE"

```

## 成功
到这里可以直接使用AMI。
