---
   # 服务无法访问
---
1. 判断服务是否已经起来:
```
kubectl -n appnamespace get svc
还可以
kubectl -n appnamespace get svc  testservername -o yaml
```
2. 判断是否有pod链接到这个服务上
```
kubectl -n appnamespace get pods -l app=testservername
```
3. 进入一个pod, 使用nslookup 判断是否能够解析成功
```
   kubectl -n appamespace exec -it podname -c containername -- sh
   nslookup testservername
   
```
4. 如果不能解析成功，那么重启一下 coredns 看看是否成功
```
kubectl -n kube-system rollout restart deployment coredns
```
5. 如果不行，那么可以在 pod 内使用 curl 判断是否能够连接 IP:端口成功, 确认通过内部 IP是能够访问的。然后应该就可以了
