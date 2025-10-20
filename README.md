
# 命令行参数解析

命令行参数
>要求
## 实现短参数
* 短参数以-开头, 多个可选短参数可以合并, 带值参数不能合并
``` bash
command -a -b -c
command -abc
command -p 10
command -p10
```

## 实现长参数
* 参数以--开头, 不能合并参数
```bash
command --parameter=10
command --parameter 10

# 必须参数需要提供值
# 可选参数, 可以预设值, 也可以不提供预设值

```