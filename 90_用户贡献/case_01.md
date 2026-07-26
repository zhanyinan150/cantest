# 庐山派主动散热顶板

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/user-contribute/case/01.html>
> **最后更新**: 2024-12-09

---

# 庐山派主动散热顶板 ​

NOTE

感谢来自用户【摩西摩西】的贡献，以下文档来自该用户，不代表 立创开发板 的观点及立场。

嘉立创庐山派的算力很强大，但是不知道各位小伙伴在用庐山派跑模型的时候有没有发现，K230芯片满负载工作大概在60度左右，请勿直接上手摸。即使嘉立创贴心的为我们准备了散热片，但还是挡不住芯片的升温。因此，庐山派主动散热顶板就诞生了。

![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/user-contribute/01/01_20241209_171322.png)

![图 0](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/user-contribute/case/01/01_20241209_171419.png)

# 一、功能简介 ​

  1. 庐山派主动散热顶板搭载一个3007风扇，使用庐山派的GPIO2和GPIO4进行5V供电，能够提供强劲的风力，且在顶板上专门开有出风孔，让板上的K230能够始终保持常温运行，为模型的稳定运行提供帮助；
  2. 庐山派主动散热顶板将摄像头开孔位置左移。很多小伙伴可能发现嘉立创原装的亚克力顶板有一个小问题，就是摄像头开孔位置略微靠右，导致摄像头的Fpc线有些偏移，主动散热顶板解决了这一问题，让摄像头开孔位置能够和开发板上的摄像头接口正对着，整体更加美观；
  3. 庐山派主动散热顶板将摄像头隐藏在平面以下，这样即使平时不小心把开发板弄掉地上了，也不会损坏摄像头。需要出门携带时，也不用担心摄像头被刮花，可以随意放在包里。



# 二、所需材料 ​

  1. M3*22螺柱 4个；
  2. M2*7螺柱 2个；
  3. M2*5螺丝 2个；
  4. 3007风扇 1个（购买链接：[适用树莓派散热风扇 3007 5V 4B散热器 30 _30_ 7 降温 带螺丝-淘宝网](https://item.taobao.com/item.htm?abbucket=4&id=632304907719&ns=1&pisk=fmDoLuiOYbPSzof0mIyWTQT1i7dYmgwQN2BLJJUegrzfeWzpN2VnJqNEe8n-oyut-kH-pvcfKciIe3wpV8i7dJ89WdenV0wBVU7qrYqVgloVaTyFr0y54J89WdCA07T8L4EIMF6Vgla44kPU4n24vu7ULWrr0nqUva5FU2-m0rZCTwzU4ZS4fk6zLkzzgrrQbgzFUTy23rZULykL16aiPVH2ypBNojodkYquZyXLq9VjIOF770aVLeo0qFaZ4rXFKz35Y3im2UXsD8iqSl3J3T0ifxg00VJHoJgjT2rnud_LFf3SCSmX_toYErNabx-FQyHoMcz0IMJrm84u_YFPv93ZmDlTixKwC82zzXeSvGY-mY0-VxmdYsyuejViERJ1zyhxjvqnde9mScMjoouwKUSz4s5wHxBQ0HHVO6Nzco49Ji1kSPPzh7xDm194aoZWnvKcTQFzcuhymnfNT7rbVLf..&priceTId=213e003517334870187988282e188f&skuId=4678601803322&spm=a21n57.1.hoverItem.12&utparam=%7B%22aplus_abtest%22%3A%224ccae1e54993770863690b24523515b5%22%7D&xxc=taobaoSearch)）；
  5. 3D打印顶板一块（也可使用嘉立创一元cnc）。



# 三、组装教程 ​

  1. 点击[庐山派顶板_带风扇版本 来自 RomanticMachine - MakerWorld](https://makerworld.com.cn/zh/models/703696#profileId-655143)下载顶板模型，用3D打印机将模型打印出来，如果自己没有3D打印机，也可以使用嘉立创3D打印，给您提供完美的打印服务；

  2. 将嘉立创送的四个M3*8螺柱换成M3*22螺柱，增加顶板和开发板之间的距离，为风扇提供空间；

  3. 将风扇用商家送的四个螺丝和螺母固定在顶板上。需注意带标签的一面朝外，且供电线在右侧（方便连接IO口进行供电）；![图 1](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/user-contribute/case/01/01_20241209_171444.png)

  4. 用两个M2*5螺丝将两个M2*7螺柱固定在摄像头上，可以在摄像头和顶板之间起到固定作用；![图 2](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/user-contribute/case/01/01_20241209_171456.png)

  5. 将风扇的正负极（红正黑负）分别插在庐山派的GPIO2和GPIO4上（详细GPIO口功能见庐山派接口定义图）；![图 3](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/user-contribute/case/01/01_20241209_171509.png)

  6. 用两个M2*5螺丝将摄像头固定在顶板上，最后再将顶板用嘉立创送的四个螺丝与M3*22螺柱连接起来，这样你的庐山派就拥有主动散热功能啦，哪怕跑多久模型都不怕了！!![图 4](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/user-contribute/case/01/01_20241209_171521.png)




# 四、更优雅版本 ​

如果你觉得这个方案的螺丝头突出来了，还是不够优雅，那我还制作了**更优雅版本** ，采用沉头座的方案，将顶板和底板的螺丝头都做到了和板面平齐，让你的庐山派**极致优雅** ！

![图 5](https://wiki.lckfb.com/storage/images/zh-hans/lushan-pi-k230/user-contribute/case/01/01_20241209_171533.png)

更优雅版本组装步骤与上面一致，具体模型见链接：[庐山派顶板+底板_优雅版 来自 RomanticMachine - MakerWorld](https://makerworld.com.cn/zh/models/706387#profileId-658430)

注：因3007风扇商家送的螺丝是圆头的，若您想追求极致完美，可以再购买4个M2.5*12的平头螺丝用于安装风扇。

