#ifndef BLOGMANAGER_H
#define BLOGMANAGER_H

#include <string>
#include <vector>
#include "../pool/sqlconnpool.h"
#include <optional>
#include <unordered_map>

struct Blog {
    int id;
    std::string title;
    std::string content;
    std::string content_type;
    std::string original_filename;
    std::string author;
    std::string created_at;
    int views{0};
    int likes_count{0};
    int comments_count{0};
};

struct User {
    std::string username;
    std::string email;
    std::string nickname;
    std::string avatar_url;
    std::string gender;
    std::string birth_date;
    std::string location;
    std::string bio;
    std::string website;
    std::string registered_at;
    int posts_count{0};
    int likes_received{0};
    int followers_count{0};
    int following_count{0};
};

struct Notification {
    int id;
    std::string content;
    bool is_read;
    std::string created_at;
};

struct Comment {
    int id;
    int blog_id;
    std::string user_id;
    std::string content;
    int parent_id;  // 使用 -1 表示没有父评论
    int likes_count;
    std::string created_at;
};

class BlogManager {
public:
    // 创建博客
    static bool CreateBlog(const std::string& title, const std::string& content, const std::string& author);

    // 获取所有博客列表
    static std::vector<Blog> GetBlogs();

    // 获取单篇博客
    static Blog GetBlogById(int id);

    static std::vector<Blog> GetBlogsByAuthor(const std::string& author); 
    static User GetUser(const std::string& username); 
    static bool UpdateUser(const std::string& username, const std::string& newPassword); 

    // 新增方法
    static std::vector<Blog> GetUserLikes(const std::string& username);
    static std::vector<Blog> GetUserFavorites(const std::string& username);
    static std::vector<Notification> GetUserNotifications(const std::string& username);

    // 评论相关的函数
    static std::vector<Comment> GetComments(int blog_id);
    static bool CreateComment(int blog_id, const std::string& user_id, 
                            const std::string& content, int parent_id = -1);
    static bool DeleteComment(int comment_id);
    static bool CanDeleteComment(int comment_id, const std::string& username);
    static bool ToggleCommentLike(int comment_id, const std::string& username);

    // 文章点赞相关
    static bool ToggleBlogLike(int blog_id, const std::string& username, bool& is_liked_out, int& likes_count_out);
    static bool HasUserLikedBlog(int blog_id, const std::string& username);
    static int GetBlogLikesCount(int blog_id);

    // 检查用户是否可以删除文章
    static bool CanDeleteBlog(int blog_id, const std::string& username);
    //删除文章
    static bool DeleteBlog(int blog_id);

    // 在 BlogManager 类中添加新方法
    static bool CreateBlogWithFile(
        const std::string& title,
        const std::string& content,
        const std::string& author,
        const std::string& filename,
        const std::string& content_type
    );

    static bool UpdateUserProfile(
        const std::string& username,
        const std::unordered_map<std::string, std::string>& profileData
    );
};

#endif // BLOGMANAGER_H