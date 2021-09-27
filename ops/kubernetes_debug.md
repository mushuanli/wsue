---
   # 服务无法访问
---
1. 判断服务是否已经起来:
```
kubectl -n appnamespace get svc
```
2. 进入一个pod, 使用nslookup 判断是否能够解析成功
```
   kubectl -n appamespace exec -it podname -c containername -- sh
   nslookup svcname
   
```
3. 如果不能解析成功，那么重启一下 coredns 看看是否成功
```
kubectl -n kube-system rollout restart deployment coredns
```

