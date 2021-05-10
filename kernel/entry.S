	# qemu -kernel 加载内核到地址0x80000000
        # 并让所有cpu都跳到这里
        # kernel.ld 会把下面代码都链接到地址0x80000000上.
.section .text
_entry:
        # 配置一个栈给C
        # stack0声明在start.c
        # 每个cpu都有一个4096字节的栈
        # sp = stack0 + (hartid * 4096)
        la sp, stack0
        li a0, 1024*4
	csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        # 跳到start.c的start()函数里
        call start
spin:
        j spin
