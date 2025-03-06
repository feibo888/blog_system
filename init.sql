DROP DATABASE IF EXISTS blog_data;
CREATE DATABASE blog_data;
USE blog_data;

-- 用户表
CREATE TABLE user (
    username VARCHAR(50) PRIMARY KEY,
    password VARCHAR(100) NOT NULL,
    email VARCHAR(100) UNIQUE,
    nickname VARCHAR(50),
    avatar_url VARCHAR(200),
    gender ENUM('male', 'female', 'other'),
    birth_date DATE,
    location VARCHAR(100),
    bio TEXT,
    website VARCHAR(200),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

-- 博客文章表
CREATE TABLE blogs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    title VARCHAR(100) NOT NULL,
    content TEXT NOT NULL,
    content_type ENUM('text', 'markdown') DEFAULT 'text',  -- 区分普通文本和markdown
    original_filename VARCHAR(255),  -- 原始文件名
    author VARCHAR(50) NOT NULL,
    views INT DEFAULT 0,  -- 浏览量
    likes_count INT DEFAULT 0,  -- 点赞数
    comments_count INT DEFAULT 0,  -- 评论数
    status ENUM('draft', 'published', 'archived') DEFAULT 'published',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (author) REFERENCES user(username) ON DELETE CASCADE
);

-- 评论表
CREATE TABLE comments (
    id INT AUTO_INCREMENT PRIMARY KEY,
    blog_id INT NOT NULL,
    user_id VARCHAR(50) NOT NULL,
    content TEXT NOT NULL,
    parent_id INT,  -- 用于回复功能，指向父评论
    likes_count INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (blog_id) REFERENCES blogs(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES user(username) ON DELETE CASCADE,
    FOREIGN KEY (parent_id) REFERENCES comments(id) ON DELETE SET NULL
);

-- 点赞表
CREATE TABLE likes (
    user_id VARCHAR(50) NOT NULL,
    blog_id INT,  -- 文章ID，可以为空
    comment_id INT,  -- 评论ID，可以为空
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES user(username) ON DELETE CASCADE,
    FOREIGN KEY (blog_id) REFERENCES blogs(id) ON DELETE CASCADE,
    FOREIGN KEY (comment_id) REFERENCES comments(id) ON DELETE CASCADE,
    UNIQUE KEY `unique_blog_like` (user_id, blog_id),
    UNIQUE KEY `unique_comment_like` (user_id, comment_id),
    CHECK (blog_id IS NOT NULL OR comment_id IS NOT NULL)  -- 确保至少有一个不为空
);

-- 如果还没有这个索引，请添加
CREATE INDEX idx_likes_blog_id ON likes(blog_id);

-- 标签表
CREATE TABLE tags (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(50) NOT NULL UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 文章标签关联表
CREATE TABLE blog_tags (
    blog_id INT NOT NULL,
    tag_id INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (blog_id, tag_id),
    FOREIGN KEY (blog_id) REFERENCES blogs(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
);

-- 用户关注表
CREATE TABLE follows (
    follower_id VARCHAR(50) NOT NULL,
    following_id VARCHAR(50) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (follower_id, following_id),
    FOREIGN KEY (follower_id) REFERENCES user(username) ON DELETE CASCADE,
    FOREIGN KEY (following_id) REFERENCES user(username) ON DELETE CASCADE
);

-- 用户收藏表
CREATE TABLE favorites (
    user_id VARCHAR(50) NOT NULL,
    blog_id INT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, blog_id),
    FOREIGN KEY (user_id) REFERENCES user(username) ON DELETE CASCADE,
    FOREIGN KEY (blog_id) REFERENCES blogs(id) ON DELETE CASCADE
);

-- 通知表
CREATE TABLE notifications (
    id INT AUTO_INCREMENT PRIMARY KEY,
    user_id VARCHAR(50) NOT NULL,
    type ENUM('like', 'comment', 'follow', 'mention') NOT NULL,
    content TEXT NOT NULL,
    is_read BOOLEAN DEFAULT FALSE,
    related_blog_id INT,
    related_comment_id INT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES user(username) ON DELETE CASCADE,
    FOREIGN KEY (related_blog_id) REFERENCES blogs(id) ON DELETE SET NULL,
    FOREIGN KEY (related_comment_id) REFERENCES comments(id) ON DELETE SET NULL
);

-- 添加测试数据
INSERT INTO user (username, password, email, nickname, gender, bio) VALUES 
('admin', 'admin123', 'admin@example.com', '管理员', 'male', '网站管理员'),
('fb', 'fb123', 'fb@example.com', 'FB', 'male', '热爱技术的程序员');

INSERT INTO blogs (title, content, author) VALUES 
('第一篇博客', '这是我的第一篇博客内容', 'admin'),
('测试文章', '这是一篇测试文章的内容', 'fb');