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
#include <sys/epoll.h>
#include"../inet_address/inet_address.h"
#include "../socket/socket.h"
#include"../epoll/epoll.h"


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

    Epoll ep;
    // ep.addfd(servsock.fd(),EPOLLIN);    // 让epoll监视listenfd的读事件，采用水平触发。
    Channel *servchannel=new Channel(&ep,servsock.fd(),true);       // 这里new出来的对象没有释放，这个问题以后再解决。
    servchannel->enablereading();       // 让epoll_wait()监视servchannel的读事件。
    // 事件循环
    while (true) {
       std::vector<Channel *> channels=ep.loop();         // 等待监视的fd有事件发生。
        // 遍历所有发生的事件
        for (auto &ch:channels) {
            ch->handleevent(&servsock);
        }
    }

    return 0;
}
