#include "iomanager.h"
#include "log.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <stack>
#include <cstring>
#include <chrono>
#include <thread>

static int s_iSockListenId = -1;

void testAccept();

void watchIoRead(){
    focus::IOManager::GetThis()->addEvent(s_iSockListenId, focus::IOManager::READ, testAccept);
}

void testAccept(){
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t len = sizeof(addr);
    int fd = accept(s_iSockListenId, (struct sockaddr*)&addr, &len);
    if(fd < 0) {

    }else {
        fcntl(fd, F_SETFL, O_NONBLOCK);
        focus::IOManager::GetThis()->addEvent(fd, focus::IOManager::READ, [fd]() {
            char buffer[1024];
            memset(buffer, 0, sizeof(buffer));
            while(true) {
                int ret = recv(fd, buffer, sizeof(buffer), 0);
                if (ret > 0)
                {
                    // 打印接收到的数据
                    //std::cout << "received data, fd = " << fd << ", data = " << buffer << std::endl;
                    
                    // 构建HTTP响应
                    const char *response = "HTTP/1.1 200 OK\r\n"
                                           "Content-Type: text/plain\r\n"
                                           "Content-Length: 13\r\n"
                                           "Connection: keep-alive\r\n"
                                           "\r\n"
                                           "Hello, World!";
                    
                    // 发送HTTP响应
                    ret = send(fd, response, strlen(response), 0);
                   // std::cout << "sent data, fd = " << fd << ", ret = " << ret << std::endl;

                    // 关闭连接
                    close(fd);
                    break;
                }
                if (ret <= 0)
                {
                    if (ret == 0 || errno != EAGAIN)
                    {
                        //std::cout << "closing connection, fd = " << fd << std::endl;
                        close(fd);
                        break;
                    }
                    else if (errno == EAGAIN)
                    {
                        //std::cout << "recv returned EAGAIN, fd = " << fd << std::endl;
                        //std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 延长睡眠时间，避免繁忙等待
                    }
                }
            }
        });
    }
    focus::IOManager::GetThis()->addEvent(s_iSockListenId, focus::IOManager::READ, testAccept);
}

void testIoManager() {
    int iPortNo = 8080;
    struct sockaddr_in ServerAddr, ClientAddr;
    socklen_t ClientLen = sizeof(ClientAddr);

    // 设置套接字
    s_iSockListenId = socket(AF_INET, SOCK_STREAM, 0);
    if(s_iSockListenId < 0) {
        FOCUS_LOG_ERROR(FOCUS_LOG_ROOT()) << "Creat Socker Error";
    }

    int yes = 1;
    // 解决 "address already in use"
    setsockopt(s_iSockListenId, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    memset((char*)&ServerAddr, 0, sizeof(ServerAddr));
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(iPortNo);
    ServerAddr.sin_addr.s_addr = INADDR_ANY;

    // 绑定套接字并监听
    if(bind(s_iSockListenId, (struct sockaddr*)&ServerAddr, sizeof(ServerAddr)) < 0) {
        FOCUS_LOG_ERROR(FOCUS_LOG_ROOT()) << "Bind Error";
    }

    if(listen(s_iSockListenId, 1024) < 0) {
        FOCUS_LOG_ERROR(FOCUS_LOG_ROOT()) << "Listen Error";
    }

    printf("Echo Server Listening On Port: %d\n", iPortNo);
    fcntl(s_iSockListenId, F_SETFL, O_NONBLOCK);
    focus::IOManager iom(8);
    iom.addEvent(s_iSockListenId, focus::IOManager::READ, testAccept);
}

int main(int argc, char* argv[]) {
    testIoManager();
    return 0;   
}