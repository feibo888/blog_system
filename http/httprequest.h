//
// Created by fb on 12/27/24.
//

#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H


#include <unordered_map>
#include <unordered_set>
//#include <string>
//#include <regex>
#include <errno.h>
#include <mysql/mysql.h>  //mysql
#include <boost/regex.hpp>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAll.h"
#include "../blog/blogmanager.h"


class HttpRequest
{
public:
    enum PARSE_STATE
    {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };

    enum HTTP_CODE
    {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

    HttpRequest(){ Init();}
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;
    std::unordered_map<std::string, std::string> GetPost() const;
    std::unordered_map<std::string, std::string> GetHeader() const;

    bool IsKeepAlive() const;
    bool IsDynamic() const { return isDynamic_; }  



private:
    bool ParseRequestLine_(const std::string& line);
    void ParseHeader_(const std::string& line);
    void ParseBody_(const std::string& line);

    void ParsePath_();
    void ParsePost_();
    void ParseFromUrlencoded_();
    void parseQueryString(const std::string& query);

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);

    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch);
    bool isDynamic_ = false;  

    void ParseJson_();  // 添加 JSON 解析函数声明
    void ParseMultipartFormData_(const std::string& body);

};



#endif //HTTPREQUEST_H
