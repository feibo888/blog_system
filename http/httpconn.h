//
// Created by fb on 12/27/24.
//

#ifndef HTTPCONN_H
#define HTTPCONN_H


#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>

#include "../log/log.h"
#include "../pool/sqlconnRAll.h"
#include "../buffer/buffer.h"
#include "../blog/blogmanager.h"
#include "httprequest.h"
#include "httpresponse.h"


class HttpConn
{
public:
    HttpConn();

    ~HttpConn();

    void init(int fd, const sockaddr_in &addr);

    ssize_t read(int* saveErrno);

    ssize_t write(int* saveErrno);

    void Close();

    int GetFd() const;

    int GetPort() const;

    const char* GetIP() const;

    sockaddr_in GetAddr() const;

    bool process();

    int ToWriteBytes()
    {
        return static_cast<int>(iov_[0].iov_len + iov_[1].iov_len);
    }

    bool IsKeepAlive() const
    {
        return request_.IsKeepAlive();
    }

    static bool isET;
    static const char* srcDir;
    static std::atomic<int> userCount;

private:
    int fd_;
    struct sockaddr_in addr_;

    bool isClose_;

    int iovCnt_;

    struct iovec iov_[2];

    Buffer readBuf_; //读缓冲区
    Buffer writeBuf_;   //写缓冲区

    HttpRequest request_;
    HttpResponse response_;




};



#endif //HTTPCONN_H
