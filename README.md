# myxv6

看代码并加上中文注释，还有就是做lab

代码基于2020 MIT 6.S081的xv6-riscv，lab也基于此

## 环境

我只在macos intel x86调试，所以下面环境就是macos环境要安装的，brew自行安装吧😄

    $ brew tap riscv/riscv
    $ brew install riscv-tools

重新配置下PATH环境变量，把riscv工具相关二进制加入到PATH

    PATH=$PATH:/usr/local/opt/riscv-gnu-toolchain/bin

最后安装qemu

    brew install qemu

原来xv6的代码在最新qemu里面起来卡住，改了些代码后可以启动，

所以不知道后面更新的qemu是否还能启动，

所以记下现在运行的版本：

    $ riscv64-unknown-elf-gcc --version
    riscv64-unknown-elf-gcc (GCC) 10.2.0
    Copyright (C) 2020 Free Software Foundation, Inc.
    This is free software; see the source for copying conditions.  There is NO
    warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    $ qemu-system-riscv64 --version
    QEMU emulator version 6.0.0
    Copyright (c) 2003-2021 Fabrice Bellard and the QEMU Project developers


## 运行

    $ make qemu

## 退出qemu:

    ctrl+a,松开,按x

## 测试

   $ usertests # 进入系统后执行

## lab

每个lab跑下面每个测试之前最好清空下文件

    make clean

### [Lab Utilities](https://pdos.csail.mit.edu/6.828/2020/labs/util.html)

    $ ./grade-lab-util                             
    make: `kernel/kernel' is up to date.
    == Test sleep, no arguments == sleep, no arguments: OK (1.8s) 
    == Test sleep, returns == sleep, returns: OK (0.7s) 
    == Test sleep, makes syscall == sleep, makes syscall: OK (0.9s) 
    == Test pingpong == pingpong: OK (1.0s) 
    == Test primes == primes: OK (1.0s) 
    == Test find, in current directory == find, in current directory: OK (1.2s) 
    == Test find, recursive == find, recursive: OK (1.6s) 
    == Test xargs == xargs: OK (1.5s) 
    == Test time == 
    time: OK 
    Score: 100/100


### [Lab System calls](https://pdos.csail.mit.edu/6.828/2020/labs/syscall.html)

    $ ./grade-lab-syscall
    make: `kernel/kernel' is up to date.
    == Test trace 32 grep == trace 32 grep: OK (1.5s) 
    == Test trace all grep == trace all grep: OK (0.8s) 
    == Test trace nothing == trace nothing: OK (1.1s) 
    == Test trace children == trace children: OK (15.1s) 
    == Test sysinfotest == sysinfotest: OK (2.5s) 
    == Test time == 
    time: OK 
    Score: 35/35

### [Lab Page tables](https://pdos.csail.mit.edu/6.828/2020/labs/pgtbl.html)

    $ LAB=pgtbl ./grade-lab-pgtbl
    make: `kernel/kernel' is up to date.
    == Test pte printout == pte printout: OK (0.9s) 
    == Test answers-pgtbl.txt == answers-pgtbl.txt: OK 
    == Test count copyin == count copyin: OK (0.9s) 
    == Test usertests == (287.8s) 
    == Test   usertests: copyin == 
    usertests: copyin: OK 
    == Test   usertests: copyinstr1 == 
    usertests: copyinstr1: OK 
    == Test   usertests: copyinstr2 == 
    usertests: copyinstr2: OK 
    == Test   usertests: copyinstr3 == 
    usertests: copyinstr3: OK 
    == Test   usertests: sbrkmuch == 
    usertests: sbrkmuch: OK 
    == Test   usertests: all tests == 
    usertests: all tests: OK 
    == Test time == 
    time: OK 
    Score: 66/66

## 来自书的截图

![](https://github.com/ejunjsh/myxv6/raw/main/res/1.1.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/2.3.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/3.1.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/3.2.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/3.3.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/3.4.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/7.1.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/8.1.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/8.2.png)
![](https://github.com/ejunjsh/myxv6/raw/main/res/8.3.png)

## 参考

https://pdos.csail.mit.edu/6.828/2020/xv6.html

https://github.com/mit-pdos/xv6-riscv

https://pdos.csail.mit.edu/6.828/2020/tools.html