#include "blogmanager.h"
#include "../log/log.h"
#include <mysql/mysql.h>
#include "../pool/sqlconnRAll.h"
#include <cassert>


bool BlogManager::CreateBlog(const std::string& title, const std::string& content, const std::string& author) {
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) {
        LOG_ERROR("Failed to get MySQL connection for CreateBlog");
        return false;
    }

    // 转义输入，防止 SQL 注入
    char escaped_title[512];
    char escaped_content[2048];
    char escaped_author[128];
    mysql_real_escape_string(sql, escaped_title, title.c_str(), title.size());
    mysql_real_escape_string(sql, escaped_content, content.c_str(), content.size());
    mysql_real_escape_string(sql, escaped_author, author.c_str(), author.size());

    // 创建文章，让数据库自动处理时间戳
    // created_at 和 updated_at 会由 MySQL 自动设置为 CURRENT_TIMESTAMP
    char query[2560];
    snprintf(query, sizeof(query), 
             "INSERT INTO blogs (title, content, author) "
             "SELECT '%s', '%s', '%s' "
             "FROM DUAL "
             "WHERE EXISTS (SELECT 1 FROM user WHERE username = '%s')",  // 确保作者存在
             escaped_title, escaped_content, escaped_author, escaped_author);
    
    LOG_DEBUG("Executing query: %s", query);
    
    if (mysql_query(sql, query)) {
        LOG_ERROR("Insert blog failed: %s", mysql_error(sql));
        return false;
    }

    // 检查是否真的插入了记录
    if (mysql_affected_rows(sql) == 0) {
        LOG_ERROR("No blog created: author %s might not exist", author.c_str());
        return false;
    }

    LOG_INFO("Blog created: title=%s, author=%s", title.c_str(), author.c_str());
    return true;
}

std::vector<Blog> BlogManager::GetBlogs() {
    std::vector<Blog> blogs;
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) {
        LOG_ERROR("Failed to get MySQL connection for GetBlogs");
        return blogs;
    }

    // 修改查询，使用实时点赞数
    const char* query = 
        "SELECT b.id, b.title, b.content, b.author, b.views, "
        "b.comments_count, b.created_at, "
        "(SELECT COUNT(*) FROM likes l WHERE l.blog_id = b.id) as real_likes_count "
        "FROM blogs b "
        "ORDER BY b.created_at DESC";

    if (mysql_query(sql, query)) {
        LOG_ERROR("Query blogs failed: %s", mysql_error(sql));
        return blogs;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) {
        LOG_ERROR("No result: %s", mysql_error(sql));
        return blogs;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        Blog blog;
        blog.id = atoi(row[0]);
        blog.title = row[1] ? row[1] : "";
        blog.content = row[2] ? row[2] : "";
        blog.author = row[3] ? row[3] : "";
        blog.views = row[4] ? atoi(row[4]) : 0;
        blog.comments_count = row[5] ? atoi(row[5]) : 0;
        blog.created_at = row[6] ? row[6] : "";
        blog.likes_count = row[7] ? atoi(row[7]) : 0;  // 使用实时计算的点赞数
        blogs.push_back(blog);
    }

    mysql_free_result(res);
    LOG_DEBUG("Fetched %zu blogs", blogs.size());
    return blogs;
}

Blog BlogManager::GetBlogById(int id) {
    Blog blog;
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) {
        LOG_ERROR("Failed to get MySQL connection for GetBlogById");
        return blog;
    }

    char query[512];
    // 明确指定需要的列，而不是使用 *
    snprintf(query, sizeof(query),
        "SELECT b.id, b.title, b.content, b.author, b.views, b.likes_count, "
        "b.comments_count, b.created_at, b.content_type, "
        "(SELECT COUNT(*) FROM likes l WHERE l.blog_id = b.id) as real_likes_count "
        "FROM blogs b "
        "WHERE b.id = %d",
        id);

    if (mysql_query(sql, query)) {
        LOG_ERROR("Query blog failed: %s", mysql_error(sql));
        std::cout << "Query blog failed: " << mysql_error(sql) << std::endl;
        return blog;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) {
        LOG_ERROR("No result: %s", mysql_error(sql));
        std::cout << "No result: " << mysql_error(sql) << std::endl;
        return blog;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        blog.id = atoi(row[0]);
        blog.title = row[1] ? row[1] : "";
        blog.content = row[2] ? row[2] : "";
        blog.author = row[3] ? row[3] : "";
        blog.views = row[4] ? atoi(row[4]) : 0;
        blog.likes_count = row[9] ? atoi(row[9]) : 0;  // 使用实时计算的点赞数
        blog.comments_count = row[6] ? atoi(row[6]) : 0;
        blog.created_at = row[7] ? row[7] : "";
        blog.content_type = row[8] ? row[8] : "";

        // 添加调试日志
        LOG_DEBUG("Blog %d likes count: %d", blog.id, blog.likes_count);
    }

    mysql_free_result(res);
    return blog;
}


std::vector<Blog> BlogManager::GetBlogsByAuthor(const std::string& author) {
    std::vector<Blog> blogs;
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) {
        LOG_ERROR("Failed to get MySQL connection for GetBlogsByAuthor");
        return blogs;
    }

    // 转义输入，防止 SQL 注入
    char escaped_author[128];
    mysql_real_escape_string(sql, escaped_author, author.c_str(), author.length());

    char query[256];
    snprintf(query, sizeof(query), 
             "SELECT id, title, content, author, created_at FROM blogs "
             "WHERE author = '%s' "
             "ORDER BY created_at DESC", 
             escaped_author);

    LOG_DEBUG("Executing query: %s", query);  // 添加日志

    if (mysql_query(sql, query)) {
        LOG_ERROR("Query blogs by author failed: %s", mysql_error(sql));
        return blogs;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) {
        LOG_ERROR("No result: %s", mysql_error(sql));
        return blogs;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        Blog blog;
        blog.id = atoi(row[0]);
        blog.title = row[1] ? row[1] : "";
        blog.content = row[2] ? row[2] : "";
        blog.author = row[3] ? row[3] : "";
        blog.created_at = row[4] ? row[4] : "";
        blogs.push_back(blog);
        LOG_DEBUG("Found blog: id=%d, title=%s, author=%s", 
                  blog.id, blog.title.c_str(), blog.author.c_str());
    }

    mysql_free_result(res);
    LOG_DEBUG("Found %zu blogs for author %s", blogs.size(), author.c_str());
    return blogs;
}

User BlogManager::GetUser(const std::string& username) 
{
    User user;
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for GetUser");
        return user;
    }

    // 转义用户名
    char escaped_username[128];
    mysql_real_escape_string(sql, escaped_username, username.c_str(), username.length());

    // 构建查询语句
    char query[512];
    // snprintf(query, sizeof(query),
    //     "SELECT u.username, u.email, u.nickname, u.avatar_url, u.bio, u.created_at, "
    //     "(SELECT COUNT(*) FROM blogs WHERE author = u.username) as posts_count, "
    //     "(SELECT COUNT(*) FROM likes WHERE blog_id IN (SELECT id FROM blogs WHERE author = u.username)) as likes_received, "
    //     "(SELECT COUNT(*) FROM follows WHERE following_id = u.username) as followers_count, "
    //     "(SELECT COUNT(*) FROM follows WHERE follower_id = u.username) as following_count "
    //     "FROM user u WHERE u.username = '%s'",
    //     escaped_username);

    snprintf(query, sizeof(query),
    "SELECT u.username, u.email, u.nickname, u.avatar_url, u.gender, u.birth_date, "
    "u.location, u.bio, u.website, u.created_at, "
    "(SELECT COUNT(*) FROM blogs WHERE author = u.username) as posts_count, "
    "(SELECT COUNT(*) FROM likes WHERE blog_id IN (SELECT id FROM blogs WHERE author = u.username)) as likes_received, "
    "(SELECT COUNT(*) FROM follows WHERE following_id = u.username) as followers_count, "
    "(SELECT COUNT(*) FROM follows WHERE follower_id = u.username) as following_count "
    "FROM user u WHERE u.username = '%s'",
    escaped_username);

    LOG_DEBUG("Executing query: %s", query);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Query user failed: %s", mysql_error(sql));
        return user;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) 
    {
        LOG_ERROR("No result: %s", mysql_error(sql));
        return user;
    }

    // if (MYSQL_ROW row = mysql_fetch_row(res)) 
    // {
    //     user.username = row[0] ? row[0] : "";
    //     user.email = row[1] ? row[1] : "";
    //     user.nickname = row[2] ? row[2] : user.username;
    //     user.avatar_url = row[3] ? row[3] : "";
    //     user.bio = row[4] ? row[4] : "";
    //     user.registered_at = row[5] ? row[5] : "";
    //     user.posts_count = row[6] ? atoi(row[6]) : 0;
    //     user.likes_received = row[7] ? atoi(row[7]) : 0;
    //     user.followers_count = row[8] ? atoi(row[8]) : 0;
    //     user.following_count = row[9] ? atoi(row[9]) : 0;
    // }

    if (MYSQL_ROW row = mysql_fetch_row(res)) 
    {
        user.username = row[0] ? row[0] : "";
        user.email = row[1] ? row[1] : "";
        user.nickname = row[2] ? row[2] : user.username;
        user.avatar_url = row[3] ? row[3] : "";
        user.gender = row[4] ? row[4] : "";
        user.birth_date = row[5] ? row[5] : "";
        user.location = row[6] ? row[6] : "";
        user.bio = row[7] ? row[7] : "";
        user.website = row[8] ? row[8] : "";
        user.registered_at = row[9] ? row[9] : "";
        user.posts_count = row[10] ? atoi(row[10]) : 0;
        user.likes_received = row[11] ? atoi(row[11]) : 0;
        user.followers_count = row[12] ? atoi(row[12]) : 0;
        user.following_count = row[13] ? atoi(row[13]) : 0;
    }

    mysql_free_result(res);
    return user;
}

bool BlogManager::UpdateUser(const std::string& username, const std::string& newPassword) 
{
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) return false;

    char query[256];
    snprintf(query, sizeof(query), "UPDATE user SET password = '%s' WHERE username = '%s'", newPassword.c_str(), username.c_str());
    if (mysql_query(sql, query)) return false;
    return true;
}

std::vector<Blog> BlogManager::GetUserLikes(const std::string& username) 
{
    std::vector<Blog> likes;
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for GetUserLikes");
        return likes;
    }

    char escaped_username[128];
    mysql_real_escape_string(sql, escaped_username, username.c_str(), username.length());

    char query[512];
    snprintf(query, sizeof(query),
        "SELECT b.id, b.title, b.author, b.created_at "
        "FROM blogs b "
        "INNER JOIN likes l ON b.id = l.blog_id "
        "WHERE l.user_id = '%s' "
        "ORDER BY l.created_at DESC",
        escaped_username);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Query user likes failed: %s", mysql_error(sql));
        return likes;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) 
    {
        LOG_ERROR("No result: %s", mysql_error(sql));
        return likes;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) 
    {
        Blog blog;
        blog.id = atoi(row[0]);
        blog.title = row[1] ? row[1] : "";
        blog.author = row[2] ? row[2] : "";
        blog.created_at = row[3] ? row[3] : "";
        likes.push_back(blog);
    }

    mysql_free_result(res);
    return likes;
}

std::vector<Blog> BlogManager::GetUserFavorites(const std::string& username) 
{
    std::vector<Blog> favorites;
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for GetUserFavorites");
        return favorites;
    }

    char escaped_username[128];
    mysql_real_escape_string(sql, escaped_username, username.c_str(), username.length());

    char query[512];
    snprintf(query, sizeof(query),
        "SELECT b.id, b.title, b.author, b.created_at "
        "FROM blogs b "
        "INNER JOIN favorites f ON b.id = f.blog_id "
        "WHERE f.user_id = '%s' "
        "ORDER BY f.created_at DESC",
        escaped_username);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Query user favorites failed: %s", mysql_error(sql));
        return favorites;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) 
    {
        LOG_ERROR("No result: %s", mysql_error(sql));
        return favorites;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) 
    {
        Blog blog;
        blog.id = atoi(row[0]);
        blog.title = row[1] ? row[1] : "";
        blog.author = row[2] ? row[2] : "";
        blog.created_at = row[3] ? row[3] : "";
        favorites.push_back(blog);
    }

    mysql_free_result(res);
    return favorites;
}

std::vector<Notification> BlogManager::GetUserNotifications(const std::string& username) 
{
    std::vector<Notification> notifications;
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for GetUserNotifications");
        return notifications;
    }

    char escaped_username[128];
    mysql_real_escape_string(sql, escaped_username, username.c_str(), username.length());

    char query[512];
    snprintf(query, sizeof(query),
        "SELECT id, content, is_read, created_at "
        "FROM notifications "
        "WHERE user_id = '%s' "
        "ORDER BY created_at DESC",
        escaped_username);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Query user notifications failed: %s", mysql_error(sql));
        return notifications;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) 
    {
        LOG_ERROR("No result: %s", mysql_error(sql));
        return notifications;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) 
    {
        Notification notif;
        notif.id = atoi(row[0]);
        notif.content = row[1] ? row[1] : "";
        notif.is_read = row[2] ? (atoi(row[2]) != 0) : false;
        notif.created_at = row[3] ? row[3] : "";
        notifications.push_back(notif);
    }

    mysql_free_result(res);
    return notifications;
}

// 获取文章的评论列表
std::vector<Comment> BlogManager::GetComments(int blog_id) 
{
    std::vector<Comment> comments;
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for GetComments");
        return comments;
    }

    char query[512];
    snprintf(query, sizeof(query),
        "SELECT c.id, c.blog_id, c.user_id, c.content, c.parent_id, "
        "c.likes_count, c.created_at "
        "FROM comments c "
        "WHERE c.blog_id = %d "
        "ORDER BY c.created_at ASC",  // 按时间正序，便于构建评论树
        blog_id);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Query comments failed: %s", mysql_error(sql));
        return comments;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) 
    {
        LOG_ERROR("No result: %s", mysql_error(sql));
        return comments;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) 
    {
        Comment comment;
        comment.id = atoi(row[0]);
        comment.blog_id = atoi(row[1]);
        comment.user_id = row[2] ? row[2] : "";
        comment.content = row[3] ? row[3] : "";
        comment.parent_id = row[4] ? atoi(row[4]) : -1;  // 使用 -1 表示没有父评论
        comment.likes_count = row[5] ? atoi(row[5]) : 0;
        comment.created_at = row[6] ? row[6] : "";
        comments.push_back(comment);
    }

    mysql_free_result(res);
    return comments;
}

// 创建新评论
bool BlogManager::CreateComment(int blog_id, const std::string& user_id, 
                              const std::string& content, int parent_id) 
                              {
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for CreateComment");
        return false;
    }

    // 转义内容，防止SQL注入
    char escaped_content[2048];
    char escaped_user_id[128];
    mysql_real_escape_string(sql, escaped_content, content.c_str(), content.length());
    mysql_real_escape_string(sql, escaped_user_id, user_id.c_str(), user_id.length());

    char query[2560];
    if (parent_id != -1) 
    {
        snprintf(query, sizeof(query),
            "INSERT INTO comments (blog_id, user_id, content, parent_id) "
            "VALUES (%d, '%s', '%s', %d)",
            blog_id, escaped_user_id, escaped_content, parent_id);
    } 
    else 
    {
        snprintf(query, sizeof(query),
            "INSERT INTO comments (blog_id, user_id, content) "
            "VALUES (%d, '%s', '%s')",
            blog_id, escaped_user_id, escaped_content);
    }

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Create comment failed: %s", mysql_error(sql));
        return false;
    }

    // 更新文章的评论数
    snprintf(query, sizeof(query),
        "UPDATE blogs SET comments_count = comments_count + 1 "
        "WHERE id = %d",
        blog_id);
    
    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Update comments_count failed: %s", mysql_error(sql));
        // 不需要回滚评论创建，因为评论计数不准确比丢失评论好
    }

    return true;
}

// 删除评论
bool BlogManager::DeleteComment(int comment_id)
{
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for DeleteComment");
        return false;
    }

    // 首先获取评论所属的博客ID
    char query[256];
    snprintf(query, sizeof(query),
        "SELECT blog_id FROM comments WHERE id = %d",
        comment_id);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Query comment blog_id failed: %s", mysql_error(sql));
        return false;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) 
    {
        LOG_ERROR("No result: %s", mysql_error(sql));
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) 
    {
        mysql_free_result(res);
        LOG_ERROR("Comment not found: %d", comment_id);
        return false;
    }

    int blog_id = atoi(row[0]);
    mysql_free_result(res);

    // 开始事务
    if (mysql_query(sql, "START TRANSACTION")) 
    {
        LOG_ERROR("Start transaction failed: %s", mysql_error(sql));
        return false;
    }

    // 删除评论及其所有回复
    snprintf(query, sizeof(query),
        "DELETE FROM comments WHERE id = %d OR parent_id = %d",
        comment_id, comment_id);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Delete comment failed: %s", mysql_error(sql));
        mysql_query(sql, "ROLLBACK");
        return false;
    }

    // 更新文章的评论计数
    snprintf(query, sizeof(query),
        "UPDATE blogs SET comments_count = GREATEST(0, comments_count - 1) "
        "WHERE id = %d",
        blog_id);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Update comments_count failed: %s", mysql_error(sql));
        mysql_query(sql, "ROLLBACK");
        return false;
    }

    // 提交事务
    if (mysql_query(sql, "COMMIT")) 
    {
        LOG_ERROR("Commit transaction failed: %s", mysql_error(sql));
        mysql_query(sql, "ROLLBACK");
        return false;
    }

    return true;
}

// 检查用户是否可以删除评论
bool BlogManager::CanDeleteComment(int comment_id, const std::string& username) 
{
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for CanDeleteComment");
        return false;
    }

    char escaped_username[128];
    mysql_real_escape_string(sql, escaped_username, username.c_str(), username.length());

    char query[256];
    snprintf(query, sizeof(query),
        "SELECT 1 FROM comments WHERE id = %d AND user_id = '%s'",
        comment_id, escaped_username);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Check comment permission failed: %s", mysql_error(sql));
        return false;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) 
    {
        LOG_ERROR("No result: %s", mysql_error(sql));
        return false;
    }

    bool can_delete = mysql_num_rows(res) > 0;
    mysql_free_result(res);
    return can_delete;
}

// 切换评论点赞状态
bool BlogManager::ToggleCommentLike(int comment_id, const std::string& username) 
{
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for ToggleCommentLike");
        return false;
    }

    char escaped_username[128];
    mysql_real_escape_string(sql, escaped_username, username.c_str(), username.length());

    // 开始事务
    if (mysql_query(sql, "START TRANSACTION")) 
    {
        LOG_ERROR("Start transaction failed: %s", mysql_error(sql));
        return false;
    }

    // 检查是否已经点赞
    char query[512];
    snprintf(query, sizeof(query),
        "SELECT 1 FROM likes WHERE user_id = '%s' AND comment_id = %d",
        escaped_username, comment_id);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Check like status failed: %s", mysql_error(sql));
        mysql_query(sql, "ROLLBACK");
        return false;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) 
    {
        LOG_ERROR("No result: %s", mysql_error(sql));
        mysql_query(sql, "ROLLBACK");
        return false;
    }

    bool already_liked = mysql_num_rows(res) > 0;
    mysql_free_result(res);

    if (already_liked) 
    {
        // 取消点赞
        snprintf(query, sizeof(query),
            "DELETE FROM likes WHERE user_id = '%s' AND comment_id = %d",
            escaped_username, comment_id);
    } 
    else 
    {
        // 添加点赞
        snprintf(query, sizeof(query),
            "INSERT INTO likes (user_id, comment_id) VALUES ('%s', %d)",
            escaped_username, comment_id);
    }

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Toggle like failed: %s", mysql_error(sql));
        mysql_query(sql, "ROLLBACK");
        return false;
    }

    // 更新评论的点赞计数
    snprintf(query, sizeof(query),
        "UPDATE comments SET likes_count = likes_count %s 1 WHERE id = %d",
        already_liked ? "-" : "+", comment_id);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Update likes_count failed: %s", mysql_error(sql));
        mysql_query(sql, "ROLLBACK");
        return false;
    }

    // 提交事务
    if (mysql_query(sql, "COMMIT")) 
    {
        LOG_ERROR("Commit transaction failed: %s", mysql_error(sql));
        mysql_query(sql, "ROLLBACK");
        return false;
    }

    return true;
}

bool BlogManager::ToggleBlogLike(int blog_id, const std::string& username, bool& is_liked_out, int& likes_count_out) 
{
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection");
        return false;
    }
    
    // 转义用户名
    char escaped_username[256];
    mysql_real_escape_string(sql, escaped_username, username.c_str(), username.length());
    
    // 检查是否已经点赞
    char query[512];
    snprintf(query, sizeof(query),
        "SELECT 1 FROM likes WHERE user_id = '%s' AND blog_id = %d",
        escaped_username, blog_id);
    
    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Check like status failed: %s", mysql_error(sql));
        return false;
    }
    
    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) 
    {
        LOG_ERROR("Store result failed: %s", mysql_error(sql));
        return false;
    }
    
    bool already_liked = mysql_num_rows(res) > 0;
    mysql_free_result(res);
    
    // 根据是否已点赞，执行添加或删除
    if (already_liked) 
    {
        // 取消点赞
        snprintf(query, sizeof(query),
            "DELETE FROM likes WHERE user_id = '%s' AND blog_id = %d",
            escaped_username, blog_id);
        is_liked_out = false;
    } 
    else
    {
        // 添加点赞
        snprintf(query, sizeof(query),
            "INSERT INTO likes(user_id, blog_id) VALUES('%s', %d)",
            escaped_username, blog_id);
        is_liked_out = true;
    }
    
    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Toggle like failed: %s", mysql_error(sql));
        return false;
    }
    
    // 获取最新点赞数
    snprintf(query, sizeof(query),
        "SELECT COUNT(*) FROM likes WHERE blog_id = %d",
        blog_id);
    
    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Get likes count failed: %s", mysql_error(sql));
        return false;
    }
    
    MYSQL_RES* count_res = mysql_store_result(sql);
    if (!count_res) 
    {
        LOG_ERROR("Store result failed: %s", mysql_error(sql));
        return false;
    }
    
    MYSQL_ROW row = mysql_fetch_row(count_res);
    if (row) 
    {
        likes_count_out = row[0] ? atoi(row[0]) : 0;
    }
    mysql_free_result(count_res);
    
    return true;
}

// 检查用户是否已经点赞了文章
bool BlogManager::HasUserLikedBlog(int blog_id, const std::string& username) 
{
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for HasUserLikedBlog");
        return false;
    }

    char escaped_username[128];
    mysql_real_escape_string(sql, escaped_username, username.c_str(), username.length());

    char query[512];
    snprintf(query, sizeof(query),
        "SELECT 1 FROM likes WHERE user_id = '%s' AND blog_id = %d",
        escaped_username, blog_id);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Check blog like status failed: %s", mysql_error(sql));
        return false;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) 
    {
        LOG_ERROR("No result: %s", mysql_error(sql));
        return false;
    }

    bool has_liked = mysql_num_rows(res) > 0;
    mysql_free_result(res);
    return has_liked;
}

// 获取文章的点赞数
int BlogManager::GetBlogLikesCount(int blog_id) 
{
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for GetBlogLikesCount");
        return 0;
    }

    char query[256];
    // 使用实时计算的点赞数，而不是使用 blogs 表中的 likes_count 字段
    snprintf(query, sizeof(query),
        "SELECT COUNT(*) FROM likes WHERE blog_id = %d",
        blog_id);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Get blog likes count failed: %s", mysql_error(sql));
        return 0;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) 
    {
        LOG_ERROR("No result: %s", mysql_error(sql));
        return 0;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    int likes_count = 0;
    if (row && row[0]) 
    {
        likes_count = atoi(row[0]);
    }

    mysql_free_result(res);
    return likes_count;
}

// 检查用户是否可以删除文章
bool BlogManager::CanDeleteBlog(int blog_id, const std::string& username) 
{
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for CanDeleteBlog");
        return false;
    }

    char escaped_username[128];
    mysql_real_escape_string(sql, escaped_username, username.c_str(), username.length());

    char query[256];
    snprintf(query, sizeof(query),
        "SELECT 1 FROM blogs WHERE id = %d AND author = '%s'",
        blog_id, escaped_username);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Check blog permission failed: %s", mysql_error(sql));
        return false;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) 
    {
        LOG_ERROR("No result: %s", mysql_error(sql));
        return false;
    }

    bool can_delete = mysql_num_rows(res) > 0;
    mysql_free_result(res);
    return can_delete;
}

// 删除文章
bool BlogManager::DeleteBlog(int blog_id) 
{
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for DeleteBlog");
        return false;
    }

    // 开始事务
    if (mysql_query(sql, "START TRANSACTION")) 
    {
        LOG_ERROR("Start transaction failed: %s", mysql_error(sql));
        return false;
    }

    // 删除相关的点赞记录
    char query[256];
    snprintf(query, sizeof(query),
        "DELETE FROM likes WHERE blog_id = %d",
        blog_id);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Delete likes failed: %s", mysql_error(sql));
        mysql_query(sql, "ROLLBACK");
        return false;
    }

    // 删除相关的评论
    snprintf(query, sizeof(query),
        "DELETE FROM comments WHERE blog_id = %d",
        blog_id);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Delete comments failed: %s", mysql_error(sql));
        mysql_query(sql, "ROLLBACK");
        return false;
    }

    // 删除文章
    snprintf(query, sizeof(query),
        "DELETE FROM blogs WHERE id = %d",
        blog_id);

    if (mysql_query(sql, query)) 
    {
        LOG_ERROR("Delete blog failed: %s", mysql_error(sql));
        mysql_query(sql, "ROLLBACK");
        return false;
    }

    // 提交事务 - 修复：直接使用 COMMIT 命令而不是之前的 query
    if (mysql_query(sql, "COMMIT")) 
    {
        LOG_ERROR("Commit transaction failed: %s", mysql_error(sql));
        mysql_query(sql, "ROLLBACK");
        return false;
    }

    return true;
}

bool BlogManager::CreateBlogWithFile(
    const std::string& title,
    const std::string& content,
    const std::string& author,
    const std::string& filename,
    const std::string& content_type
) {
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for CreateBlogWithFile");
        return false;
    }

    // 转义所有输入，防止SQL注入
    char* escaped_title = new char[title.length() * 2 + 1];
    char* escaped_author = new char[author.length() * 2 + 1];
    char* escaped_filename = new char[filename.length() * 2 + 1];
    char* escaped_content_type = new char[content_type.length() * 2 + 1];
    char* escaped_content = new char[content.length() * 2 + 1];
    
    mysql_real_escape_string(sql, escaped_title, title.c_str(), title.length());
    mysql_real_escape_string(sql, escaped_author, author.c_str(), author.length());
    mysql_real_escape_string(sql, escaped_filename, filename.c_str(), filename.length());
    mysql_real_escape_string(sql, escaped_content_type, content_type.c_str(), content_type.length());
    mysql_real_escape_string(sql, escaped_content, content.c_str(), content.length());

    // 使用 string 而非固定大小缓冲区
    std::string query = "INSERT INTO blogs (title, content, content_type, original_filename, author) VALUES ('";
    query += escaped_title;
    query += "', '";
    query += escaped_content;
    query += "', '";
    query += escaped_content_type;
    query += "', '";
    query += escaped_filename;
    query += "', '";
    query += escaped_author;
    query += "')";
    
    bool success = (mysql_query(sql, query.c_str()) == 0);
    
    // 释放分配的内存
    delete[] escaped_title;
    delete[] escaped_author;
    delete[] escaped_filename;
    delete[] escaped_content_type;
    delete[] escaped_content;
    
    if (!success) 
    {
        LOG_ERROR("Create blog failed: %s", mysql_error(sql));
        return false;
    }

    return true;
}

bool BlogManager::UpdateUserProfile(
    const std::string& username,
    const std::unordered_map<std::string, std::string>& profileData
) {
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    if (!sql) 
    {
        LOG_ERROR("Failed to get MySQL connection for UpdateUserProfile");
        return false;
    }

    // 转义用户名避免SQL注入
    char escaped_username[128];
    mysql_real_escape_string(sql, escaped_username, username.c_str(), username.length());
    
    // 构建SET子句
    std::string setClause;
    bool isFirstField = true;
    
    // 处理每个可能的字段
    const std::vector<std::string> validFields = 
    {
        "nickname", "email", "avatar_url", "gender", 
        "birth_date", "location", "bio", "website"
    };
    
    for (const auto& field : validFields) 
    {
        auto it = profileData.find(field);
        // 如果提交了该字段且不为空
        if (it != profileData.end() && !it->second.empty()) {
            // 转义字段值
            char escaped_value[1024];
            mysql_real_escape_string(sql, escaped_value, it->second.c_str(), it->second.length());
            
            // 添加到SET子句
            if (!isFirstField) 
            {
                setClause += ", ";
            }
            setClause += field + "='" + escaped_value + "'";
            isFirstField = false;
        }
    }
    
    // 特殊处理avatar字段，它在数据库中名为avatar_url
    auto it = profileData.find("avatar");
    if (it != profileData.end() && !it->second.empty()) 
    {
        char escaped_value[1024];
        mysql_real_escape_string(sql, escaped_value, it->second.c_str(), it->second.length());
        
        if (!isFirstField) {
            setClause += ", ";
        }
        setClause += "avatar_url='" + std::string(escaped_value) + "'";
        isFirstField = false;
    }
    
    // 如果没有有效字段要更新，返回成功
    if (isFirstField) 
    {
        LOG_INFO("No fields to update for user %s", username.c_str());
        return true;
    }
    
    // 构建并执行更新查询
    std::string query = "UPDATE user SET " + setClause + " WHERE username='" + escaped_username + "'";
    
    LOG_DEBUG("Executing query: %s", query.c_str());
    
    if (mysql_query(sql, query.c_str())) 
    {
        LOG_ERROR("Update user profile failed: %s", mysql_error(sql));
        return false;
    }
    
    // 检查是否真的更新了记录
    if (mysql_affected_rows(sql) == 0) 
    {
        LOG_WARN("No rows updated for user %s", username.c_str());
        // 可能用户名不存在或没有实际更改
    }
    
    LOG_INFO("User profile updated: username=%s", username.c_str());
    return true;
}