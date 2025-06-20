//
// Created by llb on 2025/6/19.
//

#include<iostream>
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
#include <netinet/tcp.h>      // TCP_NODELAY需要包含这个头文件。

// 设置非阻塞的IO。
void setnonblocking(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

int main(int argc,char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "argc != 3"<<std::endl;
        return -1;
    }

    // 创建服务端用于监听的listenfd。
    //AF_INET	使用 IPv4 协议（地址族）
    //SOCK_STREAM	表示使用面向连接的 TCP 协议（流式套接字）
    //IPPROTO_TCP	明确指定使用 TCP 协议（一般也可以写成 0，让系统自动选择）

    int listenfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (listenfd < 0)
    {
        perror("socket() failed");
        return -1;
    }


    int opt = 1;
    // 必须的。 SO_REUSEADDR：允许地址重用
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&opt,static_cast<socklen_t>(sizeof opt));
    // 必须的。TCP_NODELAY：关闭 Nagle 算法，含义：禁用 Nagle 算法，数据会立即发送。
    setsockopt(listenfd,SOL_SOCKET,TCP_NODELAY   ,&opt,static_cast<socklen_t>(sizeof opt));
    //多个进程或线程可以同时绑定、监听同一个端口。在 Reactor 模式下（单线程+多事件循环）意义不大。
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEPORT ,&opt,static_cast<socklen_t>(sizeof opt));
    // 可能有用，但是，建议自己做心跳。启用 TCP 保活机制
    setsockopt(listenfd,SOL_SOCKET,SO_KEEPALIVE   ,&opt,static_cast<socklen_t>(sizeof opt));

    // 把服务端的listenfd设置为非阻塞的。
    setnonblocking(listenfd);

     // 服务端地址的结构体
    sockaddr_in servaddr;
     // IPv4网络协议的套接字类型。
    servaddr.sin_family = AF_INET;
    // 服务端用于监听的ip地址。
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);
     // 服务端用于监听的端口。
    servaddr.sin_port = htons(atoi(argv[2]));
    std::cout<<"ip:"<<servaddr.sin_addr.s_addr<<"port:"<<servaddr.sin_port<<std::endl;
    //将创建好的 socket（listenfd）绑定到指定的本地 IP 地址和端口上，如果绑定失败，返回负值 < 0，表示出错。
    if (bind(listenfd,(sockaddr *)&servaddr,sizeof(servaddr)) < 0 )
    {
        std::cerr<<"bind() failed"<<std::endl;
        close(listenfd);
        return -1;
    }
    //开始监听某个端口上的客户端连接请求，并设置最大连接队列长度为 128。如果监听失败，返回的值不等于 0（通常返回 -1），表示出错。
    if (listen(listenfd,128) != 0 )
    {
        std::cerr<<"listen() failed"<<std::endl;
        close(listenfd);
        return -1;
    }

    //创建一个 epoll 实例，返回一个文件描述符 epollfd。参数 1 在现代 Linux 中已经没意义了（旧版本用于指定最大监听数），通常写成大于 0 的任意值即可。
    int epollfd=epoll_create(1);
    //声明一个 epoll 事件结构体 ev，用来描述你想监听的事件，比如“读事件”“写事件”。
     epoll_event ev;
    //告诉 epoll：“我想监听的目标是 listenfd 这个 socket（监听套接字）”。
    ev.data.fd=listenfd;
   //这是默认的 “水平触发（Level Trigger，LT）” 模式
    ev.events=EPOLLIN;
   //把 listenfd 加入到 epollfd 的监听队列中，并设置它的监听事件为 ev。
    epoll_ctl(epollfd,EPOLL_CTL_ADD,listenfd,&ev);
   // 创建一个 epoll_event 数组，容量为 10，用来保存 epoll_wait() 返回的就绪事件。
    epoll_event evs[10];

      // 事件循环。
    while (true)
    {
        int infds=epoll_wait(epollfd,evs,10,-1);

        // 返回失败。
        if (infds < 0)
        {
            perror("epoll_wait() failed"); break;
        }

        // 超时。
        if (infds == 0)
        {
            std::cerr<<"epoll_wait() timeout"<<std::endl;
            continue;
        }

        // 如果infds>0，表示有事件发生的fd的数量。
        for (int ii=0;ii<infds;ii++)
        {
             // 如果是listenfd有事件，表示有新的客户端连上来。
            if (evs[ii].data.fd==listenfd)
            {
                // 定义一个结构体变量，用来存储客户端的地址信息（IP地址和端口号）
                sockaddr_in clientaddr;

                // 定义一个变量，存储客户端地址结构体的大小，accept调用时需要传入该长度
                socklen_t len = sizeof(clientaddr);

                // 从监听套接字 listenfd 中接受一个新的客户端连接，
                // 并将客户端地址信息填充到 clientaddr 中，len 是地址结构长度的指针
                int clientfd = accept(listenfd, (struct sockaddr*)&clientaddr, &len);

                // 将新接收的客户端连接 socket 设置为非阻塞模式，
                // 这样后续读写操作不会阻塞，适合与 epoll 等异步IO一起使用
                setnonblocking(clientfd);


                printf ("accept client(fd=%d,ip=%s,port=%d) ok.\n",clientfd,inet_ntoa(clientaddr.sin_addr),ntohs(clientaddr.sin_port));

                // 为新客户端连接准备读事件，并添加到epoll中。
                ev.data.fd=clientfd;
                ev.events=EPOLLIN|EPOLLET;           // 边缘触发。
                //把客户端的 clientfd 加入到 epoll 实例 epollfd 的监听事件中，并指定要监听的事件类型（由 ev 定义）。
                epoll_ctl(epollfd,EPOLL_CTL_ADD,clientfd,&ev);
            }
            else  // 如果不是 listenfd（监听套接字），就是已连接客户端的 socket 有事件发生
            {
                // 如果事件中包含 EPOLLRDHUP，说明对方已经关闭连接（半关闭或全关闭）
                if (evs[ii].events & EPOLLRDHUP)
                {
                    printf("1client(eventfd=%d) disconnected.\n", evs[ii].data.fd);
                    close(evs[ii].data.fd); // 关闭这个客户端的连接
                }
                // 如果事件中包含 EPOLLIN（普通数据可读）或 EPOLLPRI（带外数据可读）
                else if (evs[ii].events & (EPOLLIN | EPOLLPRI))
                {
                    char buffer[1024];  // 定义接收缓冲区
                    while (true)  // 循环读取，直到没有更多数据（非阻塞模式 + 边缘触发必须这样做）
                    {
                        // 将 buffer 清零，防止残留数据干扰输出
                        bzero(&buffer, sizeof(buffer));

                        // 从对应 fd 中读取数据，nread 表示读取到的字节数
                        ssize_t nread = read(evs[ii].data.fd, buffer, sizeof(buffer));

                        if (nread > 0)  // 成功读取到数据
                        {
                            // 打印接收到的内容
                            printf("recv(eventfd=%d):%s\n", evs[ii].data.fd, buffer);

                            // 将接收到的数据原样发回给客户端（回显服务器）
                            send(evs[ii].data.fd, buffer, strlen(buffer), 0);
                        }
                        // 如果 read 被信号中断（如 Ctrl+C 触发中断信号），继续读取
                        else if (nread == -1 && errno == EINTR)
                        {
                            continue;
                        }
                        // 如果读取不到数据且返回 EAGAIN 或 EWOULDBLOCK，说明当前已无数据（非阻塞特有现象）
                        else if (nread == -1 && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
                        {
                            break; // 跳出循环，后面没有数据了
                        }
                        // 如果 read 返回 0，说明客户端已经关闭连接
                        else if (nread == 0)
                        {
                            printf("2client(eventfd=%d) disconnected.\n", evs[ii].data.fd);
                            close(evs[ii].data.fd); // 关闭客户端连接
                            break; // 跳出读取循环
                        }
                    }
                }
                // 如果事件是可写（EPOLLOUT），表示有数据可以写，暂时没有处理逻辑
                else if (evs[ii].events & EPOLLOUT)
                {
                    // 这里可以放写操作（例如发送数据给客户端），暂时没有实现
                }
                // 如果都不是以上情况，说明是未知或错误事件，统一按异常处理
                else
                {
                    printf("3client(eventfd=%d) error.\n", evs[ii].data.fd);
                    close(evs[ii].data.fd); // 关闭客户端连接，防止资源泄漏
                }
            }
        }
    }

  return 0;
}