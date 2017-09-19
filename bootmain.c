// Boot loader.
//
// Part of the boot block, along with bootasm.S, which calls bootmain().
// bootasm.S has put the processor into protected 32-bit mode.
// bootmain() loads an ELF kernel image from the disk starting at
// sector 1 and then jumps to the kernel entry routine.

#include "types.h"  // typedef 一些类型缩写
#include "elf.h"    // 定义ELF文件结构体
#include "x86.h"
#include "memlayout.h"

#define SECTSIZE  512

void readseg(uchar*, uint, uint);

void
bootmain(void)
{
  struct elfhdr *elf;
  struct proghdr *ph, *eph;
  void (*entry)(void);
  uchar* pa;

  elf = (struct elfhdr*)0x10000;  // scratch space 设置地址，将内核写入内存0x10000的地址处

  // Read 1st page off disk
  readseg((uchar*)elf, 4096, 0); // 从磁盘上读取0x1000个字节，从0x10000开始放

  // Is this an ELF executable? 判断是否为ELF文件格式
  if(elf->magic != ELF_MAGIC)
    return;  // let bootasm.S handle error

  // Load each program segment (ignores ph flags).
  // 加载各个函数段
  ph = (struct proghdr*)((uchar*)elf + elf->phoff); // elf的程序头部表
  eph = ph + elf->phnum;                            // 程序头部个数
  for(; ph < eph; ph++){
    pa = (uchar*)ph->paddr;                         // 加载每一个程序头部的物理地址偏移
    readseg(pa, ph->filesz, ph->off);               // 读取到内存中
    if(ph->memsz > ph->filesz)                      // 如果内存大小大于文件大小，用 0 补齐内存空位
      stosb(pa + ph->filesz, 0, ph->memsz - ph->filesz);
  }

  // Call the entry point from the ELF header.
  // Does not return!
  entry = (void(*)(void))(elf->entry);              // 进入 ELF 文件的入口开始执行代码
  entry();
}

void
waitdisk(void)
{
  // Wait for disk ready.
  // 读取 0x1F7 的状态，一直循环，直到控制器不忙、驱动器就绪
  while((inb(0x1F7) & 0xC0) != 0x40)
    ;
}

// Read a single sector at offset into dst.
void
readsect(void *dst, uint offset)
{
  // Issue command.
  waitdisk();
  outb(0x1F2, 1);                 // count = 1 读取扇区数目为1
  outb(0x1F3, offset);            // 依次设置扇区的地址位
  outb(0x1F4, offset >> 8);
  outb(0x1F5, offset >> 16);
  outb(0x1F6, (offset >> 24) | 0xE0); // offset | 11100000 保证高三位恒为 1
  outb(0x1F7, 0x20);              // cmd 0x20 - read sectors 向端口发送读取命令

  // Read data.
  waitdisk(); // 等待磁盘就绪
  insl(0x1F0, dst, SECTSIZE/4); // 将512/4=128个 words（4字节）放入内存中的 dst 处
}

// Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
// Might copy more than asked.
void
readseg(uchar* pa, uint count, uint offset)
{
  uchar* epa;

  epa = pa + count; // 结束位置

  // Round down to sector boundary.
  pa -= offset % SECTSIZE; // 位了512个字节对齐？在加载内核的部分似乎并没有用

  // Translate from bytes to sectors; kernel starts at sector 1.
  offset = (offset / SECTSIZE) + 1; // +1 是为了跳过0号扇区，也就是我们的bootblock部分，直接从kernel开始

  // If this is too slow, we could read lots of sectors at a time.
  // We'd write more to memory than asked, but it doesn't matter --
  // we load in increasing order.
  for(; pa < epa; pa += SECTSIZE, offset++)
    readsect(pa, offset); // readsect的部分完成一个扇区512字节的加载任务，加载到pa的地址上
}
