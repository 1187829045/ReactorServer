#include "BankServer.h"

BankServer::BankServer(const std::string &ip,const uint16_t port,int subthreadnum,int workthreadnum)
                   :tcpserver_(ip,port,subthreadnum),threadpool_(workthreadnum,"WORKS")
{
    // 以下代码不是必须的，业务关心什么事件，就指定相应的回调函数。
    tcpserver_.setnewconnectioncb(std::bind(&BankServer::HandleNewConnection, this, std::placeholders::_1));
    tcpserver_.setcloseconnectioncb(std::bind(&BankServer::HandleClose, this, std::placeholders::_1));
    tcpserver_.seterrorconnectioncb(std::bind(&BankServer::HandleError, this, std::placeholders::_1));
    tcpserver_.setonmessagecb(std::bind(&BankServer::HandleMessage, this, std::placeholders::_1, std::placeholders::_2));
    tcpserver_.setremoveconnectioncb(std::bind(&BankServer::HandleRemove, this, std::placeholders::_1));
}

BankServer::~BankServer()
{

}

// 启动服务。
void BankServer::Start()                
{
    tcpserver_.start();
}

 // 停止服务。
 void BankServer::Stop()
 {
    // 停止工作线程。
    threadpool_.stop();
    printf("工作线程已停止。\n");

    // 停止IO线程（事件循环）。
    tcpserver_.stop();
 }

// 处理新客户端连接请求，在TcpServer类中回调此函数。
void BankServer::HandleNewConnection(spConnection conn)    
{
    // std::cout << "New Connection Come in." << std::endl;
    // printf("BankServer::HandleNewConnection() thread is %d.\n",syscall(SYS_gettid));
    // printf ("%s new connection(fd=%d,ip=%s,port=%d) ok.\n",Timestamp::now().tostring().c_str(),conn->fd(),conn->ip().c_str(),conn->port());

    // 根据业务的需求，在这里可以增加其它的代码。
    //usermap_[conn->fd()]=std::make_shared<UserInfo>;
    spUserInfo userinfo(new UserInfo(conn->fd(),conn->ip()));   
    usermap_[conn->fd()]=userinfo;
    printf ("%s 新建连接（ip=%s）.\n",Timestamp::now().tostring().c_str(),conn->ip().c_str());
}

// 关闭客户端的连接，在TcpServer类中回调此函数。 
void BankServer::HandleClose(spConnection conn)  
{
    // printf ("%s connection closed(fd=%d,ip=%s,port=%d) ok.\n",Timestamp::now().tostring().c_str(),conn->fd(),conn->ip().c_str(),conn->port());
    // std::cout << "BankServer conn closed." << std::endl;

    // 根据业务的需求，在这里可以增加其它的代码。
    printf ("%s 连接已断开（ip=%s）.\n",Timestamp::now().tostring().c_str(),conn->ip().c_str());
    usermap_.erase(conn->fd());
}

// 客户端的连接错误，在TcpServer类中回调此函数。
void BankServer::HandleError(spConnection conn)  
{
    // std::cout << "BankServer conn error." << std::endl;

    // 根据业务的需求，在这里可以增加其它的代码。
    HandleClose(conn);
}

// 处理客户端的请求报文，在TcpServer类中回调此函数。
void BankServer::HandleMessage(spConnection conn,std::string& message)     
{
    // printf("BankServer::HandleMessage() thread is %d.\n",syscall(SYS_gettid)); 

    if (threadpool_.size()==0)
    {
        // 如果没有工作线程，表示在IO线程中计算。
        OnMessage(conn,message);       
    }
    else
    {
        // 把业务添加到线程池的任务队列中，交给工作线程去处理业务。
        threadpool_.addtask(std::bind(&BankServer::OnMessage,this,conn,message));
    }
}

 // 处理客户端的请求报文，用于添加给线程池。
 void BankServer::OnMessage(spConnection conn,std::string& message)     
 {
    // printf("%s message (fd=%d):%s\n",Timestamp::now().tostring().c_str(),conn->fd(),message.c_str());

    spUserInfo userinfo=usermap_[conn->fd()];

    std::string bizcode;
    std::string replaymessage;
    getxmlbuffer(message,"bizcode",bizcode);
    if (bizcode=="00101")     // 登录业务。
    {
        std::string username,password;
        getxmlbuffer(message,"username",username);
        getxmlbuffer(message,"password",password);
        if ( (username=="wucz") && (password=="123465") )
        {
            replaymessage="<bizcode>00102</bizcode><retcode>0</retcode><message>ok</message>";
            userinfo->setLogin(true);               // 设置用户的登录状态为true。
        }
        else
            replaymessage="<bizcode>00102</bizcode><retcode>-1</retcode><message>用户名或密码不正确。</message>";
    }
    else if (bizcode=="00201")   // 查询余额业务。
    {
        if (userinfo->Login()==true)
        {
            // 把用户的余额从Redis中查询出来。
            replaymessage="<bizcode>00202</bizcode><retcode>0</retcode><message>5088.80</message>";
        }
        else
        {
            replaymessage="<bizcode>00202</bizcode><retcode>-1</retcode><message>用户未登录。</message>";
        }
    }
    else if (bizcode=="00901")   // 注销业务。
    {
        if (userinfo->Login()==true)
        {
            replaymessage="<bizcode>00902</bizcode><retcode>0</retcode><message>ok</message>";
            userinfo->setLogin(false);               // 设置用户的登录状态为false。
            tcpserver_.closeconnection(conn);
        }
        else
        {
            replaymessage="<bizcode>00902</bizcode><retcode>-1</retcode><message>用户未登录。</message>";
        }
    }
    else if (bizcode=="00001")   // 心跳。
    {
        if (userinfo->Login()==true)
        {
            replaymessage="<bizcode>00002</bizcode><retcode>0</retcode><message>ok</message>";
        }
        else
        {
            replaymessage="<bizcode>00002</bizcode><retcode>-1</retcode><message>用户未登录。</message>";
        }
    }
    
    conn->send(replaymessage.data(),replaymessage.size());   // 把数据发送出去。 
 }

 void BankServer::HandleRemove(int fd)                       // 客户端的连接超时，在TcpServer类中回调此函数。
 {
    usermap_.erase(fd);
 }

 bool getxmlbuffer(const std::string &xmlbuffer,const std::string &fieldname,std::string  &value,const int ilen)
{
    std::string start="<"+fieldname+">";            // 数据项开始的标签。
    std::string end="</"+fieldname+">";            // 数据项结束的标签。

    int startp=xmlbuffer.find(start);               // 在xml中查找数据项开始的标签的位置。
    if (startp==std::string::npos) return false;

    int endp=xmlbuffer.find(end);                 // 在xml中查找数据项结束的标签的位置。
    if (endp==std::string::npos) return false;

    // 从xml中截取数据项的内容。
    int itmplen=endp-startp-start.length();
    if ( (ilen>0) && (ilen<itmplen) ) itmplen=ilen;
    value=xmlbuffer.substr(startp+start.length(),itmplen);

    return true;
}

/*
bool getxmlbuffer(const std::string &xmlbuffer,const std::string &fieldname,char *value,const int len)
{
    if (value==nullptr) return false;

    if (len>0) memset(value,0,len+1);   // 调用者必须保证value的空间足够，否则这里会内存溢出。

    std::string str;
    getxmlbuffer(xmlbuffer,fieldname,str);

    if ( (str.length()<=(unsigned int)len) || (len==0) )
    {
        str.copy(value,str.length());
        value[str.length()]=0;    // std::string的copy函数不会给C风格字符串的结尾加0。
    }
    else
    {
        str.copy(value,len);
        value[len]=0;
    }

    return true;
}

bool getxmlbuffer(const std::string &xmlbuffer,const std::string &fieldname,bool &value)
{
    std::string str;
    if (getxmlbuffer(xmlbuffer,fieldname,str)==false) return false;

    if (str=="true") value=true; 
    else value=false;

    return true;
}

bool getxmlbuffer(const std::string &xmlbuffer,const std::string &fieldname,int &value)
{
    std::string str;

    if (getxmlbuffer(xmlbuffer,fieldname,str)==false) return false;

    try
    {
       value = stoi(str);  // stoi有异常，需要处理异常。
    }
    catch(const std::exception& e)
    {
        return false;
    }

    return true;
}

bool getxmlbuffer(const std::string &xmlbuffer,const std::string &fieldname,unsigned int &value)
{
    std::string str;

    if (getxmlbuffer(xmlbuffer,fieldname,str)==false) return false;

    try
    {
       value = stoi(str);  // stoi有异常，需要处理异常。
    }
    catch(const std::exception& e)
    {
        return false;
    }

    return true;
}

bool getxmlbuffer(const std::string &xmlbuffer,const std::string &fieldname,long &value)
{
    std::string str;

    if (getxmlbuffer(xmlbuffer,fieldname,str)==false) return false;

    try
    {
        value = stol(str);  // stol有异常，需要处理异常。
    }
    catch(const std::exception& e)
    {
        return false;
    }

    return true;
}

bool getxmlbuffer(const std::string &xmlbuffer,const std::string &fieldname,unsigned long &value)
{
    std::string str;

    if (getxmlbuffer(xmlbuffer,fieldname,str)==false) return false;

    try
    {
        value = stoul(str);  // stoul有异常，需要处理异常。
    }
    catch(const std::exception& e)
    {
        return false;
    }

    return true;
}

bool getxmlbuffer(const std::string &xmlbuffer,const std::string &fieldname,double &value)
{
    std::string str;

    if (getxmlbuffer(xmlbuffer,fieldname,str)==false) return false;

    try
    {
        value = stod(str);  // stod有异常，需要处理异常。
    }
    catch(const std::exception& e)
    {
        return false;
    }

    return true;
}

bool getxmlbuffer(const std::string &xmlbuffer,const std::string &fieldname,float &value)
{
    std::string str;

    if (getxmlbuffer(xmlbuffer,fieldname,str)==false) return false;

    try
    {
        value = stof(str);  // stof有异常，需要处理异常。
    }
    catch(const std::exception& e)
    {
        return false;
    }

    return true;
}

*/
