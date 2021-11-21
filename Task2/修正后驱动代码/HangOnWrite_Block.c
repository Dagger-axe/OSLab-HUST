#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <fcntl.h>

char wbuf[64] = "1234567890123456789012345678901234567890123456789012345678901234";
char rbuf[64];

int main(){
    int fd = open("/dev/BFDev", O_RDWR);  //以可读写方式打开设备文件，阻塞方式
    if (fd < 0) {
        printf("Open failed.\n");
        return -1;
    }
    write(fd, &wbuf, 64);  //先将缓冲区填充完整

    pid_t son = fork(), grandson;
    if (son < 0) return -1;
    else if (son > 0) {
        grandson = fork();
        if (grandson < 0) return -1;
        else if (grandson > 0) {
            sleep(1);  //等待所有write被挂起
            read(fd, &rbuf, 64);
        } else {
            write(fd, &wbuf, 64);
            exit(0);
        }
    } else {
        write(fd, &wbuf, 64);
        exit(0);
    }

    wait(NULL);
    wait(NULL);
    close(fd);
    
    return 0;
}
