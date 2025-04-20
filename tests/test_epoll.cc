#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#define MAX_EVENTS 10
#define PORT 8888

int main() {
    int ListenFd, ConnFd, EpollFd, EventCount;
    struct sockaddr_in ServerAddr, ClientAddr;
    socklen_t AddrLen = sizeof(ClientAddr);
    struct epoll_event Events[MAX_EVENTS], Event;

    // 创建监听套接字
    if ((ListenFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    int yes = 1;
    // 解决 "address already in use" 错误
    setsockopt(ListenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    // 设置服务器地址和端口
    memset(&ServerAddr, 0, sizeof(ServerAddr));
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(PORT);
    ServerAddr.sin_addr.s_addr = INADDR_ANY;

    // 绑定监听套接字到服务器地址和端口
    if (bind(ListenFd, (struct sockaddr *)&ServerAddr, sizeof(ServerAddr)) == -1) {
        perror("bind");
        return -1;
    }

    // 监听连接
    if (listen(ListenFd, 1024) == -1) {
        perror("listen");
        return -1;
    }

    // 创建 epoll 实例
    if ((EpollFd = epoll_create1(0)) == -1) {
        perror("epoll_create1");
        return -1;
    }

    // 添加监听套接字到 epoll 实例中
    Event.events = EPOLLIN;
    Event.data.fd = ListenFd;
    if (epoll_ctl(EpollFd, EPOLL_CTL_ADD, ListenFd, &Event) == -1) {
        perror("epoll_ctl");
        return -1;
    }

    while (1) {
        // 等待事件发生
        EventCount = epoll_wait(EpollFd, Events, MAX_EVENTS, -1);
        if (EventCount == -1) {
            perror("epoll_wait");
            return -1;
        }

        // 处理事件
        for (int i = 0; i < EventCount; i++) {
            if (Events[i].data.fd == ListenFd) {
                // 有新连接到达
                ConnFd = accept(ListenFd, (struct sockaddr *)&ClientAddr, &AddrLen);
                if (ConnFd == -1) {
                    perror("accept");
                    continue;
                }

                // 将新连接的套接字添加到 epoll 实例中
                Event.events = EPOLLIN;
                Event.data.fd = ConnFd;
                if (epoll_ctl(EpollFd, EPOLL_CTL_ADD, ConnFd, &Event) == -1) {
                    perror("epoll_ctl");
                    return -1;
                }
            } else {
                // 有数据可读
                char buf[1024];
                int len = read(Events[i].data.fd, buf, sizeof(buf) - 1);
                if (len <= 0) {
                    // 发生错误或连接关闭，关闭连接
                    close(Events[i].data.fd);
                } else {
                    // 发送HTTP响应
                    const char *response = "HTTP/1.1 200 OK\r\n"
                                           "Content-Type: text/plain\r\n"
                                           "Content-Length: 1\r\n"
                                           "Connection: keep-alive\r\n"
                                           "\r\n"
                                           "1";
                    write(Events[i].data.fd, response, strlen(response));
                    epoll_ctl(EpollFd,EPOLL_CTL_DEL,Events[i].data.fd,NULL);//出现70007的错误再打开，或者试试-r命令
                    // 关闭连接
                    close(Events[i].data.fd);
                }
            }
        }
    }

    // 关闭监听套接字和 epoll 实例
    close(ListenFd);
    close(EpollFd);
    return 0;
}