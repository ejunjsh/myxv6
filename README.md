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

## Lab

这个lab跟原来的不同，是每个lab是基于上个lab基础继续做的，所以这样最后代码就基本是所有lab的集合，很有挑战哈

每个lab跑测试之前最好清空下文件

    make clean

### [Utilities（工具）](https://pdos.csail.mit.edu/6.828/2020/labs/util.html)

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


### [System calls（系统调用）](https://pdos.csail.mit.edu/6.828/2020/labs/syscall.html)

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

### [Page tables（页表）](https://pdos.csail.mit.edu/6.828/2020/labs/pgtbl.html)

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

### [Traps（陷阱）](https://pdos.csail.mit.edu/6.828/2020/labs/traps.html)

    $ LAB=traps ./grade-lab-traps
    make: `kernel/kernel' is up to date.
    == Test answers-traps.txt == answers-traps.txt: OK 
    == Test backtrace test == backtrace test: OK (1.1s) 
    == Test running alarmtest == (4.5s) 
    == Test   alarmtest: test0 == 
    alarmtest: test0: OK 
    == Test   alarmtest: test1 == 
    alarmtest: test1: OK 
    == Test   alarmtest: test2 == 
    alarmtest: test2: OK 
    == Test usertests == usertests: OK (305.4s) 
        (Old xv6.out.usertests failure log removed)
    == Test time == 
    time: OK 
    Score: 85/85

### [Lazy page allocation（页的延迟分配）](https://pdos.csail.mit.edu/6.828/2020/labs/lazy.html)

    $ ./grade-lab-lazy
    make: `kernel/kernel' is up to date.
    == Test running lazytests == (1.6s) 
    == Test   lazy: map == 
    lazy: map: OK 
    == Test   lazy: unmap == 
    lazy: unmap: OK 
    == Test usertests == (230.8s) 
    == Test   usertests: pgbug == 
    usertests: pgbug: OK 
    == Test   usertests: sbrkbugs == 
    usertests: sbrkbugs: OK 
    == Test   usertests: argptest == 
    usertests: argptest: OK 
    == Test   usertests: sbrkmuch == 
    usertests: sbrkmuch: OK 
    == Test   usertests: sbrkfail == 
    usertests: sbrkfail: OK 
    == Test   usertests: sbrkarg == 
    usertests: sbrkarg: OK 
    == Test   usertests: stacktest == 
    usertests: stacktest: OK 
    == Test   usertests: execout == 
    usertests: execout: OK 
    == Test   usertests: copyin == 
    usertests: copyin: OK 
    == Test   usertests: copyout == 
    usertests: copyout: OK 
    == Test   usertests: copyinstr1 == 
    usertests: copyinstr1: OK 
    == Test   usertests: copyinstr2 == 
    usertests: copyinstr2: OK 
    == Test   usertests: copyinstr3 == 
    usertests: copyinstr3: OK 
    == Test   usertests: rwsbrk == 
    usertests: rwsbrk: OK 
    == Test   usertests: truncate1 == 
    usertests: truncate1: OK 
    == Test   usertests: truncate2 == 
    usertests: truncate2: OK 
    == Test   usertests: truncate3 == 
    usertests: truncate3: OK 
    == Test   usertests: reparent2 == 
    usertests: reparent2: OK 
    == Test   usertests: badarg == 
    usertests: badarg: OK 
    == Test   usertests: reparent == 
    usertests: reparent: OK 
    == Test   usertests: twochildren == 
    usertests: twochildren: OK 
    == Test   usertests: forkfork == 
    usertests: forkfork: OK 
    == Test   usertests: forkforkfork == 
    usertests: forkforkfork: OK 
    == Test   usertests: createdelete == 
    usertests: createdelete: OK 
    == Test   usertests: linkunlink == 
    usertests: linkunlink: OK 
    == Test   usertests: linktest == 
    usertests: linktest: OK 
    == Test   usertests: unlinkread == 
    usertests: unlinkread: OK 
    == Test   usertests: concreate == 
    usertests: concreate: OK 
    == Test   usertests: subdir == 
    usertests: subdir: OK 
    == Test   usertests: fourfiles == 
    usertests: fourfiles: OK 
    == Test   usertests: sharedfd == 
    usertests: sharedfd: OK 
    == Test   usertests: exectest == 
    usertests: exectest: OK 
    == Test   usertests: bigargtest == 
    usertests: bigargtest: OK 
    == Test   usertests: bigwrite == 
    usertests: bigwrite: OK 
    == Test   usertests: bsstest == 
    usertests: bsstest: OK 
    == Test   usertests: sbrkbasic == 
    usertests: sbrkbasic: OK 
    == Test   usertests: kernmem == 
    usertests: kernmem: OK 
    == Test   usertests: validatetest == 
    usertests: validatetest: OK 
    == Test   usertests: opentest == 
    usertests: opentest: OK 
    == Test   usertests: writetest == 
    usertests: writetest: OK 
    == Test   usertests: writebig == 
    usertests: writebig: OK 
    == Test   usertests: createtest == 
    usertests: createtest: OK 
    == Test   usertests: openiput == 
    usertests: openiput: OK 
    == Test   usertests: exitiput == 
    usertests: exitiput: OK 
    == Test   usertests: iput == 
    usertests: iput: OK 
    == Test   usertests: mem == 
    usertests: mem: OK 
    == Test   usertests: pipe1 == 
    usertests: pipe1: OK 
    == Test   usertests: preempt == 
    usertests: preempt: OK 
    == Test   usertests: exitwait == 
    usertests: exitwait: OK 
    == Test   usertests: rmdot == 
    usertests: rmdot: OK 
    == Test   usertests: fourteen == 
    usertests: fourteen: OK 
    == Test   usertests: bigfile == 
    usertests: bigfile: OK 
    == Test   usertests: dirfile == 
    usertests: dirfile: OK 
    == Test   usertests: iref == 
    usertests: iref: OK 
    == Test   usertests: forktest == 
    usertests: forktest: OK 
    == Test time == 
    time: OK 
    Score: 119/119

### [Copy-on-Write Fork（fork的写时复制）](https://pdos.csail.mit.edu/6.828/2020/labs/cow.html)

    $ ./grade-lab-cow 
    == Test running cowtest == (11.4s) 
    == Test   simple == 
    simple: OK 
    == Test   three == 
    three: OK 
    == Test   file == 
    file: OK 
    == Test usertests == (192.0s) 
    == Test   usertests: copyin == 
    usertests: copyin: OK 
    == Test   usertests: copyout == 
    usertests: copyout: OK 
    == Test   usertests: all tests == 
    usertests: all tests: OK 
    == Test time == 
    time: OK 
    Score: 110/110

### [Multithreading（多线程）](https://pdos.csail.mit.edu/6.828/2020/labs/thread.html)

    $ ./grade-lab-thread
    == Test uthread == uthread: OK (7.0s) 
    == Test answers-thread.txt == answers-thread.txt: OK 
    == Test ph_safe == gcc -o ph -g -O2 notxv6/ph.c -pthread
    ph_safe: OK (8.9s) 
    == Test ph_fast == make: `ph' is up to date.
    ph_fast: OK (18.1s) 
    == Test barrier == gcc -o barrier -g -O2 notxv6/barrier.c -pthread
    barrier: OK (2.3s) 
    == Test time == 
    time: OK 
    Score: 60/60

### [Lock（锁）](https://pdos.csail.mit.edu/6.828/2020/labs/lock.html)

    $ LAB=lock ./grade-lab-lock
    make: `kernel/kernel' is up to date.
    == Test running kalloctest == (413.1s) 
    == Test   kalloctest: test1 == 
    kalloctest: test1: OK 
    == Test   kalloctest: test2 == 
    kalloctest: test2: OK 
    == Test kalloctest: sbrkmuch == kalloctest: sbrkmuch: OK (18.8s) 
    == Test running bcachetest == (16.6s) 
    == Test   bcachetest: test0 == 
    bcachetest: test0: OK 
    == Test   bcachetest: test1 == 
    bcachetest: test1: OK 
    == Test usertests == usertests: OK (244.3s) 
    == Test time == 
    time: OK 
    Score: 70/70

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