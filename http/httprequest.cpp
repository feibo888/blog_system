//
// Created by fb on 12/27/24.
//

#include "httprequest.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <sstream>

using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML
{
    "/index", "/register", "/login",
     "/welcome", "/video", "/picture",
};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG
{
    {"/register.html", 0}, {"/login.html", 1},
};

void HttpRequest::Init()
{
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    isDynamic_ = false;
    header_.clear();
    post_.clear();
}

bool HttpRequest::parse(Buffer& buff)
{
    LOG_DEBUG("Starting parse");

    const char CRLF[] = "\r\n";
    if (buff.ReadableBytes() <= 0)
    {
        return false;
    }
    while (buff.ReadableBytes() && state_ != FINISH)
    {
        switch (state_)
        {
            // 处理请求行（按行解析）
            case REQUEST_LINE: 
            {
                const char* lineEnd = std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
                std::string line(buff.Peek(), lineEnd);
                if (!ParseRequestLine_(line)) 
                {
                    LOG_ERROR("RequestLine parse error");
                    return false;
                }
                ParsePath_();
                buff.RetrieveUntil(lineEnd + 2);  // 移动读指针
                state_ = HEADERS;  // 进入头部解析
                break;
            }
            // 处理头部（按行解析，直到空行触发BODY状态）
            case HEADERS: 
            {
                const char* lineEnd = std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
                std::string line(buff.Peek(), lineEnd);

                // 检查是否是空行（表示头部结束）
                bool isEmptyLine = (line.empty());

                ParseHeader_(line);  // 该函数会在空行时将state_设为BODY

                if (isEmptyLine) 
                {
                    // 对于POST请求的特殊处理
                    if (method_ == "POST" || method_ == "PUT") 
                    {
                        // 无Content-Length或Content-Length为0的POST请求
                        if (header_.count("Content-Length") == 0 || header_["Content-Length"] == "0") 
                        {
                            LOG_DEBUG("POST request with empty body");
                            ParseBody_("");  // 处理空请求体
                            state_ = FINISH; // 直接设为完成状态
                        }
                        // 其他情况会自动进入BODY状态
                    } 
                    else 
                    {
                        // 非POST请求在遇到空行后直接完成
                        state_ = FINISH;
                    }
                }
                buff.RetrieveUntil(lineEnd + 2);
                break;
            }
            // 处理请求体（一次性读取所有内容）
            case BODY: 
            {
                if(method_ == "POST" || method_ == "PUT")
                {
                    auto it = header_.find("Content-Length");
                    if (it == header_.end()) 
                    {
                        
                        LOG_ERROR("No Content-Length in headers");
                        state_ = FINISH;
                        return false;
                    }
                    int content_length_;
                    // 解析Content-Length值
                    try 
                    {
                        content_length_ = std::stoi(it->second);                    
                    } 
                    catch (...) 
                    {
                        LOG_ERROR("Invalid Content-Length: %s", it->second.c_str());
                        state_ = FINISH;
                        return false;
                    }

                    // 检查缓冲区数据是否足够
                    if (buff.ReadableBytes() < static_cast<size_t>(content_length_))
                    {
                        return false;  // 数据不足，等待下次读取
                    }

                    // 一次性读取Body内容
                    std::string body(buff.Peek(), buff.Peek() + content_length_);
                    ParseBody_(body);
                    buff.Retrieve(content_length_);
                    state_ = FINISH;  // 标记解析完成
                    break;
                }
                else
                {
                    state_ = FINISH;
                    break;
                }
            }
            case FINISH:
                break;  // 无需操作
        }
    }

    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;

}

std::string HttpRequest::path() const
{
    return path_;
}

std::string& HttpRequest::path()
{
    return path_;
}

std::string HttpRequest::method() const
{
    return method_;
}

std::string HttpRequest::version() const
{
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const
{
    assert(key != "");
    if (post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const
{
    assert(key != "");
    if (post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}

std::unordered_map<std::string, std::string> HttpRequest::GetPost() const
{
    return post_;
}

std::unordered_map<std::string, std::string> HttpRequest::GetHeader() const
{
    return header_;
}

bool HttpRequest::IsKeepAlive() const
{
    if (header_.count("Connection") == 1)
    {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

bool HttpRequest::ParseRequestLine_(const std::string& line)
{
    LOG_DEBUG("Raw request line: |%s|", line.c_str()); // 调试日志

    boost::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$" );
    boost::smatch subMatch;
    if (boost::regex_match(line, subMatch, patten))
    {
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;

        LOG_DEBUG("%s, %s, %s", method_.c_str(), path_.c_str(), version_.c_str());
        return true;
    }

    LOG_ERROR("RequestLine Error");
    return false;
}

// 解析查询字符串（如 ?key=value&key2=value2）
void HttpRequest::parseQueryString(const std::string& query) {
    std::string key, value;
    int i = 0, j = 0, n = query.size();
    for (; i < n; ++i) {
        char ch = query[i];
        switch (ch) {
        case '=':
            key = query.substr(j, i - j);
            j = i + 1;
            break;
        case '&':
            value = query.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    if (j < n) {
        value = query.substr(j, n - j);
        post_[key] = value;
        LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
    }
}

void HttpRequest::ParseHeader_(const std::string& line)
{
    boost::regex patten("^([^:]*): ?(.*)$");
    boost::smatch subMatch;
    if (boost::regex_match(line, subMatch, patten))
    {
        header_[subMatch[1]] = subMatch[2];
    }
    else
    {
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const std::string& line)
{
    body_ = line;
    
    // 检查是否是文件上传请求
    if (header_["Content-Type"].find("multipart/form-data") != std::string::npos) {
        ParseMultipartFormData_(line);
    }
    else if (header_["Content-Type"].find("application/x-www-form-urlencoded") != std::string::npos) {
        ParsePost_();
    }
    else if (header_["Content-Type"].find("application/json") != std::string::npos) {
        ParseJson_();
    }
    
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());

    
}

void HttpRequest::ParsePath_()
{
    if (path_ == "/")
    {
        path_ = "index.html";
    }
    else if (path_ == "/login")
    {
        path_ = "/login.html";
    } 
    else if (path_ == "/register") 
    {
        path_ = "/register.html";
    }
    // 博客相关的动态路由
    else if (path_.rfind("/blogs/", 0) == 0)
    {
        isDynamic_ = true;
        // 保持原始路径，由响应处理器处理
    }
    else if (path_.rfind("/comments", 0) == 0)
    {
        isDynamic_ = true;
        // 保持原始路径，由响应处理器处理
    }
    else if (path_.rfind("/likes", 0) == 0)
    {
        isDynamic_ = true;
        // 保持原始路径，由响应处理器处理
    }
    // 用户相关的动态路由
    else if (path_.rfind("/user/", 0) == 0 || 
             path_.rfind("/login/", 0) == 0 || 
             path_.rfind("/register/", 0) == 0)
    {
        isDynamic_ = true;
        // 保持原始路径，由响应处理器处理
    }
    else
    {
        for (auto &item : DEFAULT_HTML)
        {
            if (item == path_)
            {
                path_ += ".html";
                break;
            }
        }
    }
}

void HttpRequest::ParsePost_()
{

    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") 
    {
        ParseFromUrlencoded_();

        if (DEFAULT_HTML_TAG.count(path_)) 
        {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if (tag == 0 || tag == 1) 
            {
                bool isLogin = (tag == 1);
                std::string login_or_register = isLogin ? "login" : "register";
                if (UserVerify(post_["username"], post_["password"], isLogin)) 
                {
                    // 修改为返回 JSON 响应，而不是 HTML
                    path_ = "/" + login_or_register + "/success";
                    
                }
                else 
                {
                    path_ = "/" + login_or_register + "/error";
                }
            }
            isDynamic_ = true; // 标记为动态请求
        } 
        else if (path_ == "/posts") 
        {
            // 创建文章
            if (BlogManager::CreateBlog(post_["title"], post_["content"], "admin")) 
            {
                path_ = "/posts";  // 重定向到列表（实际由响应处理）
            } 
            else 
            {
                path_ = "/posts/error";  // 错误页面（后续改为 JSON 错误）
            }
            isDynamic_ = true; // 标记为动态请求
        } 
        else if(path_.rfind("/likes/", 0) == 0 && method_ == "POST")
        {
            isDynamic_ = true; // 标记为动态请求
        }
        else if (path_.rfind("/user/", 0) == 0 && method_ == "PUT") 
        {
            string username = path_.substr(10); // 提取 username
            // 获取新密码，优先使用 "password" 字段，如果不存在则使用 "new-password"
            string newPassword = post_["password"];
            if (newPassword.empty()) 
            {
                newPassword = post_["new-password"];
            }
            if (BlogManager::UpdateUser(username, newPassword)) 
            {
                path_ = "/user/" + username;
            } 
            else 
            {
                path_ = "/user/" + username + "/error";
            }
            isDynamic_ = true; // 标记为动态请求
        }
    }
}

void HttpRequest::ParseFromUrlencoded_()
{
    if (body_.size() == 0)
    {
        return;
    }

    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for (; i < n; ++i)
    {
        char ch = body_[i];
        switch (ch)
        {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            if (i + 2 >= n) break;  // 防止越界
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i] = static_cast<char>(num);  // 替换为 ASCII 字符
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    // 处理最后一个键值对
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }

}

bool HttpRequest::UserVerify(const std::string& name, const std::string& pwd, bool isLogin)
{
    if (name == "" || pwd == "") return false;
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    //cout << name << " " << pwd << endl;

    // 从数据库连接池中获取一个MySQL连接
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    //SqlConnRAll(&sql,  SqlConnPool::Instance());
    //sql = SqlConnPool::Instance()->GetConn();
    assert(sql);

    bool flag = false;
    unsigned int j = 0;     // 字段数量
    char order[256] = {0};      // SQL语句缓冲区
    MYSQL_FIELD* fields = nullptr;      // 字段信息
    MYSQL_RES* res = nullptr;           // 查询结果集

    // 如果是注册操作，默认flag为true
    if (!isLogin)
    {
        flag = true;
    }

    //查询用户及密码
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    // 执行查询
    if (mysql_query(sql, order))
    {
        mysql_free_result(res);
        return false;
    }

    // 获取查询结果集
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while (MYSQL_ROW row = mysql_fetch_row(res))
    {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        //注册行为且用户名未被使用
        if (isLogin)
        {
            if (pwd == password)
            {
                flag = true;
            }
            else
            {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        }
        else
        {
            flag = false;
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    //注册行为且用户名未被使用
    if (!isLogin && flag == true)
    {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s', '%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if (mysql_query(sql, order))
        {
            LOG_DEBUG( "Insert error!");
            flag = false;
        }
        flag = true;
    }

    //SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;

}

int HttpRequest::ConverHex(char ch)
{
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}

// 新增 JSON 解析函数
void HttpRequest::ParseJson_() {
    try {
        boost::property_tree::ptree pt;
        std::stringstream ss(body_);
        boost::property_tree::read_json(ss, pt);

        // 遍历 JSON 对象的所有字段
        for (const auto& pair : pt) {
            if (pair.second.empty()) 
            {  // 如果是简单值
                post_[pair.first] = pair.second.data();
                LOG_DEBUG("JSON field: %s = %s", pair.first.c_str(), pair.second.data().c_str());
            } 
            else 
            {  // 如果是数组或对象，转换为字符串
                std::stringstream value_ss;
                boost::property_tree::write_json(value_ss, pair.second);
                post_[pair.first] = value_ss.str();
                LOG_DEBUG("JSON complex field: %s = %s", pair.first.c_str(), value_ss.str().c_str());
            }
        }
    } 
    catch (const std::exception& e) 
    {
        LOG_ERROR("JSON parse error: %s", e.what());
    }
}

void HttpRequest::ParseMultipartFormData_(const std::string& body) {
    // 获取boundary
    std::string boundary = header_["Content-Type"];
    size_t pos = boundary.find("boundary=");
    if (pos == std::string::npos) return;
    boundary = "--" + boundary.substr(pos + 9);

    // 分割每个部分
    std::vector<std::string> parts;
    size_t start = 0;
    while ((pos = body.find(boundary, start)) != std::string::npos) {
        if (start > 0) {
            parts.push_back(body.substr(start, pos - start - 2)); // -2 去掉CRLF
        }
        start = pos + boundary.length() + 2; // +2 跳过CRLF
    }

    // 处理每个部分
    for (const auto& part : parts) {
        // 查找头部和内容的分隔
        pos = part.find("\r\n\r\n");
        if (pos == std::string::npos) continue;

        // 解析头部
        std::string headers = part.substr(0, pos);
        std::string content = part.substr(pos + 4); // +4 跳过两个CRLF

        // 获取字段名
        std::string name;
        size_t name_pos = headers.find("name=\"");
        if (name_pos != std::string::npos) {
            name_pos += 6;
            size_t name_end = headers.find("\"", name_pos);
            if (name_end != std::string::npos) {
                name = headers.substr(name_pos, name_end - name_pos);
            }
        }

        // 如果是文件
        if (headers.find("filename=\"") != std::string::npos) {
            size_t filename_pos = headers.find("filename=\"");
            if (filename_pos != std::string::npos) {
                filename_pos += 10;
                size_t filename_end = headers.find("\"", filename_pos);
                if (filename_end != std::string::npos) {
                    post_["filename"] = headers.substr(filename_pos, filename_end - filename_pos);
                }
            }
            post_["file_content"] = content;
        } else {
            // 普通表单字段
            post_[name] = content;
        }
    }

}
