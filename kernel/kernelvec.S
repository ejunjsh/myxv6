	#
        # 内核模式下中断和异常的处理
        # 把所有寄存器压栈，调用kerneltrap(),恢复寄存器，返回
        #
.globl kerneltrap
.globl kernelvec
.align 4
kernelvec:
        // 为保存寄存器腾出空间
        addi sp, sp, -256

        // 保存寄存器
        sd ra, 0(sp)
        sd sp, 8(sp)
        sd gp, 16(sp)
        sd tp, 24(sp)
        sd t0, 32(sp)
        sd t1, 40(sp)
        sd t2, 48(sp)
        sd s0, 56(sp)
        sd s1, 64(sp)
        sd a0, 72(sp)
        sd a1, 80(sp)
        sd a2, 88(sp)
        sd a3, 96(sp)
        sd a4, 104(sp)
        sd a5, 112(sp)
        sd a6, 120(sp)
        sd a7, 128(sp)
        sd s2, 136(sp)
        sd s3, 144(sp)
        sd s4, 152(sp)
        sd s5, 160(sp)
        sd s6, 168(sp)
        sd s7, 176(sp)
        sd s8, 184(sp)
        sd s9, 192(sp)
        sd s10, 200(sp)
        sd s11, 208(sp)
        sd t3, 216(sp)
        sd t4, 224(sp)
        sd t5, 232(sp)
        sd t6, 240(sp)

        // 调用C的trap处理函数，这个函数在trap.C
        call kerneltrap

        // 恢复寄存器
        ld ra, 0(sp)
        ld sp, 8(sp)
        ld gp, 16(sp)
        // 这个不需要恢复，因为tp是存cpu id的地方，不变的，如果恢复，有可能会修改掉了: ld tp, 24(sp)
        ld t0, 32(sp)
        ld t1, 40(sp)
        ld t2, 48(sp)
        ld s0, 56(sp)
        ld s1, 64(sp)
        ld a0, 72(sp)
        ld a1, 80(sp)
        ld a2, 88(sp)
        ld a3, 96(sp)
        ld a4, 104(sp)
        ld a5, 112(sp)
        ld a6, 120(sp)
        ld a7, 128(sp)
        ld s2, 136(sp)
        ld s3, 144(sp)
        ld s4, 152(sp)
        ld s5, 160(sp)
        ld s6, 168(sp)
        ld s7, 176(sp)
        ld s8, 184(sp)
        ld s9, 192(sp)
        ld s10, 200(sp)
        ld s11, 208(sp)
        ld t3, 216(sp)
        ld t4, 224(sp)
        ld t5, 232(sp)
        ld t6, 240(sp)

        addi sp, sp, 256

        // 返回到中断之前内核正在做的地方
        sret

        #
        # 机器模式下的定时中断处理
        #
.globl timervec
.align 4
timervec:
        # start.c 已经设置内存空间，并让mscratch指向它:
        # scratch[0,8,16] : 寄存器保存区域.
        # scratch[24] : CLINT MTIMECMP 寄存器地址.
        # scratch[32] : 中断之间的间隔.
        
        # 保存寄存器的值
        csrrw a0, mscratch, a0
        sd a1, 0(a0)
        sd a2, 8(a0)
        sd a3, 16(a0)

        # 配置下次定时中断
        # 通过给mtimecmp加上个间隔时间
        ld a1, 24(a0) # CLINT_MTIMECMP(hart)
        ld a2, 32(a0) # interval
        ld a3, 0(a1)
        add a3, a3, a2
        sd a3, 0(a1)

        # 发起一个内核模式下的软件中断
	li a1, 2
        csrw sip, a1

        # 恢复寄存器的值
        ld a3, 16(a0)
        ld a2, 8(a0)
        ld a1, 0(a0)
        csrrw a0, mscratch, a0

        mret
