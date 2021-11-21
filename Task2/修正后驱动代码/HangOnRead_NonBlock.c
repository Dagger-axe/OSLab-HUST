#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <fcntl.h>

char buf[8];

int main(){
    int fd = open("/dev/BFDev", O_RDWR | O_NONBLOCK);  //以可读写方式打开设备文件，阻塞方式
    if (fd < 0) {
        printf("Open failed.\n");
        return -1;
    }

    pid_t son = fork(), grandson;
    if (son < 0) return -1;
    else if (son > 0) {
        grandson = fork();
        if (grandson < 0) return -1;
        else if (grandson > 0) {
            sleep(1);
            write(fd, "1", 1);
        } else {
            read(fd, buf, 1);
            exit(0);
        }
    } else {
        read(fd, buf, 1);
        exit(0);
    }

    wait(NULL);
    wait(NULL);
    close(fd);
    
    return 0;
}
