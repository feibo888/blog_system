//
// Created by fb on 12/27/24.
//

#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H



#include <unordered_map>
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <sys/stat.h>    // stat
#include <sys/mman.h>    // mmap, munmap
#include <sstream>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../blog/blogmanager.h"  // 新增 BlogManager 头文件


class HttpResponse
{
public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string& srcDir, std::string& path, const std::string& method, 
                const std::unordered_map<std::string, std::string>& post,
                const std::unordered_map<std::string, std::string> header,
                bool isKeepAlive = false, int code = -1, bool isDynamic = false);
    void MakeResponse(Buffer& buff);
    void UnmapFile();
    char* File();
    size_t FileLen() const;
    void ErrorContent(Buffer& buff, std::string message);
    int Code() const {return code_;}

private:
    void AddStateLine_(Buffer& buff);
    void AddHeader_(Buffer& buff);
    void AddContent_(Buffer& buff);

    void ErrorHtml_();

    void AddDynamicContent_(Buffer& buff);
    std::string getQueryParam(const std::string& query, const std::string& param);
    std::string GetFileType_();
    std::string getUsernameFromToken();

    int code_;
    bool isKeepAlive_;

    std::string path_;
    std::string srcDir_;
    std::string method_;

    char* mmFile_;
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;
    std::unordered_map<std::string, std::string> post_;
    std::unordered_map<std::string, std::string> header_;

    bool isDynamic_;

    std::string generateToken(const std::string& username);
    std::string EscapeJsonString(const std::string& input);
};





#endif //HTTPRESPONSE_H
