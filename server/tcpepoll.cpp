//
// Created by llb on 2025/6/19.
//

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>  // TCP_NODELAY需要包含这个头文件。
#include"../inet_address/inet_address.h"
#include "../socket/socket.h"
int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "argc != 3" << std::endl;
        return -1;
    }

    Socket servsock(createnonblocking());
    InetAddress servaddr(argv[1],atoi(argv[2]));             // 服务端的地址和协议。
    servsock.setreuseaddr(true);
    servsock.settcpnodelay(true);
    servsock.setreuseport(true);
    servsock.setkeepalive(true);
    servsock.bind(servaddr);
    servsock.listen();

    // 创建 epoll 实例
    int epollfd = epoll_create(1);
    epoll_event ev;
    ev.data.fd = servsock.fd();
    // 默认水平触发（Level Trigger，LT）模式
    ev.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, servsock.fd(), &ev);
    epoll_event evs[10];  // 就绪事件数组

    // 事件循环
    while (true) {
        int infds = epoll_wait(epollfd, evs, 10, -1);

        // 返回失败
        if (infds < 0) {
            perror("epoll_wait() failed");
            break;
        }

        // 超时
        if (infds == 0) {
            std::cerr << "epoll_wait() timeout" << std::endl;
            continue;
        }

        // 遍历所有发生的事件
        for (int ii = 0; ii < infds; ii++) {
            if (evs[ii].events & EPOLLRDHUP) {
                printf("1client(eventfd=%d) disconnected.\n", evs[ii].data.fd);
                close(evs[ii].data.fd);  // 关闭这个客户端的连接
            } else if (evs[ii].events & (EPOLLIN | EPOLLPRI)) {
                // 如果是listenfd有事件，表示有新的客户端连上来。
                if (evs[ii].data.fd == servsock.fd()) {
                    InetAddress clientaddr;
                    Socket *clientsock = new Socket(servsock.accept(clientaddr));

                    printf("accept client(fd=%d, ip=%s, port=%d) ok.\n",
                                               clientsock->fd(), clientaddr.ip(), clientaddr.port());
                    // 为新客户端连接准备读事件，并添加到epoll中。
                    ev.data.fd = clientsock->fd();
                    ev.events = EPOLLIN | EPOLLET;  // 边缘触发
                    epoll_ctl(epollfd, EPOLL_CTL_ADD, clientsock->fd(), &ev);
                }

                char buffer[1024];  // 接收缓冲区
                while (true) {
                    bzero(&buffer, sizeof(buffer));
                    ssize_t nread = read(evs[ii].data.fd, buffer, sizeof(buffer));

                    if (nread > 0) {
                        printf("recv(eventfd=%d): %s\n", evs[ii].data.fd, buffer);
                        send(evs[ii].data.fd, buffer, strlen(buffer), 0);  // 回显
                    } else if (nread == -1 && errno == EINTR) {
                        continue;  // 信号中断，继续读取
                    } else if (nread == -1 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
                        break;  // 当前已无数据
                    } else if (nread == 0) {
                        printf("client(eventfd=%d) disconnected.\n", evs[ii].data.fd);
                        close(evs[ii].data.fd);
                        break;
                    }
                }
            } else if (evs[ii].events & EPOLLOUT) {
                // 这里可以放写操作，暂未实现
            } else {//其他类型
                printf("client(eventfd=%d) error.\n", evs[ii].data.fd);
                close(evs[ii].data.fd);  // 异常处理
            }
        }
    }

    return 0;
}
