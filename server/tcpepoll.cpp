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
int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "argc != 3" << std::endl;
        return -1;
    }

    // 创建服务端用于监听的listenfd。
    // AF_INET 使用 IPv4 协议（地址族）
    // SOCK_STREAM 表示使用面向连接的 TCP 协议（流式套接字），_NONBLOCK设置为非阻塞
    // IPPROTO_TCP 明确指定使用 TCP 协议（一般也可以写成 0，让系统自动选择）
    int listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (listenfd < 0) {
        perror("socket() failed");
        return -1;
    }

    int opt = 1;
    // 必须的。SO_REUSEADDR：允许地址重用
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, static_cast<socklen_t>(sizeof opt));
    // 必须的。TCP_NODELAY：关闭 Nagle 算法，含义：禁用 Nagle 算法，数据会立即发送。
    setsockopt(listenfd, SOL_SOCKET, TCP_NODELAY, &opt, static_cast<socklen_t>(sizeof opt));
    // 多个进程或线程可以同时绑定、监听同一个端口。在 Reactor 模式下（单线程+多事件循环）意义不大。
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &opt, static_cast<socklen_t>(sizeof opt));
    // 可能有用，但是建议自己做心跳。启用 TCP 保活机制
    setsockopt(listenfd, SOL_SOCKET, SO_KEEPALIVE, &opt, static_cast<socklen_t>(sizeof opt));

    InetAddress servaddr(argv[1],atoi(argv[2]));
    //std::cout << "ip:" << servaddr.sin_addr.s_addr << " port:" << servaddr.sin_port << std::endl;

    if (bind(listenfd,servaddr.addr(),sizeof(sockaddr)) < 0 )
    {
        perror("bind() failed"); close(listenfd); return -1;
    }

    // 开始监听连接请求
    if (listen(listenfd, 128) != 0) {
        std::cerr << "listen() failed" << std::endl;
        close(listenfd);
        return -1;
    }

    // 创建 epoll 实例
    int epollfd = epoll_create(1);
    epoll_event ev;
    ev.data.fd = listenfd;
    // 默认水平触发（Level Trigger，LT）模式
    ev.events = EPOLLIN;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev);
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
                if (evs[ii].data.fd == listenfd) {
                    sockaddr_in peeraddr;
                    socklen_t len = sizeof(peeraddr);
                    int clientfd = accept4(listenfd, (struct sockaddr *)&peeraddr, &len, SOCK_NONBLOCK);

                    InetAddress clientaddr(peeraddr);

                    printf("accept client(fd=%d, ip=%s, port=%d) ok.\n",
                                               clientfd, clientaddr.ip(), clientaddr.port());
                    // 为新客户端连接准备读事件，并添加到epoll中。
                    ev.data.fd = clientfd;
                    ev.events = EPOLLIN | EPOLLET;  // 边缘触发
                    epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev);
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
