从OVA到Azure Image
# 介绍
## 背景
在云原生的大环境下，有时我们想：
* 把本地运行正常的Linux机器部署到云上;
* 或是在本地开发验证，等成功后再部署到云上。

这时就需要把本地的OVA文件上传到云上 - 这里假设本地机器运行在ESXI 环境中，远程是 AZURE IMAGE。远程是 AWS AMI 那么可以参考另外一篇文章。
注： AZURE IMAGE 对Linux环境的内核无要求，但是要求系统运行WALinuxAgent和cloud-init。

## ESXi和OVA介绍
VMware vSphere（VMware ESXi）是一种裸金属架构的虚拟化技术。虚拟机(基于linux修改)直接运行在系统硬件上，创建硬件全仿真实例，被称为“裸机”，适用于多台机器的虚拟化解决方案，而且可以图形化操作。
在ESXi安装好以后，我们可以通过vSphere Client 远程连接控制，在ESXi 服务器上创建多个VM（虚拟机），在为这些虚拟机安装好Linux /Windows Server 系统使之成为能提供各种网络应用服务的虚拟服务器，ESXi 也是从内核级支持硬件虚拟化，运行于其中的虚拟服务器在性能与稳定性上不亚于普通的硬件服务器，而且更易于管理维护。

OVA文件是由以下人员使用的虚拟设备虚拟化 应用程序，例如VMware Workstation和Oracle VM Virtualbox。 它是一个软件包，其中包含用于描述虚拟机的文件，其中包括. OVF 描述符文件，可选清单（.MF）和证书文件以及其他相关文件。
OVA是单个压缩文件，里面包含其他多个虚拟机相关信息，例如硬件配置、硬盘等等。通过 OVA文件, 可以快速在其他机器上导入一个硬件配置、硬盘内容都一样的虚拟机,相当于快速的把一台机器从一个地方复制到另外一个地方，或是在多个地方复制相同的机器。

ESXi可以通过ovftool导出OVA，具体方法可见另外一篇文章： 《通过ESXi生成ISO的OVA文件》

在创建AZURE IMAGE过程中，仅使用 OVA 内的硬盘文件，并且需要转换成原始大小的VHD格式（不压缩，没有thin 模式，并且1MB对齐）
从这个点看，使用其他虚拟机软件创建然后转换成 VHD 也是可以的，但是为了方便继续使用OVA说明。

## VHD 格式介绍
VHD（虚拟硬盘，Virtual Hard Disk）及其后续的VHDx是一种虚拟硬盘（它们通常被用作虚拟机的硬盘）文件格式。这些文件中有已分区的虚拟磁盘和虚拟文件系统，而文件系统内又有虚拟文件和虚拟文件夹。VHD适用于微软的Hypervisor以及Hyper-V。

AZURE IMAGE要求上传的是 VHD 格式，并且是是原始大小（不能是thin模式），并且是1MB对齐。
原始大小的意思就是：如果硬盘分成10GB，那么对应的VHD文件也是10GB+其他信息，即使这个硬盘内实际空间利用率很低。
由于VHD得原始大小，那么当创建虚拟机时应该尽量少分配硬盘空间，这样才能减少上传的流量，加快上传速度。然后系统中挂data分区。

## AZURE IMAGE介绍
AZURE IMAGE类似于 AWS AMI, 是一种包含软件配置 (例如，操作系统、应用程序服务器和应用程序) 的模板。通过 AZURE IMAGE，您可以启动实例，实例是作为云中虚拟服务器运行的 IMAGE 的副本。您可以启动多个 IMAGE 实例, 您的实例会保持运行，直到您停止、休眠或终止实例，或者实例失败为止。如果实例失败了，您可以从 IMAGE 启动一个新实例。
简单点可以看成是 包括内核和硬件的Docker Image, 实例可以看成运行在不同硬件的docker containers。
AZURE IMAGE与 AWS AMI 不同的是，AWS AMI会直接修改运行环境，在内部加入AWS APP，而AZURE IMAGE不会。这两种实现有利有弊：
* AWS 可以减少创建AMI的难度，因为创建IMAGE时不需要考虑什么配置才能让IMAGE在AMI上运行起来，这些都被AWS Hack进去的应用程序实现了。AZURE IMAGE则相反。
* AWS 对 IMAGE有严格要求，例如内核版本不能变，而 Azure Image没有这要求。
* Azure Image要求安装的两个软件包（WALinuxAgent和cloud-init）会影响正常系统运行，所以如果不是用于Azure Image不要安装这两个软件包。安装后禁止也不行，还得改配置文件。

## 工具
### AZURE CLI介绍
Azure CLI是一个跨平台的命令行工具，用于管理和维护Microsoft Azure上的服务和资源。 类似与PowerShell, Azure CLI提供了从命令行使用和管理Azure服务和资源的方法。
Azure CLI 版本 2 是 Azure CLI 的最新主版本，支持所有最新功能。
可以从这里获取最新的 Azure CLI: https://learn.microsoft.com/zh-cn/cli/azure/install-azure-cli

### 登陆Azure CLI
在使用Azure CLI前登陆。可以通过下面命令登陆：
```
az login
```
如下例子：
```
 az login
To sign in, use a web browser to open the page https://microsoft.com/devicelogin and enter the code HSEE3MJME to authenticate.

```

然后就可以点开上面的链接登陆，登陆后登陆信息会长期保存，但是如果失效那么需要再次登陆。

### azcopy介绍
azcopy 是一个工具，用于将本地文件复制到azure blob(对应aws S3)
下载地址： https://learn.microsoft.com/en-us/azure/storage/common/storage-use-azcopy-v10

# 步骤
使用OVA生成 AMI 包括一下步骤：
* 限制Linux机器的硬盘大小(必须1MB整数倍，但是正常默认就是)，并设置启动时挂上外部硬盘（从image创建虚拟机时指定的数据盘）。
* 在Linux机器内禁止进行IP配置，安装和配置 WALinuxAgent和cloud-init - 注意正常本地运行版本不要安装，要不会影响系统功能。
* 生成OVA并转换成固定大小的VHD。
* 上传VHD到Azure Disk
* 从Azure Disk 创建 Azure Image
* 清除 Azure Disk

以下将详细介绍。

## 限制Linux机器的硬盘大小, 并设置启动时挂上外部硬盘
由于VHD不支持thin mode，而且VHD文件需要上传到 AZURE, 为了加快上传速度，机器硬盘应该尽量小，运行AZURE IMAGE时再添加数据盘
由于块设备会动态添加，所以建议LINUX使用 LVM 硬盘管理方式

启动后可以使用lsblk获取所有块设备，并添加新块设备
```
sudo lsblk  | grep -w disk |grep -v ^fd| awk '{print $1}'
```

## 在Linux机器内禁止进行IP配置，安装和配置 WALinuxAgent和cloud-init
AMI运行映像时会接管网络配置，所以我们不需要进行IP配置，就是配置了也会失败。
WALinuxAgent - Microsoft Azure Linux Agent (waagent) 管理 Linux 配置和 VM 与 Azure Fabric 控制器的交互。
可以从 https://github.com/Azure/WALinuxAgent 获取，同时网络上也有对应的发行版本包，例如可以从openlogic 可以得到 rpm包。
cloud-init  - 这是云实例上使用的一组脚本，例如把ssh公钥同步到云实例等。但是在正常环境中安装这个软件包会带来其他配置问题（网络、主机名等）。发行版本中会自带安装包。

安装完后需要配置：
```
  # 打开Azure 虚拟机的串口输出
    sed -i -e 's/crashkernel=auto/console=ttyS0 earlyprintk=ttyS0 numa=off/g; s/rhgb//; s/quiet//' /etc/default/grub
    rm -f /etc/default/grube
    grub2-mkconfig -o /boot/grub2/grub.cfg

  # 加强SSH登陆安全性
  sed -i 's/PermitRootLogin yes/PermitRootLogin no/g' /etc/ssh/sshd_config
  sed -i 's/#UseDNS no/UseDNS no/g' /etc/ssh/sshd_config
  sed -i 's/#PasswordAuthentication yes/PasswordAuthentication no/g' /etc/ssh/sshd_config

  # 关闭SELINUX
  setenforce 0
  sed -i 's#SELINUX=enforcing#SELINUX=disabled#g' /etc/selinux/config

  # 允许waagent和cloud-init接管网络
  echo -e "NETWORKING=yes \nHOSTNAME=localhost.localdomain" >>/etc/sysconfig/network
  nmcli --fields DEVICE,UUID con show | grep -w eth0 | awk '{print $2}' | xargs -I{} nmcli conn delete {}
  cat >/etc/sysconfig/network-scripts/ifcfg-eth0 <<EOF
  DEVICE=eth0
  ONBOOT=yes
  BOOTPROTO=dhcp
  TYPE=Ethernet
  USERCTL=no
  PEERDNS=yes
  IPV6INIT=no
EOF
  ln -s /dev/null "$rootdir"/etc/udev/rules.d/75-persistent-net-generator.rules

  # 加载azure上需要的文件系统驱动
  echo -e "udf\nvfat" >/etc/modules-load.d/azure.conf

  # 配置waagent和cloud-init
  sed -i 's/Provisioning.UseCloudInit=n/Provisioning.UseCloudInit=y/g' /etc/waagent.conf
  sed -i 's/Provisioning.Enabled=y/Provisioning.Enabled=n/g' /etc/waagent.conf
  sed -i 's/ResourceDisk.Format=y/ResourceDisk.Format=n/g' /etc/waagent.conf
  sed -i 's/ResourceDisk.EnableSwap=y/ResourceDisk.EnableSwap=n/g' /etc/waagent.conf

  sed -i '/ - mounts/d' /etc/cloud/cloud.cfg
  sed -i '/ - disk_setup/d' /etc/cloud/cloud.cfg
  sed -i '/cloud_init_modules/a\\ - mounts' /etc/cloud/cloud.cfg
  sed -i '/cloud_init_modules/a\\ - disk_setup' /etc/cloud/cloud.cfg
  cat >/etc/cloud/cloud.cfg.d/91-azure_datasource.cfg <<EOF
datasource_list: [ Azure ]
datasource:
    Azure:
        apply_network_config: False
EOF

  if ! grep -q cloud-init-output.log /etc/cloud/cloud.cfg.d/05_logging.cfg; then
    cat >>/etc/cloud/cloud.cfg.d/05_logging.cfg <<EOF

# This tells cloud-init to redirect its stdout and stderr to
# 'tee -a /var/log/cloud-init-output.log' so the user can see output
# there without needing to look on the console.
output: {all: '| tee -a /var/log/cloud-init-output.log'}
EOF
  fi
  sed -i '/network/d' /etc/cloud/cloud.cfg.d/99-custom-networking.cfg
```
这部分配置成功后，azure上实例启动时azure能够正确接管实例，不然需要打开azure实例的串口，登陆进去看看是什么情况，
常见的是/var/lib/cloud/instance 不是符号连接。
与AWS AMI相同的是：云会接管网卡配置，
不同的是：
* AWS AMI需要自己获取SSH公钥，但是azure 里面 cloud-init会自动获取。
* AZURE提供串口调试，会更方便些，AWS AMI没有。
* AWS AMI会用自己的文件替换实例中某些文件（例如/etc/ssh/）,而AZURE IMAGE不会。

## 生成OVA并转换成固定大小的VHD

关机后生成OVA,但是注意需要弹出CDROM,要不新环境导入时会报少文件。
```
  vmName=MyVM
  govc vm.power -off -M "$vmName"       # 断电
  govc device.cdrom.eject -vm "$vmName" # 弹出CDROM
  # 在output_dir目录下生成OVA文件
  /opt/ovftool/ovftool -dm=thin --noSSLVerify --powerOffSource "$VI_URL/$vmName" "output_dir/$vmName.ova"

  # 从OVA中提取硬盘文件
  rm -rf "/tmp/vmdk-$version"
  mkdir -p "/tmp/vmdk-$version"

  tar xvf "output_dir/$vmName.ova" -C "/tmp/vmdk-$version"
  local vmdkfile=$(find "/tmp/vmdk-$version" -type f -name "*disk*.vmdk" | head -1)

  # 使用VirtualBox把vmdk转换成固定大小 VHD
  sed -i '/'"$version"'/d' /root/.config/VirtualBox/VirtualBox.xml
  VBoxManage clonehd "$vmdkfile" "$local_vhd_file" -format VHD --variant Fixed
  rm -rf "/tmp/vmdk-$version"
```

## 上传VHD到Azure Disk，并创建Azure Image
Azure Image是基于 Azure Disk创建的，所以：
* 创建Azure Disk(与本地vhd同样大小)
* 使用azcopy 把本地文件复制到 Azure Disk
* 根据AzureDisk创建Azure Image

具体方法如下：
```
  # 切换AZURE订阅组
  subscript_id=$(az account list -o table | grep "$AZURE_SUBSCRIPT_NAME" | awk -FAzureCloud '{print $2}' | awk '{print $1}')
  az account set --subscription "$subscript_id"

  # 创建azure 系统 disk, 需要指定系统参数, hyper-v-generation V1是普通disk, V2是EFI启动disk
  local_size=$(stat -c %s "$local_vhd_file")
  az disk create -n myapp -g MyResourceGroup -l uaenorth --for-upload --upload-size-bytes $local_size --sku standardssd_lrs --hyper-v-generation V1 --os-type Linux

  # 创建 256GB的数据azure disk
  az disk create -g MyResourceGroup -n myapp-data --size-gb 256

  # 打开系统disk的写入权限,并复制
  az disk grant-access -n myapp -g MyResourceGroup --access-level Write --duration-in-seconds 86400
  accessSas=$(az disk grant-access -n myapp -g MyResourceGroup --access-level Write --duration-in-seconds 86400| awk '/"accessSas":/{print $2}'|sed 's/"//g')
  azcopy copy --blob-type PageBlob "$local_vhd_file" "$accessSas"
  az disk revoke-access -n myapp -g MyResourceGroup

  # 根据系统disk和data-disk创建image，创建时也需要指定是否启用EFI启动，启动方式不正确无法启动
  az image create -g MyResourceGroup -n MyImage \
    --os-type Linux --hyper-v-generation V1 \
    --source myapp --data-disk-sources myapp-data
```

## 成功
到这里可以直接使用Azure Image。
