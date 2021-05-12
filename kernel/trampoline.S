	#
        # 从用户空间切换到内核空间的代码
        #
        # 这段代码内核空间和用户空间都映射到同一个虚拟内存地址 (TRAMPOLINE)
	# 所以这段代码即使是切换页表的时候都能够正确运行
	#
        # kernel.ld 令代码跟页边界对齐
	.section trampsec
.globl trampoline
trampoline:
.align 4
.globl uservec
uservec:    
	#
        # trap.c 设置stvec指向这里，所以用户空间的陷阱都会跳到这里
        # 这时候是内核模式，同时页表是用户页表
        #
        # sscratch 指向进程p->trapframe在用户空间映射后的地址TRAPFRAME
        #
        
	# a0 和 sscratch 交换
        # 所以 a0 就是 TRAPFRAME
        csrrw a0, sscratch, a0

        # 保存用户寄存器在TRAPFRAME
        sd ra, 40(a0)
        sd sp, 48(a0)
        sd gp, 56(a0)
        sd tp, 64(a0)
        sd t0, 72(a0)
        sd t1, 80(a0)
        sd t2, 88(a0)
        sd s0, 96(a0)
        sd s1, 104(a0)
        sd a1, 120(a0)
        sd a2, 128(a0)
        sd a3, 136(a0)
        sd a4, 144(a0)
        sd a5, 152(a0)
        sd a6, 160(a0)
        sd a7, 168(a0)
        sd s2, 176(a0)
        sd s3, 184(a0)
        sd s4, 192(a0)
        sd s5, 200(a0)
        sd s6, 208(a0)
        sd s7, 216(a0)
        sd s8, 224(a0)
        sd s9, 232(a0)
        sd s10, 240(a0)
        sd s11, 248(a0)
        sd t3, 256(a0)
        sd t4, 264(a0)
        sd t5, 272(a0)
        sd t6, 280(a0)

	# 保存用户a0 在 p->trapframe->a0
        csrr t0, sscratch
        sd t0, 112(a0)

        # 从 p->trapframe->kernel_sp 恢复内核栈地址
        ld sp, 8(a0)

        # 从 p->trapframe->kernel_hartid 恢复hartid到tp
        ld tp, 32(a0)

        # 从p->trapframe->kernel_trap恢复usertrap()到t0
        ld t0, 16(a0)

        # 从p->trapframe->kernel_satp恢复内核页表
        ld t1, 0(a0)
        csrw satp, t1
        sfence.vma zero, zero

        # a0 已经无效了，因为内核的页表没有指定映射到p->tf

        # 跳到usertrap(), 不用返回
        jr t0

.globl userret
userret:
        # userret(TRAPFRAME, pagetable)
        # 从内核返回到用户模式
        # usertrapret() 调用这里.
        # a0: TRAPFRAME用户空间的地址
        # a1: 用户页表, 用来放到satp.

        # 切换到用户页表
        csrw satp, a1
        sfence.vma zero, zero

        # 把之前保存的用户a0放回sscratch
        # 这样最后一步就可以跟a0 (TRAPFRAME)交换回来
        ld t0, 112(a0)
        csrw sscratch, t0

        # 从TRAPFRAME恢复所有寄存器除了a0
        ld ra, 40(a0)
        ld sp, 48(a0)
        ld gp, 56(a0)
        ld tp, 64(a0)
        ld t0, 72(a0)
        ld t1, 80(a0)
        ld t2, 88(a0)
        ld s0, 96(a0)
        ld s1, 104(a0)
        ld a1, 120(a0)
        ld a2, 128(a0)
        ld a3, 136(a0)
        ld a4, 144(a0)
        ld a5, 152(a0)
        ld a6, 160(a0)
        ld a7, 168(a0)
        ld s2, 176(a0)
        ld s3, 184(a0)
        ld s4, 192(a0)
        ld s5, 200(a0)
        ld s6, 208(a0)
        ld s7, 216(a0)
        ld s8, 224(a0)
        ld s9, 232(a0)
        ld s10, 240(a0)
        ld s11, 248(a0)
        ld t3, 256(a0)
        ld t4, 264(a0)
        ld t5, 272(a0)
        ld t6, 280(a0)

        # 恢复用户a0，保存TRAPFRAME到sscratch
        csrrw a0, sscratch, a0
        
        # 返回用户模式和用户程序计数器（pc）
        # usertrapret() 已经设置好 sstatus 和 sepc.
        sret
