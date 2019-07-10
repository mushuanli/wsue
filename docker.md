未合入debian发布版本,需要使用docker仓库
(stable testing)debian安装 https://docs.docker.com/engine/installation/linux/debian/
apt-get install \
     apt-transport-https \
     ca-certificates \
     curl \
     software-properties-common
     
     curl -fsSL https://download.docker.com/linux/debian/gpg | apt-key add -
     
     apt-key fingerprint 0EBFCD88
     add-apt-repository \
   "deb [arch=amd64] https://download.docker.com/linux/debian \
   $(lsb_release -cs) \
   stable"
   apt-get update
   apt-get install docker-ce

-------------------------------------------------------------------
使用:
当使用新内核时, centos6 需要主机内核选项添加:  "vsyscall=emulate" 不然会app运行错误(dmesg 看到 segment fault), 因为kernel 的vsyscall 增加安全模式，与旧的glibc api不兼容

根据目录(例如一个可运行版本)创建镜像:
    rm -rf tmp/* var/cache/* var/run/* root/* root/.[^b]*
      tar --numeric-owner --exclude={vmlinuz,initrd.img} --exclude=var/{backups,cache,local,lock,log,mail,opt,run,spool,tmp}/* -cvf /home/build/rain/tmp/ubuntu1404.tar *
      tar delete -f /home/build/rain/tmp/ubuntu1404.tar root/
      tar -f ../ubuntu1404.tar --delete boot/
    tar --numeric-owner --exclude=/proc --exclude=/sys -cvf centos6-base.tar *
    cat centos6-base.tar | docker import - vmibuild
    replace:  tar rf ../ubuntu1404.tar root/

确认已经导入:
    docker image ls
运行:
    docker run -it --rm -v /home/build/rain/:/builddir -v /home/build/rain/tmp/:/var/tmp vmibuild /bin/bash
定位运行异常，例如容器无法启动:
    docker events &
    然后运行上面命令，或是使用 docker ps -a 观察容器的退出状态，及使用 dmesg 看看是否有内核异常
    
确认容器已经清除:
    docker ps -a

删除容器: docker rm <容器id>
删除镜像: docker rmi <镜像名,例如vmibuild>


-----------------------------------------------------------------------------------------------------------------------
docker = lxc + aufs
利用lxc的资源分组，及aufs 的多层次机制提供应用一个升级版本的 chroot 环境
减少软件部署时的的环境依赖和冲突

三个组成部分
     Image(镜像):          一个文件系统快照(提供类似git的管理机制)，使用分层机制，同时一个文件可能在上层不可见，但是实际还存在
     Container(容器):     一个正在运行的Image  
     Resp(仓库,可选):     一个管理Image的地方，提供多个server 从这里下载Image


安装:
    新的发行版本安装 docker.io 包,官方建议 ubuntu


文件系统：
    在 Ubuntu/Debian 上有 UnionFS 可以使用，如 aufs 或者 overlay2 ，而 CentOS 和 RHEL 的内核中没有相关驱动。
    因此对于这类系统，一般使用 devicemapper 驱动利用 LVM 的一些机制来模拟分层存储。但是性能和空间成问题。
    所以对于 CentOS/RHEL 的用户来说，在没有办法使用 UnionFS 的情况下，一定 要配置 direct-lvm 给 devicemapper ，无论是为了性能、稳定性还是空间利 用率。


运行：
     下载镜像:     docker pull ubuntu:1404     -  [Docker Registry地址]<仓库名>:<标签>
     运行：          docker run -it --rm ubuntu:14.04 bash
                          参数: -i: 交互模式  -t 打开终端   --rm 容器退出后立即删除
                        docker run --name webserver -d -p 80:80 nginx
                          参数: 以映像 nginx 运行一个容器名为 webserver
                          然后通过：  docker exec -it webserver bash 进入 容器
                          比对容器相对映像的改变： docker diff webserver
                          保存容器对映像的改变：(慎用，因为可能保存垃圾信息，尽量使用 dockerfiles )
                                docker commit \ --author "Tao Wang " \ --message "修改了默认网页" \ webserver \ nginx:v2
                                参数:  [选项] <容器ID或容器名> [<仓库名>[:<标签>]]
                          察看映像的修改历史:   docker history nginx:v2

     列出已经下载的映像： docker images
    保存、恢复映像: docker save/load
                        docker save alpine | gzip > alpine-latest.tar.gz
                        docker load -i alpine-latest.tar.gz
                        docker save <镜像名> | bzip2 | pv | ssh <用户名>@<主机名> 'cat | do cker load
    删除本地镜像：  docker rmi [选项] <镜像1> [<镜像2> ...]
                        删除时先去除引用，最后没引用才会删除映像
                       如果有容器依赖，那不可以删除镜像，避免异常
                        删除虚映像: docker rmi $(docker images -q -f dangling=true)
                       根据仓库名删除：  docker rmi $(docker images -q redis)
                       删除mongo:3.2 之前的版本： docker rmi $(docker images -q -f before=mongo:3.2)



Dockerfile: 是一个文本文件(文件名固定为Dockerfile)，包含docker指令，用于创建镜像
     指令:
           FROM  基础镜像名
                     指定基础镜像，或是默认空白镜像  scratch
           RUN  shell命令或是  ["可执行文件", "参数1", "参数2"]
                     每个RUN会创建一层镜像，所以应该尽量少的 RUN, 并且删除临时文件
           COPY 上下文目录的源路径 目的路径            # 会保存完整的文件属性
           ADD   上下文目录的源路径或URL 目的路径  # 带自动解压功能，或是下载url, 但不建议使用
           CMD   <命令> |  ["可执行文件", "参数1", "参数2"...]
                     容器启动命令，例子：
                        CMD echo $HOME
                        CMD [ "sh", "-c", "echo $HOME" ]
                     可以被 docker -i 代替,
                     另外一个是，当CMD执行对象退出，那容器也退出。所以执行的进程必须前台执行
          ENTRYPOINT <命令> |  ["可执行文件", "参数1", "参数2"...]
                      跟CMD的不同是，容器运行时参数会传给entrypoint当参数
          ENV    key value | key1=value1 key2=value2 
                    设置环境变量
          ARG key=value 
                    设置构建时环境变量, 环境变量在运行时不起作用. 但是注意不要放隐私信息例如密码，因为这些信息可以使用docker history 查看
          VOLUMN  ["<路径1>", "<路径2>"...]
                   定义匿名卷，运行容器时，往这些路径写入不会写到容器存储层
                   运行时可以在参数中代替，例如
                        VOLUME /data
                        docker run -d -v mydata:/data xxxx
          EXPOSE <端口1> [<端口2>...]
                   声明容器打算使用的端口
          WORKDIR <工作目录路径>
                  切换后面操作的工作目录
          USER <用户名>
                  切换后面操作的用户名, 临时切换用户可以使用gosu
          HEALTHCHECK [选项] CMD <命令>
                 通过命令检查容器是否健康
          ONBUILD <其它指令>
                 创建本镜像时不执行，以本镜像创建其他镜像才执行



    创建镜像:
            docker build [选项] <上下文路径/URL/->
            例子：   docker build -t nginx:v3 .   表示使用当前目录的Dockerfile 建立nginx:v3镜像  . 表示是上下文目录
            
            其他创建方式: 从 git url, 或是从 tar.bz2 包


--------------------------------------------------------------------------------------------------
                           容器

启动(新建容器)：  docker run
        流程如下：

	1.  检查本地是否存在指定的镜像，不存在就从公有仓库下载
	2. 启动 101 利用镜像创建并启动一个容器
	3. 分配一个文件系统，并在只读的镜像层外面挂载一层可读写层
	4. 从宿主主机配置的网桥接口中桥接一个虚拟接口到容器中去
	5. 从地址池配置一个 ip 地址给容器
	6. 执行用户指定的应用程序
	7. 执行完毕后容器被终止


--------------------------------------------------------------------------------------------------
启动（已终止容器）： docker start
后台运行:  -d
     可以使用 docker logs <id> 观察输出日志
查看运行状态:  docker ps [-a]
终止:  docker stop
重启: docker restart <id>
进入容器： attach 
     但是进入同一容器的所有窗口都显示相同内容，并且都同时阻塞
容器快照： docker export | import
     sudo docker export 7691a814370e > ubuntu.tar :导出容器快照到文件
     sudo docker import http://example.com/exampleimage.tgz example/ imagerepo   从容器快照文件导入为映像
     与映像快照不同的是，容器快照仅有当前快照状态，无历史信息或元数据状态
删除容器:  docker rm
清除所有处于终止状态人容器：  docker rm $(docker ps -a -q)

--------------------------------------------------------------------------------------------------
仓库
概念：  集中存放镜像的地方
            官方仓库是 Docker Hub
登录:    docker login
搜索:    docker search 
下载:    docker pull
自动创建: 在Docker Hub 中配置

私有仓库: 使用docker-registry创建本地的仓库, 可以通过docker运行: 
           sudo docker run -d -p 5000:5000 -v /home/user/registry-conf:/r egistry-conf -e DOCKER_REGISTRY_CONFIG=/registry-conf/config.yml registry
       或是安装本地：
           $ sudo pip install docker-registry
       安装好后，可以通过下面命令把映像推到私有仓库:
             sudo docker tag ba58 192.168.7.26:5000/test
       然后使用命令察看仓库的映像：
             curl http://192.168.7.26:5000/v1/search
       其他机器使用命令下载镜像：
             sudo docker pull 192.168.7.26:5000/test

--------------------------------------------------------------------------------------------------
               数据管理
容器中管理数据主要有两种方式：
    数据卷： 供一个或多个容器使用的特殊目录(或文件)，绕过UFS，可以提供很多有用的特性：
              数据卷可以在容器间共享和重用
              对数据卷的修改会立马生效
              对数据卷的更新，不会影响镜像
              数据卷会一直存在，即使容器被删除
         - 类似Linux下对目录或文件进行mount
         查看数据卷信息:   docker inspect <容器名>
       给容器挂载数据卷: docker run -v <本地目录>:<数据卷目录>[:ro] ...
       删除卷:            docker rm -v 

    数据卷容器： 是一个正常容器，适用于有一些数据需要在容器间共享的情况。
         步骤：

	1. 建立数据卷容器:sudo docker run -d -v /dbdata --name dbdata training/postgres
	2. 在其他容器间使用 --volumes-from 挂载dabata容器中的数据卷 
sudo docker run -d --volumes-from dbdata --name db1 training/p
ostgres 
echo Data-only container for postgres
sudo docker run -d --volumes-from dbdata --name db2 training/p
ostgres
还可以级联： sudo docker run -d --name db3 --volumes-from db1 training/post
gres

	3. 数据卷容器可以用来备份，恢复，迁移数据卷
备份： sudo docker run --volumes-from dbdata -v $(pwd):/backup ubuntu
tar cvf /backup/backup.tar /dbdata
恢复： 先建立一个有空数据卷的容器： 
sudo docker run -v /dbdata --name dbdata2 ubuntu /bin/bash
再挂载后恢复： 
sudo docker run --volumes-from dbdata2 -v $(pwd):/backup busyb
ox tar xvf
/backup/backup.tar

	4. 



--------------------------------------------------------------------------------------------------
网络功能

宿主机端口映射：
使用 -P 会随机把 49000-49900 映射到内部开放网络端口
-p 指定要映射的端口  hostPort:containerPort | ip::containerPort |hostPort:containerPort
docker port 察看端口映射关系

容器间连接:
建立连接关系：
  docker  run --link <要链接到的其他容器名>:<连接别名>
建立后两个容器间建立了一个安全隧道，而且不用映射他们的端口到宿主主机上，
映射关系出现在环境变量和/etc/hosts文件中

高级网络配置：
     docker启动时，会自动在主机上创建一个docker0 虚拟网桥，网桥的IP地址段是一段未占用的私有网段
     没建立一个容器，就会建立一对虚拟网卡veth pair, 一端连接容器(可见为eth0)，另一端连接docker0网桥

--------------------------------------------------------------------------------------------------
repack tensorflow + opencv

docker pull tensorflow/tensorflow
mkdir -f tensorflow-docker && echo '
FROM tensorflow/tensorflow
WORKDIR /notebooks
RUN sed -i "s|http://archive.ubuntu.com|http://mirrors.163.com|g" /etc/apt/sources.list&&rm -Rf /var/lib/apt/lists/*&&apt-get -y update&&apt-get install -y\
       pkg-config\
       python-dev\
       python-opencv\
       libopencv-dev\
       libav-tools\
       libjpeg-dev\
       libpng-dev\
       libtiff-dev\
       libjasper-dev\
       python-numpy\
       python-pycurl\
       python-opencv
'> tensorflow-docker/Dockerfile 
      
cd tensorflow-docker &&  docker build -t tensorflowcv .

--------------------------------------------------------------------------------------------------
1. 建立私有仓库
1.1. 启动私有仓库服务
docker 的私有仓库也是以一个 docker image 方式提供服务。

默认 image 保存在 container中。如果想让 image 保存到其他路径，那么使用 -v /srv/registry/data:/var/lib/registry 指定保存路径

启动方式如下：

docker run -d \
  -p 5000:5000 \
  --restart=always \
  --name registry \
  -v /svr/docker/registry/data:/var/lib/registry \
  registry:2


其中 --restart=always 表示每次 docker 服务重启都会重启这个 image.

1.2. 获取私有仓库信息


curl -XGET http://localhost:5000/v2/_catalog
curl -XGET http://localhost:5000/v2/image_name/tags/list


1.3. 对外提供服务
要让其他机器能够访问，需要设置密码和https证书。



1.3.1. 创建密码：
用户名密码保存在独立文件 htpasswd 中, 可以有多个, 下面是设置admin 账号密码的方法:

mkdir /svr/docker/registry/auth
docker run --entrypoint htpasswd registry:2 -Bbn admin <insert-password> >> /vosg/docker/registry/auth/htpasswd
docker ps # Get ID
docker stop <id> && docker rm <id>
设置成功后文件内容如下, 每一行包括用户名和密码，如果需要增加其他用户可以继续添加更多行：

[root@localhost registry]# cat /svr/docker/registry/auth/htpasswd
admin:$2y$05$n0cZ9Sp7bzwXn25AJQAGLOR8EX3SHpy1MnCax.y0nq0UrWnNephGW


启动支持htps 认证

1.3.2. 创建https秘钥
建议使用正式证书。如果要自签证书那：

 首先修改 /etc/pki/tls/openssl.cnf 配置，在该文件中找到 [ v3_ca ]
，在它下面添加如下内容：

...
[ v3_ca ]
# Extensions for a typical CA
subjectAltName = IP:10.206.138.106
再次重启 docker，解决 "x509: cannot validate certificate for 192.168.1.211 because it doesn't contain any IP SANs" bugs。



生成私有证书：

cd /svr/docker/registry/ ; mkdir -p certs ; openssl req \
   -newkey rsa:4096 -nodes -sha256 -keyout certs/domain.key \
   -x509 -days 99999 -out certs/domain.crt
在每个需要访问的客户端上生效私有证书：

mkdir -p /etc/docker/certs.d/10.206.138.106:5000/
keytool -printcert -sslserver 10.206.138.106:5000 -rfc > /etc/docker/certs.d/10.206.138.106:5000/ca.crt
sudo systemctl restart docker


1.3.3. 启动
下面是启动命令，

docker run -d \
  -p 5000:5000 \
  --restart=always \
  --name registry \
  -v /svr/docker/registry/data:/var/lib/registry \
  -v /svr/docker/registry/certs:/cert \
  -v /svr/docker/registry/auth:/auth \
  -e "REGISTRY_HTTP_TLS_CERTIFICATE=/cert/domain.crt" \
  -e "REGISTRY_HTTP_TLS_KEY=/cert/domain.key" \
  -e "REGISTRY_AUTH=htpasswd" \
  -e "REGISTRY_AUTH_HTPASSWD_PATH=/auth/htpasswd" \
  -e "REGISTRY_AUTH_HTPASSWD_REALM=Registry Realm" \
  registry:2
启动时加入 restart=always, 因为容器内保存仓库私有状态，必须保持容器并重用这个容器。


y由于参数很多，所以可以通过 yaml 配置文件方式启动, 把下面内容保存到 docker-compose.yaml文件中，和上面运行方式等同:

registry:
 restart: always
 image: registry:2
 ports:
 — 5000:5000
 environment:
 REGISTRY_HTTP_TLS_CERTIFICATE: /cert/domain.crt
 REGISTRY_HTTP_TLS_KEY: /cert/domain.key
 REGISTRY_AUTH: htpasswd
 REGISTRY_AUTH_HTPASSWD_PATH: /auth/htpasswd
 REGISTRY_AUTH_HTPASSWD_REALM: Registry Realm
 volumes:
 — /svr/docker/registry/data:/var/lib/registry
 — /svr/docker/registry/cert:/cert
 — /svr/docker/registry/auth:/auth


1.4. 放一个 image 到私有仓库中
放 image 到私有仓库中，主要先 tag 打标签，然后 push 就可以。

docker tag debian8:latest 10.206.138.106:5000/debian8:latest
docker push 10.206.138.106:5000/debian8:latest
最后在私有仓库服务器上使用 curl 确认已经放上去:

# curl -XGET http://localhost:5000/v2/_catalog
{"repositories":["debian8"]}


1.5. 重启私有仓库
重启仓库时，得使用 docker retart 方式重启容器，而不能运行一个新容器。

[root@localhost ~]# docker ps
CONTAINER ID        IMAGE               COMMAND                  CREATED             STATUS              PORTS                    NAMES
fa713d5ad163        registry            "/entrypoint.sh /e..."   23 minutes ago      Up 23 minutes       0.0.0.0:5000->5000/tcp   silly_leavitt
[root@localhost ~]# docker kill fa713d5ad163
fa713d5ad163
[root@localhost ~]# docker ps -a
CONTAINER ID        IMAGE               COMMAND                  CREATED             STATUS                       PORTS               NAMES
fa713d5ad163        registry            "/entrypoint.sh /e..."   23 minutes ago      Exited (137) 2 seconds ago                       silly_leavitt
[root@localhost ~]# docker restart fa713d5ad163
fa713d5ad163
[root@localhost ~]# curl -XGET http://localhost:5000/v2/_catalog
{"repositories":["debian8"]}






2. kubernetes使用私有仓库
2.1. 创建 secret
通常在实际的项目中用kubernetes做开发的时候，会用到私有的registry（镜像仓库），比如：在创建应用的时候，镜像用的就是私有仓库的镜像。但是通常会有一个问题，如果你的私有的镜像仓库做了认证和授权，kubernetes在创建应用的时候去获取私有仓库镜像就会失败，会报没有认证的错误。有两种方式去解决。

1. 在k8s中的每个集群中的node节点中去docker login 登录。显然这种方式不合理。
2. 通过k8s的secret来做。
下面我主要讲解的就是第二种方式。

首先在其中一个node上登录私有仓库

首先在其中一个node上登录私有仓库

docker login hub.rain.io



登录成功后会在/root/.docker目录下生产config.json文件，然后执行如下命令：

cat /root/.docker/config.json | base64



该命令会将你的认证信息通过base64编码，生成一个编码之后的字符串，在linux中terminal中看到是两行，但是其实质是一行，所以之后要用到的这个字符串需要合并为一行。

在kubernetes中的master节点中创建secret 元素：

apiVersion: v1
kind: Secret
metadata:
name: hub.rain.io.key
type: kubernetes.io/dockercfg
data:
.dockercfg: ewoJImF1dGhzIjogewoJCSJkb2NrZXIuY29vY2xhLm9yZyI6IHsKCQkJImF1dGgiOiAiWkdWMk9tUnZZMnRsY2c9PSIsCgkJCSJlbWFpbCI6ICIiCgkJfQoJfQp9



其中name你可以随便取，推介用私有仓库地址.key的方式命名。

2.2. 应用secret
之后在创建其他元素的时候指定：imagesPullSecrets即可。例如pod：

apiVersion: v1
kind: Pod
metadata:
name: go-web
spec:
containers:
- name: go-web
image: hub.yfcloud.io/go-web
imagePullSecrets:
- name: hub.rain.io.key

replicationController:

apiVersion: v1
kind: ReplicationController
metadata:
name: go-web
labels:
name: go-web
spec:
replicas: 1
selector:
name: go-web
template:
metadata:
labels:
name: go-web
spec:
containers:
- name: go-web
image: hub.yfcloud.io/go-web
ports:
- containerPort: 9080
resources:
limits:
cpu: 100m
memory: 100Mi
imagePullSecrets:
- name: hub.rain.io.key
