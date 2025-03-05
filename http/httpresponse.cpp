//
// Created by fb on 12/27/24.
//

#include "httpresponse.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE =
{
    {".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/nsword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},
    {".css", "text/css"},
    {".js", "text/javascript"},
};

const unordered_map<int, string> HttpResponse::CODE_STATUS =
{
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
};

const unordered_map<int, string> HttpResponse::CODE_PATH =
{
    {400, "/400.html"},
    {403, "/403.html"},
    {404, "/404.html"},
};

HttpResponse::HttpResponse()
{
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
}

HttpResponse::~HttpResponse()
{
    UnmapFile();
}

void HttpResponse::Init(const std::string& srcDir, std::string& path, const std::string& method,
                        const std::unordered_map<std::string, std::string>& post,
                        const std::unordered_map<std::string, std::string> header,
                        bool isKeepAlive, int code, bool isDynamic)
{
    assert(srcDir != "");
    if (mmFile_)
    {
        UnmapFile();
    }
    isDynamic_ = isDynamic;
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    method_ = method;
    post_ = post;
    header_ = header;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
    
}

void HttpResponse::MakeResponse(Buffer& buff)
{
    //判断请求的资源文件
    //cout << srcDir_ + path_ << endl;

    if (isDynamic_) {
        // 动态生成博客列表
        code_ = 200;
        AddStateLine_(buff);
        AddHeader_(buff);
        AddDynamicContent_(buff);  // 新增动态内容处理
    }
    else
    {
        if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode))
        {
            code_ = 404;        // 文件不存在或路径是目录，设置状态码为 404
        }
        else if (!(mmFileStat_.st_mode & S_IROTH))
        {
            //S_IRUSR：用户读  00400
            //S_IRGRP：用户组读 00040
            //S_IROTH: 其他读 00004
            code_ = 403; // 文件没有读权限，设置状态码为 403
        }
        else if (code_ == -1)
        {
            code_ = 200;
        }
        ErrorHtml_();
        AddStateLine_(buff);
        AddHeader_(buff);
        AddContent_(buff);
    }

    
}

void HttpResponse::UnmapFile()
{
    if (mmFile_)
    {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

char* HttpResponse::File()
{
    return mmFile_;
}

size_t HttpResponse::FileLen() const
{
    return mmFileStat_.st_size;
}

void HttpResponse::ErrorContent(Buffer& buff, std::string message)
{
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (CODE_STATUS.count(code_) == 1)
    {
        status = CODE_STATUS.find(code_)->second;
    }
    else
    {
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}

void HttpResponse::AddStateLine_(Buffer& buff)
{
    string status;
    if (CODE_STATUS.count(code_) == 1)
    {
        status = CODE_STATUS.find(code_)->second;
    }
    else
    {
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader_(Buffer& buff)
{
    buff.Append("Connection: ");
    if (isKeepAlive_)
    {
        buff.Append("Keep-alive\r\n");
        buff.Append("Keep-alive: max=6, timeout=120\r\n");
    }
    else
    {
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

void HttpResponse::AddContent_(Buffer& buff)
{
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if (srcFd < 0)
    {
        ErrorContent(buff, "File NotFound");
        return;
    }

    //将文件映射到内存提高文件的访问速度
    //MAP_PRIVATE 建立一个写入时拷贝的私有映射

    /*
    *addr：映射区域的起始地址，通常设为 0，由系统自动选择。

    length：映射区域的长度，这里是文件的大小 mmFileStat_.st_size。

    prot：映射区域的保护模式，PROT_READ 表示只读。

    flags：映射区域的标志，MAP_PRIVATE 表示建立一个写入时拷贝的私有映射。

    fd：文件描述符，这里是 srcFd。

    offset：文件的偏移量，通常设为 0。
     */
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if (*mmRet == -1)
    {
        ErrorContent(buff, "File Not Found");
        return;
    }
    mmFile_ = (char*)mmRet;
    close(srcFd);
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

void HttpResponse::ErrorHtml_()
{
    if (CODE_PATH.count(code_) == 1)
    {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

std::string HttpResponse::GetFileType_()
{
    if (isDynamic_) {
        return "application/json";  // 动态路由返回 JSON
    }
    //判断文件类型
    string::size_type idx = path_.find_last_of('.');
    if (idx == string::npos)
    {
        return "text/html";
    }
    string suffix = path_.substr(idx);
    if (SUFFIX_TYPE.count(suffix) == 1)
    {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/html";
}


// 新增动态内容生成函数
void HttpResponse::AddDynamicContent_(Buffer& buff) {
    std::ostringstream json;

    // 获取博客列表
    if (path_ == "/blogs/getall" && method_ == "GET") {
        std::vector<Blog> blogs = BlogManager::GetBlogs();
        json << "[";
        for (size_t i = 0; i < blogs.size(); ++i) {
            json << "{\"id\":" << blogs[i].id 
                 << ",\"title\":\"" << blogs[i].title 
                 << "\",\"author\":\"" << blogs[i].author 
                 << "\",\"created_at\":\"" << blogs[i].created_at 
                 << "\",\"views\":" << blogs[i].views
                 << ",\"likes_count\":" << blogs[i].likes_count
                 << ",\"comments_count\":" << blogs[i].comments_count
                 << "}";
            if (i < blogs.size() - 1) json << ",";
        }
        json << "]";
    }
    else if (path_.rfind("/blogs/getbyid/", 0) == 0 && method_ == "GET") {
        string str_id = path_.substr(15);  // 去掉 "/blogs/getbyid/"
        int id = atoi(str_id.c_str());
        Blog blog = BlogManager::GetBlogById(id);
        
        // 获取当前用户是否已点赞
        string username = getUsernameFromToken();
        bool is_liked = false;
        if (!username.empty()) {
            is_liked = BlogManager::HasUserLikedBlog(id, username);
        }
        
        json << "{"
             << "\"id\":" << blog.id << ","
             << "\"title\":\"" << blog.title << "\","
             << "\"content\":\"" << blog.content << "\","
             << "\"author\":\"" << blog.author << "\","
             << "\"created_at\":\"" << blog.created_at << "\","
             << "\"views\":" << blog.views << ","
             << "\"likes_count\":" << blog.likes_count << ","
             << "\"comments_count\":" << blog.comments_count << ","
             << "\"is_liked\":" << (is_liked ? "true" : "false")
             << "}";
    }
    // 获取用户的文章列表
    else if (path_.rfind("/blogs/user/", 0) == 0 && path_.find("/posts") != string::npos && method_ == "GET") 
    {
        string username = path_.substr(12, path_.find("/posts") - 12);
        std::vector<Blog> posts = BlogManager::GetBlogsByAuthor(username);
        json << "[";
        for (size_t i = 0; i < posts.size(); ++i) {
            json << "{\"id\":" << posts[i].id 
                 << ",\"title\":\"" << posts[i].title 
                 << "\",\"created_at\":\"" << posts[i].created_at 
                 << "\",\"views\":" << posts[i].views
                 << ",\"likes_count\":" << posts[i].likes_count
                 << ",\"comments_count\":" << posts[i].comments_count
                 << "}";
            if (i < posts.size() - 1) json << ",";
        }
        json << "]";
    }
    // 获取用户信息
    else if (path_.rfind("/blogs/user/", 0) == 0 && path_.find("/profile") != string::npos && method_ == "GET") 
    {
        string username = path_.substr(12, path_.find("/profile") - 12);  // 去掉 "/blogs/user/"和"/profile"
        User user = BlogManager::GetUser(username);
        if (user.username.empty()) {
            code_ = 404;
            json << "{\"error\":\"User not found\"}";
        } else {
            json << "{"
                 << "\"username\":\"" << user.username << "\","
                 << "\"nickname\":\"" << user.nickname << "\","
                 << "\"email\":\"" << user.email << "\","
                 << "\"avatar_url\":\"" << user.avatar_url << "\","
                 << "\"bio\":\"" << user.bio << "\","
                 << "\"posts_count\":" << user.posts_count << ","
                 << "\"likes_received\":" << user.likes_received << ","
                 << "\"followers_count\":" << user.followers_count << ","
                 << "\"following_count\":" << user.following_count
                 << "}";
        }
    }
    // 获取用户基本信息
    else if (path_.rfind("/blogs/user/", 0) == 0 && path_.find("/basic") != string::npos && method_ == "GET") 
    {
        string username = path_.substr(12, path_.find("/basic") - 12);  // 去掉 "/blogs/user/"和"/basic"
        User user = BlogManager::GetUser(username);
        if (user.username.empty()) {
            code_ = 404;
            json << "{\"error\":\"User not found\"}";
        } else {
            json << "{"
                 << "\"posts_count\":" << user.posts_count << ","
                 << "\"likes_received\":" << user.likes_received << ","
                 << "\"followers_count\":" << user.followers_count << ","
                 << "\"following_count\":" << user.following_count
                 << "}";
        }
    }
    
    // 获取用户点赞的文章
    else if (path_.rfind("/blogs/user/", 0) == 0 && path_.find("/likes") != string::npos && method_ == "GET") 
    {
        string username = path_.substr(12, path_.find("/likes") - 12);
        std::vector<Blog> likes = BlogManager::GetUserLikes(username);
        json << "[";
        for (size_t i = 0; i < likes.size(); ++i) {
            json << "{\"id\":" << likes[i].id 
                 << ",\"title\":\"" << likes[i].title 
                 << "\",\"author\":\"" << likes[i].author
                 << "\",\"created_at\":\"" << likes[i].created_at 
                 << "\"}";
            if (i < likes.size() - 1) json << ",";
        }
        json << "]";
    }
    // 获取用户收藏的文章
    else if (path_.rfind("/blogs/user/", 0) == 0 && path_.find("/favorites") != string::npos && method_ == "GET") 
    {
        string username = path_.substr(11, path_.find("/favorites") - 11);
        std::vector<Blog> favorites = BlogManager::GetUserFavorites(username);
        json << "[";
        for (size_t i = 0; i < favorites.size(); ++i) {
            json << "{\"id\":" << favorites[i].id 
                 << ",\"title\":\"" << favorites[i].title 
                 << "\",\"author\":\"" << favorites[i].author
                 << "\",\"created_at\":\"" << favorites[i].created_at 
                 << "\"}";
            if (i < favorites.size() - 1) json << ",";
        }
        json << "]";
    }
    // 获取用户通知
    else if (path_.rfind("/blogs/user/", 0) == 0 && path_.find("/notifications") != string::npos && method_ == "GET") 
    {
        string username = path_.substr(12, path_.find("/notifications") - 12);
        std::vector<Notification> notifications = BlogManager::GetUserNotifications(username);
        json << "[";
        for (size_t i = 0; i < notifications.size(); ++i) {
            json << "{\"id\":" << notifications[i].id 
                 << ",\"content\":\"" << notifications[i].content 
                 << "\",\"is_read\":" << (notifications[i].is_read ? "true" : "false")
                 << ",\"created_at\":\"" << notifications[i].created_at 
                 << "\"}";
            if (i < notifications.size() - 1) json << ",";
        }
        json << "]";
    }
    // 登录成功响应
    else if (path_.find("/login/success") != string::npos && method_ == "POST") 
    {
        string token = generateToken(post_["username"]);  // 需要实现 generateToken 函数
        json << "{\"success\":true,\"token\":\"" << token 
             << "\",\"message\":\"Login successful\"}";
    }
    // 登录失败响应
    else if (path_.find("/login/error") != string::npos && method_ == "POST") 
    {
        json << "{\"success\":false,\"message\":\"Invalid credentials\"}";
    }
    // 注册成功响应
    else if (path_.find("/register/success") != string::npos && method_ == "POST") 
    {
        json << "{\"success\":true,\"message\":\"Registration successful\"}";
    }
    // 注册失败响应
    else if (path_.find("/register/error") != string::npos && method_ == "POST") 
    {
        json << "{\"success\":false,\"message\":\"Username already exists\"}";
    }
    // 获取文章评论
    else if (path_.rfind("/comments/", 0) == 0 && method_ == "GET") {
        string str_id = path_.substr(10);  // 去掉 "/comments/"
        int blog_id = atoi(str_id.c_str());
        
        std::vector<Comment> comments = BlogManager::GetComments(blog_id);
        json << "[";
        for (size_t i = 0; i < comments.size(); ++i) {
            json << "{"
                 << "\"id\":" << comments[i].id << ","
                 << "\"blog_id\":" << comments[i].blog_id << ","
                 << "\"user_id\":\"" << comments[i].user_id << "\","
                 << "\"content\":\"" << comments[i].content << "\","
                 << "\"parent_id\":" << (comments[i].parent_id != -1 ? std::to_string(comments[i].parent_id) : "null") << ","
                 << "\"likes_count\":" << comments[i].likes_count << ","
                 << "\"created_at\":\"" << comments[i].created_at << "\""
                 << "}";
            if (i < comments.size() - 1) json << ",";
        }
        json << "]";
    }
    // 创建评论
    else if (path_ == "/comments" && method_ == "POST") {
        if (post_.count("blog_id") && post_.count("user_id") && post_.count("content")) {
            
            int blog_id = std::stoi(post_["blog_id"]);
            string user_id = post_["user_id"];
            string content = post_["content"];
            int parent_id = -1;  // 默认为 -1，表示没有父评论
            if (post_.count("parent_id") && post_["parent_id"] != "null") {
                parent_id = std::stoi(post_["parent_id"]);
            }
            
            bool success = BlogManager::CreateComment(blog_id, user_id, content, parent_id);
            if (success) {
                json << "{\"success\":true,\"message\":\"Comment created successfully\"}";
            } else {
                code_ = 400;
                json << "{\"success\":false,\"message\":\"Failed to create comment\"}";
            }
        } else {
            code_ = 400;
            json << "{\"success\":false,\"message\":\"Missing required fields\"}";
        }
    }
    // 删除评论
    else if (path_.rfind("/comments/", 0) == 0 && method_ == "DELETE") {
        string str_id = path_.substr(10);  // 去掉 "/comments/"
        int comment_id = atoi(str_id.c_str());
        
        // 从 Authorization header 中获取用户信息
        string username = getUsernameFromToken();

        if (username.empty()) {
            code_ = 401;
            json << "{\"success\":false,\"message\":\"Unauthorized\"}";
        }
        else if (BlogManager::CanDeleteComment(comment_id, username)) {
            bool success = BlogManager::DeleteComment(comment_id);
            if (success) {
                json << "{\"success\":true,\"message\":\"Comment deleted successfully\"}";
            } else {
                code_ = 400;
                json << "{\"success\":false,\"message\":\"Failed to delete comment\"}";
            }
        } else {
            code_ = 403;
            json << "{\"success\":false,\"message\":\"Permission denied\"}";
        }
    }
    // 评论点赞
    else if (path_.rfind("/likes/comment/", 0) == 0 && method_ == "POST") {
        string str_id = path_.substr(15);  // 去掉 "/likes/comment/"
        int comment_id = atoi(str_id.c_str());
        
        string username = getUsernameFromToken();
        
        bool success = BlogManager::ToggleCommentLike(comment_id, username);
        if (success) {
            json << "{\"success\":true,\"message\":\"Like toggled successfully\"}";
        } else {
            code_ = 400;
            json << "{\"success\":false,\"message\":\"Failed to toggle like\"}";
        }
    }
    // 点赞文章
    else if (path_.rfind("/likes/", 0) == 0 && method_ == "POST") {
        string str_id = path_.substr(7);  // 去掉 "/likes/"
        int blog_id = atoi(str_id.c_str());
        
        string username = getUsernameFromToken();
        if (username.empty()) {
            code_ = 401;
            json << "{\"success\":false,\"message\":\"Unauthorized\"}";
        } else {
            bool success = BlogManager::ToggleBlogLike(blog_id, username);
            if (success) {
                bool is_liked = BlogManager::HasUserLikedBlog(blog_id, username);
                int likes_count = BlogManager::GetBlogLikesCount(blog_id);
                json << "{\"success\":true,"
                     << "\"liked\":" << (is_liked ? "true" : "false") << ","
                     << "\"likes_count\":" << likes_count << "}";
            } else {
                code_ = 400;
                json << "{\"success\":false,\"message\":\"Failed to toggle like\"}";
            }
        }
    }
    // 获取文章详情
    else if (path_.rfind("/posts/", 0) == 0 && method_ == "GET") {
        string str_id = path_.substr(7);
        int id = atoi(str_id.c_str());
        Blog blog = BlogManager::GetBlogById(id);
        
        // 获取当前用户是否已点赞
        string username = getUsernameFromToken();
        bool is_liked = false;
        if (!username.empty()) {
            is_liked = BlogManager::HasUserLikedBlog(id, username);
        }
        
        json << "{"
             << "\"id\":" << blog.id << ","
             << "\"title\":\"" << blog.title << "\","
             << "\"content\":\"" << blog.content << "\","
             << "\"author\":\"" << blog.author << "\","
             << "\"created_at\":\"" << blog.created_at << "\","
             << "\"views\":" << blog.views << ","
             << "\"likes_count\":" << blog.likes_count << ","
             << "\"comments_count\":" << blog.comments_count << ","
             << "\"is_liked\":" << (is_liked ? "true" : "false")
             << "}";
    }
    // 发布文章
    else if (path_ == "/blogs/create" && method_ == "POST") {
        // 获取当前用户
        string username = getUsernameFromToken();
        if (username.empty()) {
            code_ = 401;
            json << "{\"success\":false,\"message\":\"Unauthorized\"}";
        } else {
            // 从 post 数据中获取标题和内容
            string title = post_["title"];
            string content = post_["content"];
            
            if (title.empty() || content.empty()) {
                code_ = 400;
                json << "{\"success\":false,\"message\":\"Title and content are required\"}";
            } else {
                bool success = BlogManager::CreateBlog(title, content, username);
                if (success) {
                    json << "{\"success\":true,\"message\":\"Blog created successfully\"}";
                } else {
                    code_ = 500;
                    json << "{\"success\":false,\"message\":\"Failed to create blog\"}";
                }
            }
        }
    }
    // 删除文章
    else if (path_.rfind("/blogs/", 0) == 0 && method_ == "DELETE") {
        string str_id = path_.substr(7);  // 去掉 "/blogs/"
        int blog_id = atoi(str_id.c_str());
        
        // 从 Authorization header 中获取用户信息
        string username = getUsernameFromToken();
        
        if (username.empty()) {
            code_ = 401;
            json << "{\"success\":false,\"message\":\"Unauthorized\"}";
        }
        else if (BlogManager::CanDeleteBlog(blog_id, username)) {
            bool success = BlogManager::DeleteBlog(blog_id);
            if (success) {
                json << "{\"success\":true,\"message\":\"Blog deleted successfully\"}";
            } else {
                code_ = 500;
                json << "{\"success\":false,\"message\":\"Failed to delete blog\"}";
            }
        } else {
            code_ = 403;
            json << "{\"success\":false,\"message\":\"Permission denied\"}";
        }
    }
    // 处理markdown文件上传
    else if (path_ == "/blogs/upload" && method_ == "POST") {
        string username = getUsernameFromToken();
        if (username.empty()) {
            code_ = 401;
            json << "{\"success\":false,\"message\":\"Unauthorized\"}";
        } else {
            // 从 multipart/form-data 中获取文件内容
            string title = post_["title"];
            string filename = post_["filename"];
            string content = post_["file_content"];

            if (title.empty() || content.empty()) {
                code_ = 400;
                json << "{\"success\":false,\"message\":\"Title and file are required\"}";
            } else {
                // 创建博客文章，保存文件内容
                bool success = BlogManager::CreateBlogWithFile(
                    title, content, username, filename, "markdown"
                );
                if (success) {
                    json << "{\"success\":true,\"message\":\"Blog uploaded successfully\"}";
                } else {
                    code_ = 500;
                    json << "{\"success\":false,\"message\":\"Failed to upload blog\"}";
                }
            }
        }
    }

    string body = json.str();
    if (body.empty()) {
        body = "{\"error\":\"Invalid request or empty response\"}";
        code_ = 400;
    }
    
    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}

// 辅助函数：从查询字符串中提取参数
std::string HttpResponse::getQueryParam(const std::string& query, const std::string& param) {
    size_t start = query.find(param + "=");
    if (start == string::npos) return "";
    start += param.length() + 1; // 跳过 "="
    size_t end = query.find("&", start);
    if (end == string::npos) end = query.length();
    return query.substr(start, end - start);
}

std::string HttpResponse::generateToken(const std::string& username) {
    // 获取当前时间戳
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    // 生成随机数
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);
    int random = dis(gen);

    // 将用户名、时间戳和随机数组合成token
    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << username << "_"
       << std::setw(16) << now_ms << "_"
       << std::setw(6) << random;

    return ss.str();
}

std::string HttpResponse::getUsernameFromToken()
{
    // 从 Authorization header 中获取用户信息
    string username;
    auto it = header_.find("Authorization");
    if (it != header_.end()) {
        string auth_header = it->second;
        if (auth_header.substr(0, 7) == "Bearer ") {
            string token = auth_header.substr(7);
            // 从 token 中提取用户名（假设 token 格式为：username_timestamp_random）
            size_t underscore_pos = token.find('_');
            if (underscore_pos != string::npos) {
                username = token.substr(0, underscore_pos);
            }
        }
    }
    return username;
}
