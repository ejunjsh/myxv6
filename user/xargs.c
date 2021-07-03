#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXN 1024

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "usage: xargs command\n");
        exit(1);
    }

    char *_argv[MAXARG];    // 存放子进程exec的参数
    for (int i = 1; i < argc; i++)  // 略去xargs
        _argv[i - 1] = argv[i];
    char buf[MAXN]; // 存放从标准输入转化而来的参数
    char c = 0;
    int stat = 1;   // 从标准输入read返回的状态

    while (stat) // 标准输入中还有数据
    {
        int buf_cnt = 0;    // buf尾指针
        int arg_begin = 0;  // 当前这个参数在buf中开始的位置
        int argv_cnt = argc - 1;
        while (1)   // 读取一行
        {
            stat = read(0, &c, 1);
            if (stat == 0) // 标准输入中没有数据，exit
                exit(0);
            if (c == ' ' || c == '\n')
            {
                buf[buf_cnt++] = 0;
                _argv[argv_cnt++] = &buf[arg_begin];
                arg_begin = buf_cnt;
                if (c == '\n')
                    break;
            }
            else
            {
                buf[buf_cnt++] = c;
            }
        }

        _argv[argv_cnt] = 0;
        if (fork() == 0)
        {
            exec(_argv[0], _argv);
        }
        else
        {
            wait(0);
        }
    }
    exit(0);
}