Linux上使用NetworkManager创建 WIFI热点

# 介绍
WIFI热点可以共享主机流量，在手机WIFI流量很贵的情况下是一个很好的省钱方法。
新版本的Linux已经提供创建 WIFI 热点、并自动进行IP地址伪装方法，简单几个命令就能实现。 

一下使用debian举例，其他发行版本可以使用类似方法

# 准备
## 禁止网卡随意编号（可选）
/etc/default/grub GRUB_CMDLINE_LINUX一行增加如下选项"biosdevname=0 net.ifnames=0"：
```
GRUB_CMDLINE_LINUX=... biosdevname=0 net.ifnames=0 ...
```
然后:
```
sudo update-grub
reboot
```

对于yum系列，使用"    grub2-mkconfig -o /boot/grub2/grub.cfg "



## 安装wifi驱动
Linux上一些WIFI是免驱的，但是还有一些得安装固件（例如intel系列），或是从网络上安装驱动。wifi的包一般是non-free, 对于debian, 需要/etc/apt/source.list里面指定non-free 
如果是Intel系列wifi, 那么下载： firmware-iwlwifi(https://packages.debian.org/search?keywords=firmware-iwlwifi), 
如果是其他的，那么对于 usb无线网卡使用 lsusb, 对于pci无线网卡使用 lspci -vv
```
 lsusb
Bus 002 Device 001: ID 1d6b:0003 Linux Foundation 3.0 root hub
Bus 001 Device 004: ID 0bda:c811 Realtek Semiconductor Corp. 802.11ac NIC
...

 lspci
...
00:14.3 Network controller: Intel Corporation Alder Lake-S PCH CNVi WiFi (rev 11)

```

然后网络上查找安装安装方法。如上面的Intel找(Alder Lake-S PCH CNVi WiFi)得到的指导是下载 firmware-iwlwifi,
而从 0bda:c811 找到 (https://github.com/morrownr/8821cu-20210916), 然后按照他的方法编译内核模块并安装，
再dmesg 和 nmcli d 确认确实找到新无线网卡


## 安装依赖软件
* network-manager - 提供网络配置
* dnsmasq         - 提供DNS缓存
* nftables        - 提供IP地址伪装
安装命令:
```
apt install -y network-manager dnsmasq nftables
```

安装完后配置/etc/NetworkManager/NetworkManager.conf, 增加unmanaged-devices一行以让network-manager 忽略docker网卡(如果有virtualbox或vmware的也可以以类似方法添加)：
```
cat /etc/NetworkManager/NetworkManager.conf
[main]
plugins=ifupdown,keyfile

[ifupdown]
managed=false

[keyfile]
unmanaged-devices=interface-name:docker0;interface-name:veth*;;interface-name:br*

```

## 启动服务并读入最新配置
dnsmasq不需要运行服务，nftables和NetworkManager需要

```
systemctl enable --now nftables
systemctl enable --now NetworkManager
systemctl reload NetworkManager
systemctl restart NetworkManager

```

# 配置WIFI热点
```
nmcli device wifi hotspot ifname wlan1 con-name hot ssid rainli password MySelfPassword
# 如果需要修改IP或是隐藏SSID广播，执行下面语句
nmcli con modify hot ipv4.method shared ipv4.addresses 192.168.240.1/24 802-11-wireless.hidden 'TRUE’
```
到这里就配置完成，NetworkManager会自动往 nftable里面添加IP伪装规则，手机能够通过热点上网
```
nft list ruleset
...
table ip nm-shared-wlan1 {
        chain nat_postrouting {
                type nat hook postrouting priority srcnat; policy accept;
                ip saddr 192.168.240.0/24 ip daddr != 192.168.240.0/24 masquerade
        }

        chain filter_forward {
                type filter hook forward priority filter; policy accept;
                ip daddr 192.168.240.0/24 oifname "wlan1" ct state { established, related } accept
                ip saddr 192.168.240.0/24 iifname "wlan1" accept
                iifname "wlan1" oifname "wlan1" accept
                iifname "wlan1" reject
                oifname "wlan1" reject
        }
}

```
如果到这里有问题，那么可能是iptables冲突，假如已经安装了 docker, 把docker任务停止看看，是否能够上网
