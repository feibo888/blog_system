// 获取文章列表
function loadPosts() {
    // 确保 DOM 已加载
    if (!document.getElementById('post-list')) {
        setTimeout(loadPosts, 100);  // 如果元素不存在，等待100ms后重试
        return;
    }

    fetch('/blogs/getall')
        .then(response => response.json())
        .then(posts => {
            console.log('Received posts:', posts);
            const list = document.getElementById('post-list');
            if (!list) {
                console.error('post-list element not found');
                return;
            }
            
            list.innerHTML = '';
            posts.forEach(post => {
                const div = document.createElement('div');
                div.className = 'post-item';
                
                const titleDiv = document.createElement('div');
                titleDiv.className = 'post-title';
                titleDiv.textContent = post.title || '无标题';
                
                const metaDiv = document.createElement('div');
                metaDiv.className = 'post-meta';
                metaDiv.innerHTML = `
                    <span class="author">作者：${post.author}</span>
                    <span class="date">发布于：${formatDate(post.created_at)}</span>
                `;
                
                const statsDiv = document.createElement('div');
                statsDiv.className = 'post-stats';
                statsDiv.innerHTML = `
                    <span><i class="icon-eye"></i>${post.views || 0}</span>
                    <span><i class="icon-heart"></i>${post.likes_count || 0}</span>
                    <span><i class="icon-comment"></i>${post.comments_count || 0}</span>
                `;
                
                div.onclick = () => window.location.href = `post.html#${post.id}`;
                
                div.appendChild(titleDiv);
                div.appendChild(metaDiv);
                div.appendChild(statsDiv);
                
                list.appendChild(div);
            });
        })
        .catch(error => {
            console.error('加载文章失败:', error);
        });
}

// 初始调用
loadPosts();

// 添加日期格式化函数
function formatDate(dateStr) {
    const date = new Date(dateStr);
    const now = new Date();
    const diff = now - date;
    
    // 小于1分钟
    if (diff < 60000) {
        return '刚刚';
    }
    // 小于1小时
    if (diff < 3600000) {
        return `${Math.floor(diff / 60000)} 分钟前`;
    }
    // 小于24小时
    if (diff < 86400000) {
        return `${Math.floor(diff / 3600000)} 小时前`;
    }
    // 小于30天
    if (diff < 2592000000) {
        return `${Math.floor(diff / 86400000)} 天前`;
    }
    
    // 超过30天显示具体日期
    return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;
}

// 获取单篇文章
function loadPost(id) {
    const token = localStorage.getItem('token');
    fetch(`/posts/${id}`, {
        headers: {
            'Authorization': `Bearer ${token}`
        }
    })
    .then(response => response.json())
    .then(post => {
        document.getElementById('post-title').textContent = post.title;
        document.getElementById('post-content').textContent = post.content;
        document.getElementById('post-meta').innerHTML = `
            作者：${post.author} | 
            发布时间：${formatDate(post.created_at)} | 
            ${post.views || 0} 次浏览
        `;
        
        // 更新点赞按钮和点赞数
        const likeBtn = document.getElementById('like-btn');
        const likesCount = document.querySelector('.likes-count');
        likesCount.textContent = post.likes_count || 0;
        
        // 根据用户是否已点赞设置按钮状态
        if (post.is_liked) {
            likeBtn.classList.add('liked');
            likeBtn.textContent = '取消点赞';
        } else {
            likeBtn.classList.remove('liked');
            likeBtn.textContent = '点赞';
        }

        // 检查是否是作者并处理编辑控件
        const editControls = document.querySelector('.edit-controls');
        if (!editControls) return;

        if (localStorage.getItem('username') === post.author) {
            editControls.style.display = 'inline-block';
            // 使用 innerHTML 重置编辑控件内容
            editControls.innerHTML = `
                <button id="edit-btn">编辑</button>
                <button id="delete-btn">删除</button>
            `;
            
            // 使用一次性事件绑定
            const deleteBtn = document.getElementById('delete-btn');
            if (deleteBtn) {
                // 移除可能存在的旧事件监听器
                const newDeleteBtn = deleteBtn.cloneNode(true);
                deleteBtn.parentNode.replaceChild(newDeleteBtn, deleteBtn);
                // 添加新的事件监听器
                newDeleteBtn.onclick = () => {
                    console.log('Delete button clicked for post:', id); // 添加调试日志
                    deletePost(id);
                };
            }

            const editBtn = document.getElementById('edit-btn');
            if (editBtn) {
                const newEditBtn = editBtn.cloneNode(true);
                editBtn.parentNode.replaceChild(newEditBtn, editBtn);
                newEditBtn.onclick = () => editPost(id);
            }
        } else {
            editControls.style.display = 'none';
        }
    })
    .catch(error => console.error('加载文章失败:', error));
}

// 添加登录状态检查函数
function checkLoginStatus() {
    const token = localStorage.getItem('token');
    if (!token) {
        alert('请先登录！');
        window.location.href = 'login.html';
        return false;
    }
    return true;
}

// 添加通用的退出登录函数
function logout() {
    localStorage.removeItem('token');
    localStorage.removeItem('username');
    alert('已退出登录！');
    window.location.href = 'index.html';
}

// 创建文章函数
function createPost(event) {
    event.preventDefault();
    
    if (!checkLoginStatus()) {
        alert('请先登录！');
        return;
    }

    const title = document.getElementById('title').value.trim();
    const content = document.getElementById('content').value.trim();

    if (!title || !content) {
        alert('标题和内容不能为空！');
        return;
    }

    const token = localStorage.getItem('token');
    const formData = new URLSearchParams();
    formData.append('title', title);
    formData.append('content', content);

    fetch('/blogs/create', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
            'Authorization': `Bearer ${token}`
        },
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert('文章发布成功！');
            window.location.href = 'index.html';  // 发布成功后跳转到首页
        } else {
            alert(data.message || '发布失败，请稍后重试');
        }
    })
    .catch(error => {
        console.error('发布文章失败:', error);
        alert('发布失败，请稍后重试');
    });
}

// 编辑文章
function editPost(id) {
    if (!checkLoginStatus()) return;
    // 跳转到编辑页面
    window.location.href = `edit.html#${id}`;
}

// 删除文章
function deletePost(id) {
    console.log('deletePost called with id:', id); // 添加调试日志

    // 防止重复点击
    const deleteBtn = document.getElementById('delete-btn');
    if (deleteBtn) {
        if (deleteBtn.disabled) {
            console.log('Delete button is already disabled'); // 添加调试日志
            return; // 如果按钮已经禁用，直接返回
        }
        deleteBtn.disabled = true;
    }

    if (!checkLoginStatus()) {
        if (deleteBtn) deleteBtn.disabled = false;
        return;
    }
    
    if (confirm('确定删除这篇文章吗？')) {
        const token = localStorage.getItem('token');
        fetch(`/blogs/${id}`, {
            method: 'DELETE',
            headers: {
                'Authorization': `Bearer ${token}`
            }
        })
        .then(response => response.json())
        .then(data => {
            console.log('Delete response:', data); // 添加调试日志
            if (data.success) {
                alert('文章已删除');
                window.location.href = 'index.html';
                return; // 确保后续代码不执行
            } else {
                alert(data.message || '删除失败');
                if (deleteBtn) deleteBtn.disabled = false;
            }
        })
        .catch(error => {
            console.error('删除文章失败:', error);
            alert('删除失败，请稍后重试');
            if (deleteBtn) deleteBtn.disabled = false;
        });
    } else {
        if (deleteBtn) deleteBtn.disabled = false;
    }
}

// 获取评论
function loadComments(postId) {
    fetch(`/comments/${postId}`)
        .then(response => response.json())
        .then(comments => {
            const list = document.getElementById('comment-list');
            list.innerHTML = '';
            
            // 创建评论树结构
            const commentMap = new Map();
            const rootComments = [];
            
            // 首先将所有评论放入 Map 中
            comments.forEach(comment => {
                comment.replies = [];
                commentMap.set(comment.id, comment);
            });
            
            // 构建评论树
            comments.forEach(comment => {
                if (comment.parent_id) {
                    const parentComment = commentMap.get(comment.parent_id);
                    if (parentComment) {
                        parentComment.replies.push(comment);
                    }
                } else {
                    rootComments.push(comment);
                }
            });
            
            // 渲染评论树
            function renderComment(comment, level = 0) {
                const div = document.createElement('div');
                div.className = 'comment-item';
                div.style.marginLeft = `${level * 20}px`;
                
                div.innerHTML = `
                    <div class="comment-content">
                        <p>${comment.content}</p>
                        <div class="comment-meta">
                            <span class="comment-author">${comment.user_id}</span>
                            <span class="comment-date">${formatDate(comment.created_at)}</span>
                            <span class="comment-likes">${comment.likes_count || 0} 赞</span>
                        </div>
                        <div class="comment-actions">
                            <button onclick="replyToComment(${comment.id}, '${comment.user_id}')">回复</button>
                            ${comment.user_id === localStorage.getItem('username') ? 
                                `<button onclick="deleteComment(${comment.id}, ${postId})">删除</button>` : 
                                ''}
                            <button onclick="toggleCommentLike(${comment.id})">点赞</button>
                        </div>
                    </div>
                `;
                
                list.appendChild(div);
                
                // 递归渲染回复
                comment.replies.forEach(reply => {
                    renderComment(reply, level + 1);
                });
            }
            
            // 渲染所有根评论
            rootComments.forEach(comment => renderComment(comment));
            
            // 添加评论输入框
            const replyForm = document.createElement('div');
            replyForm.id = 'reply-form';
            replyForm.style.display = 'none';
            replyForm.innerHTML = `
                <div class="reply-to"></div>
                <textarea id="reply-content"></textarea>
                <button onclick="submitReply()">发送回复</button>
                <button onclick="cancelReply()">取消</button>
            `;
            list.appendChild(replyForm);
        })
        .catch(error => console.error('加载评论失败:', error));
}

// 创建评论
function createComment(event, postId) {
    if (!checkLoginStatus()) return;
    
    event.preventDefault();
    const content = document.getElementById('comment-content').value;
    const token = localStorage.getItem('token');
    const username = localStorage.getItem('username');
    
    fetch('/comments', {
        method: 'POST',
        headers: { 
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${token}`
        },
        body: JSON.stringify({ 
            blog_id: postId, 
            user_id: username,
            content,
            parent_id: null // 新评论没有父评论
        })
    })
    .then(response => response.json())
    .then(() => {
        document.getElementById('comment-content').value = '';
        loadComments(postId);
    })
    .catch(error => console.error('创建评论失败:', error));
}

// 回复评论
function replyToComment(parentId, parentAuthor) {
    if (!checkLoginStatus()) return;
    
    const replyForm = document.getElementById('reply-form');
    replyForm.style.display = 'block';
    replyForm.dataset.parentId = parentId;
    document.querySelector('#reply-form .reply-to').textContent = `回复 @${parentAuthor}：`;
    document.getElementById('reply-content').focus();
}

// 提交回复
function submitReply() {
    if (!checkLoginStatus()) return;
    
    const replyForm = document.getElementById('reply-form');
    const parentId = replyForm.dataset.parentId;
    const content = document.getElementById('reply-content').value;
    const token = localStorage.getItem('token');
    const username = localStorage.getItem('username');
    const postId = window.location.hash.substring(1); // 从URL获取文章ID
    
    fetch('/comments', {
        method: 'POST',
        headers: { 
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${token}`
        },
        body: JSON.stringify({ 
            blog_id: postId, 
            user_id: username,
            content,
            parent_id: parentId
        })
    })
    .then(response => response.json())
    .then(() => {
        cancelReply();
        loadComments(postId);
    })
    .catch(error => console.error('回复评论失败:', error));
}

// 取消回复
function cancelReply() {
    const replyForm = document.getElementById('reply-form');
    replyForm.style.display = 'none';
    replyForm.dataset.parentId = '';
    document.getElementById('reply-content').value = '';
    document.querySelector('#reply-form .reply-to').textContent = '';
}

// 删除评论
function deleteComment(id, postId) {
    if (!checkLoginStatus()) return;
    
    if (confirm('确定要删除这条评论吗？')) {
        const token = localStorage.getItem('token');
        fetch(`/comments/${id}`, { 
            method: 'DELETE',
            headers: {
                'Authorization': `Bearer ${token}`
            }
        })
        .then(response => response.json())
        .then(() => loadComments(postId))
        .catch(error => console.error('删除评论失败:', error));
    }
}

// 评论点赞
function toggleCommentLike(commentId) {
    if (!checkLoginStatus()) return;
    
    const token = localStorage.getItem('token');
    fetch(`/likes/comment/${commentId}`, {
        method: 'POST',
        headers: {
            'Authorization': `Bearer ${token}`
        }
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            // 重新加载评论以更新点赞数
            const postId = window.location.hash.substring(1);
            loadComments(postId);
        }
    })
    .catch(error => console.error('评论点赞失败:', error));
}

// 在script.js中修改点赞函数，增加超时处理
function toggleLike(id) {
    // 显示加载指示器
    const likeBtn = document.getElementById('like-btn');
    const originalText = likeBtn.textContent;
    likeBtn.textContent = '处理中...';
    likeBtn.disabled = true;
    
    // 设置超时
    const timeout = setTimeout(() => {
        likeBtn.textContent = originalText;
        likeBtn.disabled = false;
        alert('点赞操作超时，请稍后再试');
    }, 5000); // 5秒超时
    
    fetch(`/likes/${id}`, {
        method: 'POST',
        headers: {
            'Authorization': `Bearer ${localStorage.getItem('token')}`,
            'Content-Length': '0',  // 明确指定内容长度为0
            'Content-Type': 'application/x-www-form-urlencoded'  // 指定内容类型
        }
    })
    .then(response => response.json())
    .then(data => {
        clearTimeout(timeout); // 取消超时
        likeBtn.disabled = false;
        
        if (data.success) {
            document.querySelector('.likes-count').textContent = data.likes_count;
            if (data.liked) {
                likeBtn.classList.add('liked');
                likeBtn.textContent = '取消点赞';
            } else {
                likeBtn.classList.remove('liked');
                likeBtn.textContent = '点赞';
            }
        } else {
            likeBtn.textContent = originalText;
            alert('操作失败: ' + data.message);
        }
    })
    .catch(error => {
        clearTimeout(timeout); // 取消超时
        likeBtn.textContent = originalText;
        likeBtn.disabled = false;
        console.error('Error:', error);
    });
}

// 注册用户
function registerUser(event) {
    event.preventDefault();
    const username = document.getElementById('username').value;
    const password = document.getElementById('password').value;
    const confirmPassword = document.getElementById('confirm-password').value;

    // 使用 URLSearchParams 生成表单格式的请求体
    const formData = new URLSearchParams();
    formData.append('username', username);
    formData.append('password', password);

    if (password !== confirmPassword) {
        document.getElementById('error-message').textContent = '密码不一致';
        return;
    }

    fetch('/register', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: formData
        //body: JSON.stringify({ username, password })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert('注册成功！');
            window.location.href = 'login.html';
        } else {
            document.getElementById('error-message').textContent = data.message;
        }
    })
    .catch(error => {
        document.getElementById('error-message').textContent = '注册失败，请稍后重试';
        console.error('注册错误:', error);
    });
}

// 登录用户（保持不变，更新 token 和 username 存储）
function loginUser(event) {
    event.preventDefault();
    const username = document.getElementById('username').value;
    const password = document.getElementById('password').value;

    const formData = new URLSearchParams();
    formData.append('username', username);
    formData.append('password', password);

    fetch('/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: formData 
    })
    .then(response => {
        if (!response.ok) {
            return response.text().then(text => {
                throw new Error(`HTTP error! status: ${response.status}, body: ${text}`);
            });
        }
        return response.text().then(text => {
            if (!text || text.trim() === '') {
                throw new Error('收到空响应');
            }
            console.log('Response text:', text);
            return JSON.parse(text);
        });
    })
    .then(data => {
        if (data.success) {
            localStorage.setItem('token', data.token);
            localStorage.setItem('username', username);
            alert('登录成功！');
            window.location.href = 'index.html'; // 修改为跳转到主页
        } else {
            document.getElementById('error-message').textContent = data.message || '登录失败';
        }
    })
    .catch(error => {
        document.getElementById('error-message').textContent = '登录失败，请稍后重试: ' + error.message;
        console.error('登录错误:', error);
    });
}

// 修改用户头像点击事件处理
document.addEventListener('DOMContentLoaded', () => {
    const userProfile = document.getElementById('user-profile');
    if (userProfile) {
        userProfile.addEventListener('click', (e) => {
            // 如果点击的是退出按钮，不进行跳转
            if (e.target.id !== 'logout-btn') {
                window.location.href = 'dashboard.html';
            }
        });
    }
});

// 处理 Markdown 文件上传
function handleMarkdownUpload(event) {
    event.preventDefault();
    
    if (!checkLoginStatus()) {
        alert('请先登录！');
        return;
    }

    const title = document.getElementById('title').value.trim();
    const file = document.getElementById('markdown-file').files[0];
    
    if (!title || !file) {
        alert('请填写标题并选择文件！');
        return;
    }

    const formData = new FormData();
    formData.append('title', title);
    formData.append('file', file);
    formData.append('content_type', 'markdown');

    const token = localStorage.getItem('token');
    fetch('/blogs/upload', {
        method: 'POST',
        headers: {
            'Authorization': `Bearer ${token}`
        },
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            alert('文章上传成功！');
            window.location.href = 'index.html';
        } else {
            alert(data.message || '上传失败');
        }
    })
    .catch(error => {
        console.error('上传失败:', error);
        alert('上传失败，请稍后重试');
    });
}