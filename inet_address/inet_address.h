//
// Created by 11878 on 25-6-21.
//

#ifndef INET_ADDRESS_H
#define INET_ADDRESS_H
#include<arpa/inet.h>
#include<netinet/in.h>
#include<string>


class InetAddress {
public:
 InetAddress();
 InetAddress(const std::string &ip,uint16_t  port);
 InetAddress(const sockaddr_in addr);
 ~InetAddress();

 const char*ip()const;
 uint16_t port()const;
 const sockaddr *addr()const;
 void setaddr(sockaddr_in clientaddr);   // 设置addr_成员的值。
private:
 sockaddr_in addr_;
};



#endif //INET_ADDRESS_H
