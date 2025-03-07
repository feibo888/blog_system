# 基于WebServer的博客系统实现

## 简介

本项目基于my_webserver项目实现：[feibo888/my_webverver: base Webserver](https://github.com/feibo888/my_webverver)

- 基于boost实现JSON解析
- 支持multipart/form-data类型，可处理浏览器对于大型文件分批次上传，支持markdown文件
- 支持GET、POST、DELETE、PUT
- 支持登录、点赞、写博客、搜索、评论等基本功能。
- 可docker一键部署

![image-20250307191321295](./bin/resource/image-20250307191321295.png)

## TODO

关注功能（有接口但还没实现），通知功能（同前）