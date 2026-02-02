#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

// 链接脚本中定义的符号，代表 RAM 中静态数据的结束地址，也就是堆的起始地址
extern char _end; 

// 只是为了解决链接错误，这是最简版本
// 如果你的程序真的大量使用 malloc，需要加上堆栈碰撞检测
caddr_t _sbrk(int incr)
{
    static char *heap_end;
    char *prev_heap_end;

    // 第一次调用时，将堆指针初始化为 _end
    if (heap_end == 0)
    {
        heap_end = &_end;
    }

    prev_heap_end = heap_end;
    
    // 简单的指针移动，不检查溢出（为了节省空间和避免复杂依赖）
    heap_end += incr;

    return (caddr_t) prev_heap_end;
}

// ============================================================================
// 下面是其他一些可能缺少的桩函数 (Stubs)
// 如果以后报 undefined reference to _write, _close 等，这个文件也能解决
// ============================================================================

int _close(int file)
{
    return -1;
}

int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}

int _read(int file, char *ptr, int len)
{
    return 0;
}


int _write(int file, char *ptr, int len)
{
    return len;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    errno = EINVAL;
    return -1;
}

