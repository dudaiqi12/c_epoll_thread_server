#pragma once 
#include <dirent.h>
int initListenFd(int port);

int acceptClient(int lfd,int epfd);

int EpollRun(int server_sock);


int echo(int ep_event_data_fd, int epoll);


int recvHttpRequest(int client_sock, int epoll);

int parseRequeseLine(int client_sock,const char* line);

int isFileOrDir(const char* relative_path);


int sendHttpResponseLineAndHead(int client_sock, int state_num, const char* content_type);

int sendFile(int client_sock, const char* file_name);

int sendDir(int client_sock,const char * dir);

const char* getReasonPhraseByStateNum(int state_num);

const char* getContentTypeByFileSuffix(const char* file_suffix);

