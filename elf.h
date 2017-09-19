// Format of an ELF executable file

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// File header
struct elfhdr {
  uint magic;  // must equal ELF_MAGIC 用于判断是否为ELF文件magic number
  uchar elf[12];    // 12 字节，每字节对应意义如下：
                    //     0 : 1 = 32 位程序；2 = 64 位程序
                    //     1 : 数据编码方式，0 = 无效；1 = 小端模式；2 = 大端模式
                    //     2 : 只是版本，固定为 0x1
                    //     3 : 目标操作系统架构
                    //     4 : 目标操作系统版本
                    //     5 ~ 11 : 固定为 0
                    //     总之定义了ELF文件的许多信息，包括编码方式、版本、操作系统之类的。
  ushort type;      // 是何种文件类型（可重定向、可执行文件、共享目标文件...）
  ushort machine;   // 体系架构，0x3 为 x86架构
  uint version;
  uint entry;       // 入口
  uint phoff;       // 程序头部表的偏移
  uint shoff;
  uint flags;
  ushort ehsize;    // elf文件头部的大小
  ushort phentsize; // 程序头部表的一个入口的大小
  ushort phnum;     // 程序头部表入口的个数
  ushort shentsize;
  ushort shnum;
  ushort shstrndx;
};

// Program section header 程序头表
struct proghdr {
  uint type;    // 加载方式
  uint off;     // 段在文件中的偏移
  uint vaddr;   // 第一个字节的虚拟地址
  uint paddr;   // 第一个字节的物理地址
  uint filesz;  // 段在文件中的大小
  uint memsz;   // 段在内存中的大小（和上面的有何区别？无法理解）
  uint flags;
  uint align;
};

// Values for Proghdr type
#define ELF_PROG_LOAD           1

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4
