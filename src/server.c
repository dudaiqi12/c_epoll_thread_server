#include "server.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <stdio.h> //NULL
#include <unistd.h>
#include <fcntl.h>
#include <string.h> //memcpy
#include <error.h>
#include <sys/stat.h>
#include <stdlib.h> //free
#include <dirent.h> //opendir
#include <pthread.h>
struct Info{
    int fd;
    pthread_t client_thread_pid;
    int epoll;
};
int initListenFd(int port)
{
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        perror("socket");
        return -1;
    }
    int opt = 1;
    int ret = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
    if (ret == -1)
    {
        perror("setsockopt");
        return -1;
    }

    struct sockaddr_in sockaddr_in;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    sockaddr_in.sin_port = htons(port);

    ret = bind(server_sock, (struct sockaddr *)&sockaddr_in, sizeof sockaddr_in);
    if (ret == -1)
    {
        perror("bind");
        return -1;
    }

    ret = listen(server_sock, 128);
    if (ret == -1)
    {
        perror("listen");
        return -1;
    }
    return server_sock;
}

int EpollRun(int server_sock)
{

    int epoll = epoll_create(1);
    if (epoll == -1)
    {
        perror("epoll_create");
        return -1;
    }

    struct epoll_event event;
    event.data.fd = server_sock;
    event.events = EPOLLIN | EPOLLOUT;
    int epollCtl = epoll_ctl(epoll, EPOLL_CTL_ADD, server_sock, &event);
    if (epollCtl == -1)
    {
        perror("epollCtl");
        return -1;
    }

    struct epoll_event ep_events[1024]; // = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);
    int triggered_event_num;
    while (1)
    {

        // printf("asdasdasds\n");
        triggered_event_num = epoll_wait(epoll, ep_events, sizeof(ep_events), -1);//-1是阻塞 0是非阻塞
        // printf("triggered_event_num: %d\n", triggered_event_num); //-1
        if (triggered_event_num == -1)
        {
            perror("event_cnt");
            return -1;
        }

        int i = 0;
        for (; i < triggered_event_num; i++)
        {
            struct Info* info;
            memset(info,0,sizeof info);
            info->fd = ep_events[i].data.fd;
            info->epoll = epoll;
            if (ep_events[i].data.fd == server_sock)
            {
                //为什么用线程呢？因为这样建立连接和发数据的时候 主线程就不会被阻塞
                //能快速的进入下一个while循环 快速的处理请求
                pthread_create(&info->client_thread_pid,NULL,acceptClient,info);
                //acceptClient(server_sock, epoll);
                //第一次提交分支
            }
            else
            {

                //recvHttpRequest(ep_events[i].data.fd, epoll);
                pthread_create(&info->client_thread_pid,NULL,recvHttpRequest,info);
                //printf("\n----------------------\n");
            }
        }
    }

    
    return 0;
}


int acceptClient(int server_sock, int epoll)
{

    int client_sock = accept(server_sock, NULL, 0);

    int flags = fcntl(client_sock, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(client_sock, F_SETFL, flags);

    // fcntl(client_sock, F_SETFD, flag);

    struct epoll_event event;
    event.data.fd = client_sock;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epoll, EPOLL_CTL_ADD, client_sock, &event);
    // printf("client connect : %d\n", client_sock);
    return client_sock;
}


int echo(int ep_event_data_fd, int epoll)
{

    char read_buf[1024];
    int read_len = recv(ep_event_data_fd, read_buf, sizeof read_buf, 0);

    if (read_len == 0)
    {

        epoll_ctl(epoll, EPOLL_CTL_DEL, ep_event_data_fd, NULL);

        close(ep_event_data_fd);
        printf("�ͻ��˶Ͽ�����: %d\n", ep_event_data_fd);
    }
    else
    {

        send(ep_event_data_fd, read_buf, sizeof read_buf, 0);
    }
    return 0;
}

int recvHttpRequest(int client_sock, int epoll)
{
    int read_len = 0;
    int total = 0;
    char tmp[512] = {0};

    char buf[4096] = {0}; //  4k

    while ((read_len = recv(client_sock, tmp, sizeof tmp, 0)) > 0)
    {

        if (read_len + total < sizeof buf)
        {
            memcpy(buf + total, tmp, read_len);
        }
        total += read_len;

        buf[total] = '\0';
    }

    if (read_len == -1)
    {

        parseRequeseLine(client_sock, buf);
        return 0;
    }

    if (read_len == 0)
    {

        epoll_ctl(epoll, EPOLL_CTL_DEL, client_sock, NULL);
        close(client_sock);
        return -1;
    }
    return 0;
}

int parseRequeseLine(int client_sock, const char *line)
{   

    char method[14];
    char path[1024];
    sscanf(line, "%[^ ] %[^ ]", method, path);
    int len = strlen(path);
    if(path[len-1] == '/'){
        path[len - 1] = '\0';
    }
    printf("\n-----------------------------------------------------\nmethod = %s, path = %s\n", method, path);
    int ret = isFileOrDir(path + 1);
    printf("\n\n");
    if (ret == 0)
    { // 0不存在 1是目录 2是文件
        printf("404\n");
        sendHttpResponseLineAndHead(client_sock, 404, getContentTypeByFileSuffix(".html"));
        sendFile(client_sock, "404.html");
        printf("发完了");
    }
    else if (ret == 1)
    {
        printf("发目录\n");
        sendHttpResponseLineAndHead(client_sock, 200, getContentTypeByFileSuffix(".html"));
        sendDir(client_sock, path + 1);
        printf("发完了");
    }
    else
    {
        printf("发文件\n");
        char* suffix;
        suffix = strrchr(path+1,'.'); //换回指向.的指针
        sendHttpResponseLineAndHead(client_sock, 200, getContentTypeByFileSuffix(suffix));
        sendFile(client_sock, path + 1);
        printf("发完了");
    }

    // http://192.168.42.141:8080/path/404.html
    // http://192.168.42.141:8080/path
    // http://192.168.42.141:8080/404.html
    // http://192.168.42.141:8080/null.html
    return 0;
}
/*relative_path可能是
存在文件路径
存在目录路径
不存在文件路径
不存在目录路径
4种情况
*/
int isFileOrDir(const char *relative_path)
{

    // 打开目录 不存在 或者是文件 则返回NULL
    DIR *dir = opendir(relative_path);
    if (dir == NULL)
    {
        int dir_or_file;
        // 尝试打开这个文件  存在返回0 不存在返回-1
        if ((dir_or_file = open(relative_path, O_RDONLY)) == -1)
        {
            printf("file not exit");
            close(dir_or_file);
            closedir(dir);
            return 0;
        }
        else
        {
            printf("file exit");
            close(dir_or_file);
            closedir(dir);
            return 2;
        }
    }
    // 目录存在
    printf("dir exit");
    closedir(dir);
    return 1;
}

int sendHttpResponseLineAndHead(int client_sock, int state_num, const char *content_type)
{

    char buf[4096];

    sprintf(buf, "HTTP/1.1 %d %s\r\nContent-Type: %s;\r\nContent-Length: -1\r\n\r\n\r\n", state_num, getReasonPhraseByStateNum(state_num), content_type);
    // printf("\nsendHttpResponseLineAndHead:\n%s\n",buf);
    send(client_sock, buf, strlen(buf), 0);
    usleep(1000);
    return 0;
}

int sendFile(int client_sock, const char *file_name)
{
    FILE *fp;
    char buf[1024];
    size_t nread;
    if ((fp = fopen(file_name, "rb")) == NULL) {
        perror("打开文件失败");
        return -1;
    }
    while ((nread = fread(buf, 1, sizeof(buf), fp)) > 0) {
        printf("\n%zu",nread);
        usleep(50);
        if (send(client_sock, buf, nread, 0) == -1) {
            perror("发送数据失败");
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);
    return 0;
}

//判断一个已经存在的目录是否为空
int isDirEmpty(const char *dir)
{

    struct dirent *ent;
    int count = 0;
    DIR *test = opendir(dir);
    while ((ent = readdir(test)) != NULL)
    {
        if (ent->d_type == DT_REG || ent->d_type == DT_DIR)
        { // 判断是否为普通文件
            count++;
        }
    }
    closedir(test);
    //printf("\n%d\n",count);
    if (count == 2)
    {
        printf("目录为空。\n");
        return 1;
    }
    else
    {
        printf("目录不为空。\n");
        return 0;
    }
}

int sendDir(int client_sock, const char *dir)
{
    DIR *t;
    struct dirent *ent;
    printf("%s\n", dir);
    t = opendir(dir);
    if(isDirEmpty(dir)){
        send(client_sock,"<p>is empty</p>",16,0);
        return 0;
    }
    char buf[1024] = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>目录</title></head><body>";
    send(client_sock, buf, strlen(buf), 0);
    while ((ent = readdir(t)) != NULL)
    {
        if (strcmp(ent->d_name,".") != 0 && strcmp(ent->d_name,"..") != 0) // 判断是否为普通文件 不包含.. . 这两个是  DT_DIR
        {
            sprintf(buf, "<a href=\"/%s/%s\">%s</a></br>", dir, ent->d_name, ent->d_name );
            printf("%s\n", buf);
            send(client_sock, buf, strlen(buf), 0);
        }
    }
    send(client_sock, "</body></html>", 16, 0);
    free(ent);
    return 0;
}

const char *getReasonPhraseByStateNum(int state_num){

    if (state_num == 404)
        return "Not Found";
    else if (state_num == 200)
        return "OK";
    else if (state_num == 500)
        return "Internal Server Error";
    else if (state_num == 301)
        return "Moved Permanently";
    else if (state_num == 302)
        return "Found";
    else if (state_num == 307)
        return "Temporary Redirect";
    else if (state_num == 401)
        return "Unauthorized";
    else if (state_num == 403)
        return "Forbidden";
    else if (state_num == 204)
        return "No Content";
    else if (state_num == 206)
        return "Partial Content";
    else if (state_num == 304)
        return "Not Modified";
    else if (state_num == 416)
        return "Range Not Satisfiable";
    else if (state_num == 505)
        return "HTTP Version Not Supported";
    return "Not Found";
}

const char *getContentTypeByFileSuffix(const char *file_suffix)
{
    if (strcmp(file_suffix,".html") == 0)
    {
        return "text/html";
    }
    else if (strcmp(file_suffix,".jpeg") == 0 || strcmp(file_suffix,".jpg") == 0)
    {
        return "image/jpeg";
    }else if (strcmp(file_suffix,".gif") == 0)
    {
        return "image/gif";
    }else if (strcmp(file_suffix,".jpe") == 0)
    {
        return "image/jpeg";
    }else if (strcmp(file_suffix,".java") == 0)
    {
        return "java/*";
    }else if (strcmp(file_suffix,".mp4") == 0)
    {
        return "video/mpeg4";
    }else if (strcmp(file_suffix,".png") == 0)
    {
        return "application/x-png";
    }else if (strcmp(file_suffix,".c") == 0)
    {
        return "text/html";
    }else{
        return "text/html";
    }
}