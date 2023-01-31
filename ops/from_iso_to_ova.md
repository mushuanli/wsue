从ISO通过ESXi生成OVA

#   介绍
##   使用场景
如果某些软件足够大，发布时需要跟操作系统一起发布，那么发布 OVA 相比发布 ISO 是一个更简单的方法：
* ESXi运行环境在大部分企业中都是可得的，具有通用性。
* 软件需要一定的硬件环境，而使用 OVA, 那么由于 OVA 被 ESXi导入，硬件环境的不同已经被 ESXi隔离，可以避免硬件驱动不兼容问题。
* 软件运行需要一定的硬件配置（最低的CPU、内存、硬盘要求），在 OVA中可以指定硬件配置，避免硬件配置不足无法运行的情况。
* ISO安装后一般还有配置的过程，而我们在基本配置完成后再导出OVA，那么就可以避免用户配置不当导致的运行问题，并且由于统一了基本配置，对后续的支持或对用户部署来说都是简化了。

##   ESXi与OVA介绍
VMware vSphere（VMware ESXi）是一种裸金属架构的虚拟化技术。虚拟机(基于linux修改)直接运行在系统硬件上，创建硬件全仿真实例，被称为“裸机”，适用于多台机器的虚拟化解决方案，而且可以图形化操作。
在ESXi安装好以后，我们可以通过vSphere Client 远程连接控制，在ESXi 服务器上创建多个VM（虚拟机），在为这些虚拟机安装好Linux /Windows Server 系统使之成为能提供各种网络应用服务的虚拟服务器，ESXi 也是从内核级支持硬件虚拟化，运行于其中的虚拟服务器在性能与稳定性上不亚于普通的硬件服务器，而且更易于管理维护。

VMware ESXi是我们常说的ESXi主机，虚拟化层组件的作用：
* ESXi用于协调物理计算机的资源，同时通过ESXi管理其上的虚拟机，如部署、迁移等操作。
* 同时还可以通过ESXi对物理计算机上的网络存储资源进行管理，ESXi通过配置虚拟交换机上的vSwitch管理配置网络资源，通过VMfs和nfs管理虚拟存储资源

OVA文件是由以下人员使用的虚拟设备虚拟化 应用程序，例如VMware Workstation和Oracle VM Virtualbox。 它是一个软件包，其中包含用于描述虚拟机的文件，其中包括. OVF 描述符文件，可选清单（.MF）和证书文件以及其他相关文件。
OVA是单个压缩文件，里面包含其他多个虚拟机相关信息，例如硬件配置、硬盘等等。通过 OVA文件, 可以快速在其他机器上导入一个硬件配置、硬盘内容都一样的虚拟机,相当于快速的把一台机器从一个地方复制到另外一个地方，或是在多个地方复制相同的机器。

## 使用到的工具
###  ovftool
这是把虚拟机导出成 ova文件的工具，可以从 https://developer.vmware.com/web/tool/4.4.0/ovf 下载。

### govc
govc 是 VMware 官方 govmomi 库的一个封装实现。使用它可以完成对 ESXi 主机或 vCenter 的一些操作。比如创建虚拟机、管理快照等。基本上能在 ESXi 或 vCenter 上的操作，在 govmomi 中都有对应的实现。目前 govc 支持的 ESXi / vCenter 版本有 7.0, 6.7, 6.5 , 6.0 (5.x 版本太老了，干脆放弃吧)，另外也支持 VMware Workstation 的某些版本。
可以从 https://github.com/vmware/govmomi/tree/master/govc 下载。

govc 通过 GOVC_URL 环境变量指定 ESXi 主机或 vCenter 的 URL，登录的用户名和密码可设置在 GOVC_URL 中或者单独设置 GOVC_USERNAME 和 GOVC_PASSWORD。如果 https 证书是自签的域名或者 IP 需要通过设置 GOVC_INSECURE=true 参数来允许不安全的 https 连接。


# 从ISO安装ESXi虚拟机
## 设置govc环境变量
```
  export GOVC_INSECURE=1                    #   允许不安全的 https 连接
  export GOVC_GUEST_LOGIN=your_esxi_username:your_esxi_password
  export GOVC_URL=https://10.10.16.2/sdk    #    10.10.16.2 是 ESXi 主机或 vCenter 的IP地址
  export GOVC_DATASTORE=datastore1          #   govc ls /XM/datastore 查看当前有什么DATASTORE
  export GOVC_RESOURCE_POOL=Resources       #   单个ESXi情况下，是Resources, 如果连接vCenter那么是ESXi_IP/Resources
  export VI_URL=vi://10.10.16.2/XM/vm       #   内部环境变量，govc ls 看看 vm路径，这个变量用于下面运行 ovftool

```
## 上传ISO到ESXi主机
```
    isofile=ISO文件路径
    isoname=$(basename "$isofile")
    govc datastore.upload "$isofile" "ISO/$isoname"
```

## 创建VM
以下创建8核、12G内存、500GB硬盘的虚拟机，虚拟机名字为MyVM，可以根据需要自己调整参数
```
  vmName=MyVM
  disksize=500GB
  govc vm.destroy "$vmName" #   删除同名虚拟机
  govc vm.create -m 12384 -c=8 -g=centos7_64Guest -disk="$disksize" "-iso=ISO/$isoname" "$vmName"
```
创建后虚拟机自动进入上电状态，ISO安装后可以进行其他配置，等一切符合发布标准后进入下一步

## 创建OVA

```
  vmName=MyVM
  govc vm.power -off -M "$vmName"       # 断电
  govc device.cdrom.eject -vm "$vmName" # 弹出CDROM
  # 在output_dir目录下生成OVA文件
  /opt/ovftool/ovftool -dm=thin --noSSLVerify --powerOffSource "$VI_URL/$vmName" "output_dir/$vmName.ova"
```
到这里这个虚拟机的映像就生成了，可以快速的复制到其他环境部署，大大加快了部署速度。
