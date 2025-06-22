//
// Created by llb on 2025/6/22.
//

#include "socket.h"

// 创建一个非阻塞的socket。
int createnonblocking()
{
    // 创建服务端用于监听的listenfd。
    int listenfd = socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK,IPPROTO_TCP);
    if (listenfd < 0)
    {
        printf("%s:%s:%d listen socket create error:%d\n", __FILE__, __FUNCTION__, __LINE__, errno); exit(-1);
    }
    return listenfd;
}

 // 构造函数，传入一个已准备好的fd。
Socket::Socket(int fd):fd_(fd)
{

}

// 在析构函数中，将关闭fd_。
Socket::~Socket()
{
    ::close(fd_);
}

int Socket::fd() const                              // 返回fd_成员。
{
    return fd_;
}

void Socket::settcpnodelay(bool on)
{
    // TCP_NODELAY包含头文件 <netinet/tcp.h>
      // 必须的。TCP_NODELAY：关闭 Nagle 算法，含义：禁用 Nagle 算法，数据会立即发送。
    int optval = on ? 1 : 0;
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setreuseaddr(bool on)
{
      // 必须的。SO_REUSEADDR：允许地址重用
    int optval = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setreuseport(bool on)
{
    // 多个进程或线程可以同时绑定、监听同一个端口。在 Reactor 模式下（单线程+多事件循环）意义不大。
    int optval = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setkeepalive(bool on)
{
     // 可能有用，但是建议自己做心跳。启用 TCP 保活机制
    int optval = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

void Socket::bind(const InetAddress& servaddr)
{
    if (::bind(fd_,servaddr.addr(),sizeof(sockaddr)) < 0 )
    {
        perror("bind() failed");
        close(fd_);
        exit(-1);
    }
}

void Socket::listen(int nn)
{
    if (::listen(fd_,nn) != 0 )        // 在高并发的网络服务器中，第二个参数要大一些。
    {
        perror("listen() failed");
        close(fd_);
        exit(-1);
    }
}

int Socket::accept(InetAddress& clientaddr)
{
    sockaddr_in peeraddr;
    socklen_t len = sizeof(peeraddr);
    int clientfd = accept4(fd_,(sockaddr*)&peeraddr,&len,SOCK_NONBLOCK);
    clientaddr.setaddr(peeraddr);             // 客户端的地址和协议。
    return clientfd;
}