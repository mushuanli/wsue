使用fluent-bit与Fluentd搭建kubernetes上pod的日志收集器

# 背景
## kubernetes介绍
kubernetes是一个优秀的容器编排平台， 是新一代的云原生的操作系统。但是kubernetes与原始操作系统的区别在于：

原始操作系统提供统一的日志存储功能，但是kubernetes未收集pod日志。

同时大部分pod都是无状态，当pod退出时pod内的调试日志也会丢失，这给故障定位带来一定麻烦。本文介绍使用fluent-bit与fluentd重定向和收集日志的方法。

## 日志收集简介
虽然可以在代码中自己写日志并发送到自己的日志服务器。但是不同的模块有不同的日志格式，而现在云原生时代必然使用大量的第三方模块，他们的日志格式都各不相同，如果是自己写日志服务器，那么需要处理各种不同的日志格式，会增加自己的负担。

fluentd是解决这个问题的一个工具，它 是一个针对日志的收集、处理、（聚合）转发系统。通过丰富的插件系统， 可以收集来自于各种系统或应用的日志，转化为用户指定的格式后，转发到用户所指定的日志存储系统之中。

fluentd可以与Elasticsearch 、Kibana组成EFK。

![fluentd功能](https://miro.medium.com/max/720/0*dmVIzpYMPwQeXin9.webp)



Fluentd功能强大，但是这也导致它的CPU和内存开销比较大，这就产生了fluent-bit, 一个简化的fluentd。

fluent-bit是一个用C编写的日志收集器和处理器（它没有Fluentd等强大的聚合功能）。由于使用C语言编写，而且功能有限，所以性能高、资源开销小，可以作为日志转发器转发给 fluentd, 由fluentd进行聚合和进一步处理。

fluent-bit与fluentd对比：

![fluentd与fluent-bit对比](https://miro.medium.com/max/720/1*JkR49YseNaju0fF7Hr-3_A.webp)

根据fluent-bit的高性能低资源利用率和fluentd的功能强大特点，一般这两个模块可以如下组合，实现日志的收集：
![image](https://user-images.githubusercontent.com/15431022/215995222-0d3afdf7-69a9-4e0a-80be-77119513d274.png)

## 日志收集模型说明
在下面的例子就是采用kubernetes内fluent-bit边车收集日志，然后发送给fluentd写到文件(也保存在kubernetes的PVC内)。我们将建立一个 fluentd 日志聚合系统，收集所有pod(通过fluent-bit边车)发过来的所有日志，并按日期分类保存到PVC(kubernetes的持久性存储)中, 同时保存90天内日志。

# 使用fluentd建立日志服务器
##  建立持久性存储(PVC)
虽然mongodb、kibanna等功能更强大，但是由于本例日志仅是定位用，所以为了简单是使用文件系统方式保存日志文件。先建立PVC用于保存日志的存储。

建立storage”ext-storage”的过程忽略(例如在aws上使用ebs创建)，yaml文件内容如下：
```
# fluentd storage
apiVersion: v1
kind: PersistentVolumeClaim
metadata:
  name:      fluentd-data    # 用于 pod 中挂载的PVC名字
  namespace: app-backend
spec:
  accessModes:
    - ReadWriteOnce          # 限制一次仅能一个pod挂载, 可以根据需要修改
  storageClassName: ext-storage  # storage名字
  resources:
    requests:
      storage: 5Gi           # 大小是5G
```

## 创建fluentd配置
Fluentd是一个日志收集系统，那么一条日志消息，在Fluentd里就认为是一个事件(Event)。Fluentd的事件由下面三部分组成:

* 标签(tag): 用于说明这个事件是哪里产生的，可用于后面的事件路由;
* 时间(time): 事件是什么时候发生的，时间格式为: Epoch time, 即Unix时间戳;
* 记录(record): 事件内容本身，JSON格式。
所有的输入插件都需要解析原始日志，生成满足上面结构的事件字段。

当fluentd收到一个事件之后，会经过一系列的处理流程:

* 如修改事件的相关字段
* 过滤掉一些不关心的事件
* 路由事件输出到不同的地方。

配置文件由以下几部分组成:

* source: 输入, 可以定义多个以指定不同的输入源。
* match: 输出目的地，也可以定义多个，会根据tag按顺序进行匹配。每个tag被匹配成功后进入这个 match处理，然后不会再往后处理。如果需要继续处理那么要使用@out_copy。
tag的匹配可以使用通配符*，但是当有.时，得使用**才能全匹配；还可以使用{a,b}的方式表示a或b。
内部使用<store>定义输出一个模块, 例如<store> @file 定义输出到文件。
在<store>里面，还可以使用<format>定义输出格式，使用<buffer>定义输出缓冲区参数。
* filter: 指定事件的处理流程, 可以多个filter串联起来,如：
```
Input -> filter 1 -> … -> filter N -> Output
```
* system：系统级别的配置。
* label: 用于分组特定的filter和match
* @include: 引入其它的配置文件。可以将配置文件拆分为多个，便于复用, 例如buffer的配置参数。当要使用的时候，直接使用@include引入即可。
配置文件中还可以使用${envname}获取环境变量envname的值。

下面是配置部署文件yaml例子, 配置文件保存着configmap中，方便随时调整：
```
# fluentd configmap
apiVersion: v1
kind: ConfigMap
metadata:
  creationTimestamp: null
  name: configmap-fluentd
  namespace: app-backend
data:
  fluent-logfile-format.conf: |
    # 定义可复用的按日期写文件参数
    append        true
    time_slice_format %Y%m%d
    time_slice_wait 24h
    compress      gzip
  fluent-format-json.conf: |
    # 定义可复用的json格式日志参数, 把接受到的日志格式当成一个json, 并且自动添加行结束符号
    <format>
      @type       json
      add_newline true
    </format>
  fluent-format-line.conf: |
    # 定义可复用的fluent-bit发过来的其他格式日志参数, 其他类型日志也是按json发送，但是日志内容在 log: 字段中(由message_key定义)
    <format>
      @type       single_value
      message_key log
    </format>
  fluent-buffer-gzip-20m.conf: |
    # 定义可复用的写文件参数
    <buffer tag,time>  # 日志发送方式是buffer
    #buffer 机制将日志传送分成了两个阶段，stage 和 queue。既然是 buffer，多条日志会累计到一定的数量再发往目的地，这一定数量的日志就是 chunk。
      @type         file # 写入文件
      timekey       1d  # chunk 间隔是1天
      timekey_wait  10m  # 当下一个 timekey 到的时候，再推迟 timekey_wait 事件写入
      timekey_use_utc true # 使用 UTC 时间
      flush_mode    interval # 定时刷新
      compress      gzip  # 为了节省空间使用压缩，
      flush_interval 5s   # 发送间隔是5秒
      total_limit_size  20MB # buffer 能够存储的最大日志大小是20MB
    </buffer>
  fluent.conf: |
    # fluentd 主配置文件
    #Receive events from 24224/tcp
    # This is used by log forwarding and the fluent-cat command
    <source>  # 接收fluent-bit转发过来的日志
      @type         forward
      port          24222
    </source>
    <source>  # 定时清除超过90天日志, 避免磁盘空间用尽
      @type exec
      format none
      tag none
      command find /var/log -type f -mtime +90 -delete
      run_interval 1440m  # 每天运行一次
    </source>
    # 将tag为 app*.* 的日志转存到文件, 这些日志格式都是json
    <match app**>
      @type         file  # 目的地是文件
      path          /var/log/${tag[0]}/${tag} # 例如tag 啥app1.info, 那么保存路径为/var/log/app/app.info.20211030.log.gz 
      @include fluent-logfile-format.conf
      @include fluent-format-json.conf
      @include fluent-buffer-gzip-20m.conf
    </match>
    # 将标记为 svr*.*  的日志转存到文件, 这些日志可以是任意格式
    <match svr**>
      @type         file
      path          /var/log/svr/${tag[0]}/${tag}
      @include fluent-logfile-format.conf
      @include fluent-format-line.conf
      @include fluent-buffer-gzip-20m.conf
    </match>
```

## fluentd deployment与service
yaml文件入下：
```
apiVersion: apps/v1 # for versions before 1.9.0 use apps/v1beta2
kind: Deployment
metadata:
  name: logsvr-fluentd
  namespace: app-backend
  labels:
    app: logsvr-fluentd
spec:
  selector:
    matchLabels:
        app: logsvr-fluentd
  strategy:
        type: Recreate # 由于挂载PVC（默认只支持一个POD读写），所以升级方式是完全重启
  template:
    metadata:
      labels:
        app: logsvr-fluentd
        role: master
        tier: backend
spec:
  nodeSelector:
    "beta.kubernetes.io/os": linux
  imagePullSecrets:
  - name: fluent-ecr-secret
  volumes:
  - name: logconfig  # 把上面的configmap-fluentd关联到logconfig卷，这里是配置文件信息
    configMap:
      name: configmap-fluentd
  - name: varlog  # 挂载上面创建的PVC关联到varlog卷，用于保存日志
    persistentVolumeClaim:
      claimName: fluentd-data
  initContainers: # 由于PVC默认权限不支持普通用户读写，这里用初始化pod修改一下/var目录权限避免写入失败 
  - name: init
    image: busybox:latest
    args: [/bin/sh, -c, 'chmod 777 /var/']
    volumeMounts:
    - name: varlog
      mountPath: /var
    resources:
      requests:
        cpu: 20m
        memory: 24Mi
      limits:
        cpu: 100m
        memory: 128Mi
  containers: # fluentd日志服务器container
  - name: log
    image: fluent/fluentd:v1.14
    ports:
      - containerPort: 24222
        name: fluentd
        protocol: TCP
    resources:
      requests:
        cpu: 20m
        memory: 50Mi
      limits:
        cpu: 100m
        memory: 100Mi
    volumeMounts:
      - name: logconfig
        mountPath: /fluentd/etc/
        readOnly: true
      - name: varlog
        mountPath: /var
---
# 定义服务，方便namespace内部其他pod访问
kind: Service
apiVersion: v1
metadata:
  name: logsvr-fluentd
  namespace: app-backend
  annotations:
  service.beta.kubernetes.io/brightbox-load-balancer-healthcheck-request: /
spec:
  type: NodePort
  selector:
    app: logsvr-fluentd
  ports:
    - name: fluentd  # 把24222端口开放，以接收其他pod 发送的消息
      protocol: TCP
      port: 24222
      targetPort: 24222
```
  
## 部署与使用
运行下面命令进行部署：
```
kubectl -n app-namespace apply -f yaml配置文件
```
部署顺序是PVC、configmap、deployment与service。

最后使用:
```
kubectl -n app-namespace get po
```
可以检查状态，或是使用:
```
kubectl -n app-namespace exec -it logsvr-fluentd-xxxx — sh
```
进入日志服务器，然后到 /var/log 目录下观察收集的日志。

## 在pod中添加fluent-bit边车
### 边车介绍
边车是kubernetes的扩展pod的一种方式。指在一个Pod 中同时运行两个容器，共享网络和文件系统。应用容器执行业务，边车容器辅助执行其他功能，例如这里就是把应用容器产生的日志发送到日志服务器。

边车的好处是：

* 低耦合：为应用容器添加增强功能，而对其不变动；
* 单一职责：每个容器的职责不同；
* 边车的异常不会影响主容器；
* 复用性好，边车容器可以和不同的应用容器搭配；
* 各自更新，不受影响。
* 而fluent-bit的高性能和低资源占用，是边车容器的理想选择。

### 创建fluent-bit配置
同样使用 yaml进行配置。配置文件中可以使用${APP_FLUENT_TAG}方式来环境变量，下面是yaml文件内容：
```
# fluent-bit configmap
apiVersion: v1
kind: ConfigMap
metadata:
  creationTimestamp: null
  name: configmap-fluent-bit
  namespace: app-backend
data:
  fluent-bit.conf: |
    # fluent-bit 主配置文件 
    [SERVICE]
      Flush           5    # 每5秒刷新一次
      Daemon          off  # 因为啥边车容器运行，所以不能放在后台
      Log_Level       debug  # 最低日志级别
      Parsers_File    parsers.conf  # 引用日志处理文件
    [INPUT]
      Name        tail  # 读取文件方式，类似 tail -f
      Path        /var/log/app_debug.log # 日志文件路径
      DB          /tmp/ng.log  # 保存文件读取信息，例如偏移
      Parser      app   # 使用 parsers.conf 定义的app处理每一行日志
      Tag         ${APP_FLUENT_TAG} # 给处理后的日志设置tag = ${APP_FLUENT_TAG}
      Db.sync     Full  # 什么DB的同步方式
      Buffer_Chunk_Size 320KB # 设置读取文件数据的初始缓冲区大小
      Buffer_Max_Size   520KB # 设置每个监控文件的缓冲区大小,设置大些以支持长行。
    [FILTER]
      # 第一个过滤器，对匹配的记录修改成新标签, 当一个记录匹配一个 RULE时它就退出这个filter，不会继续匹配下一个Rule
      Name          rewrite_tag   
      Match         ${APP_FLUENT_TAG} # 仅处理 tag = ${APP_FLUENT_TAG}, 注意这里不能用*, 不然会一直循环匹配
      # 如果 记录.level == 'info' 那么设置tag= ${APP_FLUENT_TAG}.info, 并且保留旧tag记录
      Rule          $level ^(info)$  $TAG.info true
      # 如果 记录.message.act 包含 EXCEPTION 字符串，那么设置tag= ${APP_FLUENT_TAG}.exc,并且保存旧tag记录
      Rule          $message['act'] EXCEPTION  $TAG.exc true
      Emitter_Name  re_emitted
    [FILTER]
      # 由于一个filter内一个记录仅能匹配一个rule,当要再处理时需要创建新的 filter
      Name          rewrite_tag
      Match         ${APP_FLUENT_TAG}.info
      # 如果info记录.respError.code不为空，那么设置 tag = ${APP_FLUENT_TAG}.err, 并且保存旧tag
      Rule          $message['respError']['code'] .  ${APP_FLUENT_TAG}.err true
      Emitter_Name  re_emitted2
    [FILTER]
      # 设置${APP_FLUENT_TAG}.err里面某些记录tag=ignore, 并且不保存旧tag
      Name          rewrite_tag
      Match         ${APP_FLUENT_TAG}.err
      Rule          $message['respError']['code'] ^.*Content$  ignore false
      Emitter_Name  re_emitted3
    [OUTPUT]
      # 忽略tag=ignore的记录
      Name NULL
      Match ignore
    
    # 用于调试, 把处理结果打印到屏幕上
    #[OUTPUT]
    #  Name stdout
    #  Match **
    [OUTPUT]
      # 把所有 tag=${XDR_FLUENT_TAG}、${XDR_FLUENT_TAG}.info、${XDR_FLUENT_TAG}.exc、${XDR_FLUENT_TAG}.err的记录发送到fluent server,如果失败会重试6次
      Name        forward
      Match       ${XDR_FLUENT_TAG}**
      Host        ${XDR_FLUENT_HOST}
      Port        ${XDR_FLUENT_PORT}
      Retry_Limit 6
  parsers.conf: |
    # 分析json记录，主要是time_key 指向记录中时间的字段名，timer_format指向记录的时间格式
    [PARSER]
      name                    app
      format                  json
      time_key                timestamp
      time_format             %Y-%m-%dT%H:%M:%S.%L
      Time_Keep               On  # 分析完后保存time字段
```
  
### 添加fluent-bit边车
fluent-bit边车挂载主容器的日志磁盘存储空间，并根据fluent-bit.conf 配置处理主容器产生的日志文件。

在自己的deployment->spec->template->speccontainers下面添加边车定义，其中 varlog 定义为与主容器共享的日志存储空间。
```
      - name: bit
        image: "fluent/fluent-bit:1.8.7"
        imagePullPolicy: IfNotPresent
        resources:
          requests:
             cpu: 5m
             memory: 5Mi
          limits:
            cpu: 50m
            memory: 50Mi
        volumeMounts:
          - name: logconfig
            mountPath: /fluent-bit/etc/
            readOnly: true
          - name: varlog
            mountPath: /var/log
        env:
        - name: APP_FLUENT_HOST
          value: logsvr-fluentd  # fluentd service 名称
        - name: APP_FLUENT_PORT
          value: "24222"
        - name: APP_FLUENT_TAG
          value: "appbackend"
```
  
## 部署和调试：
使用 kubectl -n app-namespace apply -f yaml名字部署, 先部署 configmap, 再部署deployment和service.

当调试时可以把 fluent-bit.conf 里面的 stdout 的OUPUT打开，看看分析结果是否正确。还可以使用dummy 来调试,如下把fluent-bit.conf 的input 修改成下面, 其中dummy 改成自己的日志格式：
```
[INPUT]
    Name         dummy
    Dummy       {"type":"trace","level":"info","taskId":"516ad9cb-f3b8-4215-94bb-1927d83b901c","message":{"remoteAddr":"::ffff:10.131.47.14"}}
    Tag         ${XDR_FLUENT_TAG}
```
  
# 参考
参考：

* fluentd教程(含实例)
fluentd是一个开源的日志收集系统，能够收集各式各样的日志, 并将日志转换成方便机器处理的json格式。 不同操作系统的安装方式不同，具体可以参考: 官方文档: Installation…
crazygit.wiseturtles.com
  https://crazygit.wiseturtles.com/2019/11/29/fluentd-tutorial/

* fluentd中buffer配置
fluentd 向 es 进行日志传送的过程中，如果 es 挂了，日志就会被丢弃。常见的做法是在 fluentd 和 es 之间加一个 kafka 作为 buffer。 问题虽然解决了，但是日志量不大的时候使用 kafka…
www.dazhuanlan.com
  https://www.dazhuanlan.com/vx18201032372/topics/1126886

* tail
Path 模式中的每个匹配文件，并为每个新行(分隔符为 \n)生成一条新纪录。作为可选的，可以使用数据库文件，以便插件可以跟踪文件的历史记录和偏移状态，这对于重启服务时的状态恢复非常有用。
hulining.gitbook.io
  https://hulining.gitbook.io/fluentbit/pipeline/inputs/tail

Fluentd
Fluentbit
Kubernetes
