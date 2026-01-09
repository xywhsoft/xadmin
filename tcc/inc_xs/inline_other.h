


// HTTP 响应（根据 mg_http_reply 修改而来，不会进行 printf 代入）
void http_reply(struct mg_connection* c, int code, const char* headers, const char* sBody, size_t iSize);

// 解析 cookies
xdict ParseCookies(struct mg_http_message* hm);

// 释放 Cookies 表
void FreeCookies(xdict tblCookies);

// 将函数映射到 TCC 执行环境
void ImportAll(TCCState* s);


