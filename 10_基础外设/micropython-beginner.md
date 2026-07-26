# MicroPython基础

> **原文**: <https://wiki.lckfb.com/zh-hans/lushan-pi-k230/basic/micropython-beginner.html>
> **最后更新**: 2025-03-03

---

指的是从num1开始，到num2结束（不含num2本身）

## 7 函数 ​

函数是组织好的，可重复使用的，用来实现特定功能的代码段。 函数的定义：

c
    
    
    def 函数名(传入参数):
        函数体
        return 返回值

1  
2  
3  


> 注意： 如果函数没有使用return语句返回数据，会返回None这个字面量；在if判断中，None等同于False；定义变量，但暂时不需要变量有具体值，可以用None来代替。

使用 global关键字 可以在函数内部声明变量为全局变量，相当于C语言里的static。

C
    
    
    def test():
        global num
        num = 200
        print(num)

1  
2  
3  
4  


## 8 类和继承 ​

在Python中，可以通过定义类来实现面向对象编程。类包含数据和函数，数据保存在类的属性中，而函数保存在类的方法中。通过创建类，可以生成相同类型（或者父类）的多个对象，它们共享相同的属性和方法。

继承是指一个类可以派生出另一个子类，而子类继承了父类的属性和方法。子类可以进一步重载父类的方法或者添加新的属性和方法，从而实现对父类的扩展。

  1. 定义类：



使用 class 关键字定义一个类，并使用代码块来编写类的属性和方法。例如：

c
    
    
    class MyClass:
        def __init__(self, param):
            self.param = param
    
        def method(self):
            # 方法实现
            pass

1  
2  
3  
4  
5  
6  
7  


  2. 实例化对象：



通过调用类的构造函数，可以创建类的实例（对象）。例如：

c
    
    
    my_object = MyClass("value")

1  


  3. 访问属性和调用方法：



使用对象名后跟 . 来访问对象的属性和方法。例如：

c
    
    
    value = my_object.param
    my_object.method()

1  
2  


  4. 继承：



在 microPython 中，您可以使用继承来创建一个类从另一个类继承属性和方法。通过继承，子类可以获得父类的特征并添加自己的特定功能。例如：

c
    
    
    class ChildClass(MyClass):
        def __init__(self, param, child_param):
            super().__init__(param)
            self.child_param = child_param
    
        def child_method(self):
            # 子类方法实现
            pass

1  
2  
3  
4  
5  
6  
7  
8  


在上面的示例中，ChildClass 继承了 MyClass，并添加了自己的属性和方法。super().**init**(param) 调用父类的构造函数。

  5. 多重继承： microPython 支持多重继承，即一个类可以从多于一个父类继承。多重继承可通过在类定义中列出多个父类来实现。例如：



C
    
    
    class ChildClass(ParentClass1, ParentClass2): # 类定义
    pass

1  
2  


这些是 microPython 中类和继承的基本用法。类和继承是面向对象编程的核心概念，可以帮助您组织和抽象代码，实现代码的重用和扩展性。

