利用GOVC管理ESXi

# 介绍
VMware vSphere（VMware ESXi）是一种裸金属架构的虚拟化技术。虚拟机(基于linux修改)直接运行在系统硬件上，创建硬件全仿真实例，被称为“裸机”，适用于多台机器的虚拟化解决方案，而且可以图形化操作。
在ESXi安装好以后，我们可以通过vSphere Client 远程连接控制，在ESXi 服务器上创建多个VM（虚拟机），在为这些虚拟机安装好Linux /Windows Server 系统使之成为能提供各种网络应用服务的虚拟服务器，ESXi 也是从内核级支持硬件虚拟化，运行于其中的虚拟服务器在性能与稳定性上不亚于普通的硬件服务器，而且更易于管理维护。

govc 是 VMware 官方 govmomi 库的一个封装实现。使用它可以完成对 ESXi 主机或 vCenter 的一些操作。比如创建虚拟机、管理快照等。基本上能在 ESXi 或 vCenter 上的操作，在 govmomi 中都有对应的实现。目前 govc 支持的 ESXi / vCenter 版本有 7.0, 6.7, 6.5 , 6.0 (5.x 版本太老了，干脆放弃吧)，另外也支持 VMware Workstation 的某些版本。
可以从 https://github.com/vmware/govmomi/tree/master/govc 下载。

govc 通过 GOVC_URL 环境变量指定 ESXi 主机或 vCenter 的 URL，登录的用户名和密码可设置在 GOVC_URL 中或者单独设置 GOVC_USERNAME 和 GOVC_PASSWORD。如果 https 证书是自签的域名或者 IP 需要通过设置 GOVC_INSECURE=true 参数来允许不安全的 https 连接。

# 准备govc环境变量
```
  export GOVC_INSECURE=1                    #   允许不安全的 https 连接
  export GOVC_GUEST_LOGIN=your_esxi_username:your_esxi_password
  export GOVC_URL=https://10.10.16.2/sdk    #    10.10.16.2 是 ESXi 主机或 vCenter 的IP地址
  export GOVC_DATASTORE=datastore1          #   govc ls /XM/datastore 查看当前有什么DATASTORE
  export GOVC_RESOURCE_POOL=Resources       #   单个ESXi情况下，是Resources, 如果连接vCenter那么是ESXi_IP/Resources

```

# 操作
## 查看资源路径
```
  govc ls
```
结果如下:
```
[root@rainbuild sa2]# govc ls
/VX/vm          # 本ESXI虚拟机信息
/VX/network     # 本ESXI网络配置信息
/VX/host        # Vcenter下各ESXi虚拟机信息
/VX/datastore   # Vcenter下各ESXi datastore信息

```

## VM管理
### 显示ESXi上所有运行的VM(名字、状态、IP）
```
  path=/VX/host #   可以通过govc ls 获取host路径
  govc object.collect -type m /VX/host name runtime.powerState guest.ipAddress | awk '{print $4}' | tr '\n' ' ' | sed -r 's/powered(\w+)/\1\n/g'
```

### 开机
```
    govc vm.power -on -M "$vmName"
```

### 关机
```
    govc vm.power -off -M "$vmName"
```

### 从ISO创建机器
```
    govc datastore.upload "$path_to_isofile/$isoname" "ISO/$isoname"  # 上传ISO文件到ESXi的ISO/目录下
    govc vm.create -m 12384 -c=8 -g=centos7_64Guest -disk=200GB "-iso=ISO/$isoname" "$vmName"
    govc vm.power -on -M "$vmName"
```

### 从OVA创建机器
```
    tmpFile=$(mktemp)
    echo '{"DiskProvisioning": "thin"}' > "$tmpFile"    #   硬盘使用thin模式，避免ESXi提前为虚拟机分配空闲硬盘空间
    govc import.ova -options "$tmpFile" -name "$vmName" "${ova_file_path}"
```

### 扩大硬盘空间
```
    diskname=$(govc  device.info -vm "$vmName" | grep ^Name | grep disk|awk '{print $2}')
    govc vm.disk.change -vm "$vmName" -disk.name "$diskname" -size 200G
    # 如果不成功，那么可能需要修改：/etc/vmware/vpxa/vpxa.cfg ; /etc/init.d/vpxa stop ; /etc/init.d/hostd restart ； /etc/init.d/vpxa start
```

### 删除机器
```
    govc vm.destroy "$vmName"
```

### 看机器状态
```
    govc vm.info "$vmName"
```


### 在VM上运行脚本
```
    govc guest.run -vm "$vmName" /usr/bin/nmcli c s
```

## 快照
### VM的快照列表
```
    govc snapshot.tree -D -vm "$vmName"
```

### 生成VM的快照
```
    govc snapshot.create -vm "$vmName" "$snapshotname"
```

### 删除VM的快照
```
    govc snapshot.remove -vm "$vmName" "$snapshotname"
```

### 使用VM的快照
```
  govc vm.power -off -M "$vmName"
  govc snapshot.revert -vm "$vmName" "$snapshotname"
  govc vm.power -on -M "$vmName"
```

## 文件的上传、列表、删除、详细信息
```
  govc datastore.upload "$path_to_isofile/$isoname" "ISO/$isoname"  # 上传ISO文件到ESXi的ISO/目录下
  govc datastore.ls "$1"
  govc datastore.rm -f "$1"
  govc datastore.ls -l -json=true "ISO/$isoname"

```

