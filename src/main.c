#include <stdio.h>
#include "server.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
//avg的参数个数会填到arg里
//   ./ConsoleApplication1.out port path  一共3个参数
//   port => avg[1]    path => avg[2]
// path是服务器的资源目录 也是工作目录  ./就是工作目录

int main(int arg,char * avg[])
{
    // https://192.168.42.141:8080
    //如果输入的参数小于3 提示一下
    if (arg < 2) {
        printf("./ConsoleApplication1.out port path\n");
        return 0;
    }
    int port = atoi(avg[1]);

    chdir("../"); //工作路径上移
    // char buf[1000];
    // getcwd(buf,sizeof buf);
    // printf("%s\n",buf);
    int server_sock = initListenFd(port);

    EpollRun(server_sock);
    printf("\n结束:server_sock=%d\n", server_sock);

    return 0;
}