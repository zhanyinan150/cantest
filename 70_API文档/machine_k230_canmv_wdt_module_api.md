# 2.12 WDT 模块 API 手册

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/api/machine/k230_canmv_wdt_module_api.html>
> **最后更新**: 2025-01-10

---

# 2.12 `WDT` 模块 API 手册 ​

## 1\. 概述 ​

K230 内部集成了两个 WDT（看门狗定时器）硬件模块，旨在确保系统在应用程序崩溃并进入不可恢复状态时能够重启。WDT 在启动后，如果硬件运行期间未定期进行“喂狗”操作，将会在超时后自动复位系统。

## 2\. API 介绍 ​

WDT 类位于 `machine` 模块中。

### 2.1 示例代码 ​

python
    
    
    from machine import WDT
    
    # 实例化 WDT1，超时时间为 3 秒
    wdt1 = WDT(1, 3)
    
    # 执行喂狗操作
    wdt1.feed()

1  
2  
3  
4  
5  
6  
7  


### 2.2 构造函数 ​

python
    
    
    wdt = WDT(id=1, timeout=5)

1  


**参数**

  * `id`: WDT 模块编号，取值范围为 [0, 1]，默认为 1。
  * `timeout`: 超时值，单位为秒（s），默认为 5。



**注意：** WDT0 暂不可用。

### 2.3 `feed` 方法 ​

python
    
    
    WDT.feed()

1  


执行喂狗操作。

**参数**

无

**返回值**

无

