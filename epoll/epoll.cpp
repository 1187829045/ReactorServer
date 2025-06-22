//
// Created by llb on 2025/6/22.
//

#include "epoll.h"
Epoll::Epoll()
{
    if ((epollfd_=epoll_create(1))==-1)       // 创建epoll句柄（红黑树）。
    {
        printf("epoll_create() failed(%d).\n",errno); exit(-1);
    }
}

Epoll::~Epoll()
{
    close(epollfd_);           // 在析构函数中关闭epollfd_。
}

// 把fd和它需要监视的事件添加到红黑树上。
void Epoll::addfd(int fd, uint32_t op)
{
    epoll_event ev;      // 声明事件的数据结构。
    ev.data.fd=fd;       // 指定事件的自定义数据，会随着epoll_wait()返回的事件一并返回。
    ev.events=op;       // 让epoll监视fd的。

    if (epoll_ctl(epollfd_,EPOLL_CTL_ADD,fd,&ev)==-1)     // 把需要监视的fd和它的事件加入epollfd中。
    {
        printf("epoll_ctl() failed(%d).\n",errno);
        exit(-1);
    }
}

// 运行epoll_wait()，等待事件的发生，已发生的事件用vector容器返回。
std::vector<epoll_event> Epoll::loop(int timeout)
{
    std::vector<epoll_event> evs;        // 存放epoll_wait()返回的事件。

    bzero(events_,sizeof(events_));
    int infds=epoll_wait(epollfd_,events_,MaxEvents,timeout);       // 等待监视的fd有事件发生。

    // 返回失败。
    if (infds < 0)
    {
        perror("epoll_wait() failed"); exit(-1);
    }

    // 超时。
    if (infds == 0)
    {
        printf("epoll_wait() timeout.\n"); return evs;
    }

    // 如果infds>0，表示有事件发生的fd的数量。
    for (int ii=0;ii<infds;ii++)       // 遍历epoll返回的数组events_。
    {
        evs.push_back(events_[ii]);
    }

    return evs;
}