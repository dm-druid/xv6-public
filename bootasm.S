#include "asm.h"
#include "memlayout.h"
#include "mmu.h"

# Start the first CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

.code16                       # Assemble for 16-bit mode
.globl start
start:
  cli                         # BIOS enabled interrupts; disable 屏蔽处理器中断

  # cli的作用在于将中断机制关掉，
  # 由于开机启动时控制权在BIOS手上，它负责一些硬件的初始化，
  # 这个过程中可能要处理一些硬件中断的问题，
  # 然而此时，控制权已经交给了boot loader（BIOS已经没有在运行了），
  # 这个时候的中断必然会影响boot loader 的正常工作，因而一开始就会用cli这条指令将其关掉。

  # Zero data segment registers DS, ES, and SS.
  xorw    %ax,%ax             # Set %ax to zero
  movw    %ax,%ds             # -> Data Segment
  movw    %ax,%es             # -> Extra Segment
  movw    %ax,%ss             # -> Stack Segment

  # .code16 意味着此时处理器在模拟 Inter 8088 的16位实模式，
  # 尽管该状态下的通用寄存器使用16位地址，但实际上处理器却发送20位地址来进行寻址，
  # 而多处理的这4位则会通过段寄存器(segment register) %cs, %ds, %es, %ss来提供，
  # 这个状态下它们的初值是不确定的，因而代码中对它们清零（%cs用于提取指令，可能因此而不需要清零）

  # Physical address line A20 is tied to zero so that the first PCs
  # with 2 MB would run software that assumed 1 MB.  Undo that.
  # 为了突破实模式下1MB的寻址能力，xv6需要把A20 gate打开
seta20.1:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al 				  # 对0x64端口进行读（in）操作时，判断输入缓冲区是否有数据，
  jnz     seta20.1                # 若等于0x2则说明缓冲区有数据，回到头部，继续等待

  movb    $0xd1,%al               # 0xd1 -> port 0x64
  outb    %al,$0x64               # 向 0x64 端口进行写（out）操作，
                                  # 写入0xd1代表向P2端口发送指令，写入的数据通过0x60端口写入

seta20.2:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.2				  # 同理检查缓冲区

  movb    $0xdf,%al               # 0xdf -> port 0x60
  outb    %al,$0x60               # 向0x60端口写数据，第二位为1代表打开A20 gate，进入保护模式

  # Switch from real to protected mode.  Use a bootstrap GDT that makes
  # virtual addresses map directly to physical addresses so that the
  # effective memory map doesn't change during the transition.
  lgdt    gdtdesc				  # lgdt是一条专门的指令用来将GDT的地址和长度放入GDTR寄存器
  movl    %cr0, %eax
  orl     $CR0_PE, %eax 		  # 将控制寄存器cr0的PE位置1，从而正式打开保护模式
  movl    %eax, %cr0

//PAGEBREAK!
  # Complete the transition to 32-bit protected mode by using a long jmp
  # to reload %cs and %eip.  The segment descriptors are set up with no
  # translation, so that the mapping is still the identity mapping.
  ljmp    $(SEG_KCODE<<3), $start32

.code32  # Tell assembler to generate 32-bit code now.
start32:
  # Set up the protected-mode data segment registers
  movw    $(SEG_KDATA<<3), %ax    # Our data segment selector
  movw    %ax, %ds                # -> DS: Data Segment
  movw    %ax, %es                # -> ES: Extra Segment
  movw    %ax, %ss                # -> SS: Stack Segment
  movw    $0, %ax                 # Zero segments not ready for use
  movw    %ax, %fs                # -> FS 辅助寄存器清零
  movw    %ax, %gs                # -> GS 辅助寄存器清零

  # Set up the stack pointer and call into C.
  movl    $start, %esp			  # 栈顶被设置为 0x7C00 处，也就是bootasm.S最开始的地方，如果失败，栈弹出可重新开始
  call    bootmain

  # If bootmain returns (it shouldn't), trigger a Bochs
  # breakpoint if running under Bochs, then loop.
  movw    $0x8a00, %ax            # 0x8a00 -> port 0x8a00
  movw    %ax, %dx
  outw    %ax, %dx
  movw    $0x8ae0, %ax            # 0x8ae0 -> port 0x8a00
  outw    %ax, %dx
spin:
  jmp     spin

# Bootstrap GDT
.p2align 2                                # force 4 byte alignment
gdt:
  SEG_NULLASM                             # null seg
  SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # code seg
  SEG_ASM(STA_W, 0x0, 0xffffffff)         # data seg

  # gdt:  gdt的宏翻译过来是这样的
  # .word 0, 0;							  # 空段（2个字，32位）
  # .byte 0, 0, 0, 0                      # 空段（4个字节，32位）加起来正好8个字节
  # .word 0xffff, 0x0000;				  # 代码段
  # .byte 0x00, 0x9a, 0xcf, 0x00          # 如果空断为GDT表的第0项，那么代码段作为第一项
  # .word 0xffff, 0x0000;				  # 数据段
  # .byte 0x00, 0x92, 0xcf, 0x00          # 数据段和代码段解析后，唯一的不同就在于两者的身份

gdtdesc:
  .word   (gdtdesc - gdt - 1)             # sizeof(gdt) - 1
  .long   gdt                             # address gdt

