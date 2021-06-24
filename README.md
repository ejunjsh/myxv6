# myxv6

看代码并加上中文注释

## 环境

我只在macos调试，所以下面环境就是macos环境要安装的，brew自行安装吧😄

    $ brew tap riscv/riscv
    $ brew install riscv-tools

重新配置下PATH环境变量，把riscv工具相关二进制加入到PATH

    PATH=$PATH:/usr/local/opt/riscv-gnu-toolchain/bin

最后安装qemu

    brew install qemu

原来xv6的代码在最新qemu里面起来卡住，改了些代码后可以启动，

所以不知道后面更新的qemu是否还能启动，

所以记下现在运行的qemu版本：

    $ brew info qemu
    qemu: stable 6.0.0 (bottled), HEAD
    Emulator for x86 and PowerPC
    ...

退出qemu:

    ctrl+a,松开,按x

## 运行

    $ make qemu

## 来自书的截图

![](https://github.com/ejunjsh/myxv6/raw/main/res/1.1.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/2.3.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/3.1.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/3.2.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/3.4.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/7.1.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/8.1.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/8.2.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/8.3.png)

## 参考

https://pdos.csail.mit.edu/6.828/2020/xv6.html

https://github.com/mit-pdos/xv6-riscv

https://pdos.csail.mit.edu/6.828/2020/tools.html