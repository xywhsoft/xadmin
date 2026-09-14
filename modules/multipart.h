/* multipart/form-data 整包解析（RFC 2046/7578 务实子集）。
 * 视图全部借用原始正文（正文缓冲为应用私有堆拷贝，参数解码原地展开）；
 * 边界只在行首匹配（CRLF + "--boundary"，首 Part 允许顶格），Part 头只识别
 * Content-Disposition 的 name/filename 参数（quoted-string 含反斜杠转义）。 */

typedef struct {
	const char* name;         /* 字段名（非 NUL 结尾，配合 nameLen） */
	size_t      nameLen;
	const char* filename;     /* 无文件字段时为 NULL */
	size_t      filenameLen;
	const char* data;         /* Part 正文（不含归属边界的 CRLF） */
	size_t      size;
} MultipartPart;

static const char* MultipartMemFind(const char* hay, size_t hayLen, const char* needle, size_t needleLen)
{
	const char* p = hay;
	const char* end;

	if (needleLen == 0 || hayLen < needleLen) return NULL;
	end = hay + hayLen - needleLen;
	while (p <= end) {
		if (*p == needle[0] && memcmp(p, needle, needleLen) == 0) return p;
		p++;
	}
	return NULL;
}

/* 从 Content-Type 提取 boundary（支持 quoted-string），写入 out（NUL 结尾）。 */
static bool MultipartBoundary(const char* contentType, char* out, size_t cap)
{
	const char* p;

	if (!contentType || !out || cap == 0) return false;
	p = strstr(contentType, "boundary=");
	if (!p) return false;
	p += 9;
	if (*p == '"') {
		const char* q = ++p;
		size_t n = 0;
		while (*q && *q != '"' && n + 1 < cap) {
			out[n++] = (*q == '\\' && q[1]) ? *++q : *q;
			q++;
		}
		out[n] = '\0';
		return n > 0;
	}
	{
		size_t n = 0;
		while (*p && *p != ';' && *p != ' ' && *p != '\r' && *p != '\n' && n + 1 < cap)
			out[n++] = *p++;
		out[n] = '\0';
		return n > 0;
	}
}

/* 从 Content-Disposition 参数中取 name="..." / filename="..."（含转义还原）。 */
static bool MultipartParam(const char* header, size_t headerLen, const char* key,
	const char** outValue, size_t* outLen)
{
	size_t keyLen = strlen(key);
	size_t i;

	for (i = 0; i + keyLen + 1 < headerLen; i++) {
		if ((i == 0 || header[i - 1] == ';' || header[i - 1] == ' ' || header[i - 1] == '\t')
			&& memcmp(header + i, key, keyLen) == 0 && header[i + keyLen] == '=') {
			const char* v = header + i + keyLen + 1;
			const char* end = header + headerLen;
			if (v < end && *v == '"') {
				const char* q = ++v;
				char* write = (char*)v;
				while (q < end && *q != '"') {
					if (*q == '\\' && q + 1 < end) q++;
					*write++ = *q++;
				}
				*outValue = v;
				*outLen = (size_t)(write - v);
				return true;
			}
			{
				const char* q = v;
				while (q < end && *q != ';' && *q != '\r' && *q != '\n' && *q != ' ' && *q != '\t') q++;
				*outValue = v;
				*outLen = (size_t)(q - v);
				return *outLen > 0;
			}
		}
	}
	return false;
}

/* 迭代 Part：*offset 初始 0，指向当前 Part 的前导边界（首 Part 顶格，
 * 后续为 CRLF + "--boundary" 的 CR 处）。返回 false 表示结束或格式错误。 */
static bool MultipartNext(const char* body, size_t bodySize, const char* boundary,
	size_t boundaryLen, size_t* offset, MultipartPart* out)
{
	const char* end = body + bodySize;
	const char* pos;
	const char* headers;
	const char* bodyStart;
	const char* hit;
	char needle[76];
	size_t needleLen = boundaryLen + 4;

	if (!body || !boundary || boundaryLen == 0 || boundaryLen > 70 || !offset || !out)
		return false;
	memset(out, 0, sizeof(*out));

	/* 消费前导边界 */
	if (*offset == 0) {
		if (bodySize < 2 + boundaryLen || body[0] != '-' || body[1] != '-'
			|| memcmp(body + 2, boundary, boundaryLen) != 0)
			return false;
		pos = body + 2 + boundaryLen;
	} else {
		if (bodySize - *offset < needleLen || memcmp(body + *offset, "\r\n--", 4) != 0
			|| memcmp(body + *offset + 4, boundary, boundaryLen) != 0)
			return false;
		pos = body + *offset + needleLen;
	}
	/* transport padding 之后必须是行结束或关闭边界 */
	while (pos < end && (*pos == ' ' || *pos == '\t')) pos++;
	if (end - pos >= 2 && pos[0] == '-' && pos[1] == '-')
		return false;
	if (end - pos >= 2 && pos[0] == '\r' && pos[1] == '\n') pos += 2;
	else if (pos < end && pos[0] == '\n') pos++;
	else return false;
	headers = pos;

	/* Part 头以空行结束：行首即 CRLF/LF 为空行，否则整行跳过 */
	bodyStart = headers;
	while (bodyStart < end) {
		if (end - bodyStart >= 2 && bodyStart[0] == '\r' && bodyStart[1] == '\n') { bodyStart += 2; break; }
		if (bodyStart[0] == '\n') { bodyStart++; break; }
		while (bodyStart < end && bodyStart[0] != '\n') bodyStart++;
		if (bodyStart < end) bodyStart++;
	}
	if (bodyStart >= end) return false;

	/* 解析 Content-Disposition 行 */
	{
		const char* hscan = headers;
		while (hscan < bodyStart) {
			const char* eol = hscan;
			while (eol < bodyStart && *eol != '\r' && *eol != '\n') eol++;
			if ((size_t)(eol - hscan) > 20
				&& xrtStrCaseCompare(xrtStrViewN(hscan, 20), xrtStrView("Content-Disposition:")) == 0) {
				MultipartParam(hscan + 20, (size_t)(eol - hscan - 20), "name", &out->name, &out->nameLen);
				MultipartParam(hscan + 20, (size_t)(eol - hscan - 20), "filename", &out->filename, &out->filenameLen);
			}
			if (eol >= bodyStart) break;
			if (*eol == '\r')
				hscan = (eol + 1 < bodyStart && eol[1] == '\n') ? eol + 2 : eol + 1;
			else
				hscan = eol + 1;
		}
	}

	/* 下一个行首边界即本 Part 正文终点；"\r\n--boundary" 后跟其他字符的是
	 * 正文伪命中（如 boundary 恰为正文前缀），须校验分隔符后缀再采纳。 */
	needle[0] = '\r'; needle[1] = '\n'; needle[2] = '-'; needle[3] = '-';
	memcpy(needle + 4, boundary, boundaryLen);
	{
		const char* scan = bodyStart;
		hit = NULL;
		for (;;) {
			const char* q;
			hit = MultipartMemFind(scan, (size_t)(end - scan), needle, needleLen);
			if (!hit) return false;
			q = hit + needleLen;
			while (q < end && (*q == ' ' || *q == '\t')) q++;
			if (q >= end
				|| (end - q >= 2 && q[0] == '\r' && q[1] == '\n')
				|| (end - q >= 2 && q[0] == '-' && q[1] == '-')
				|| q[0] == '\n')
				break;
			scan = hit + 1;
		}
	}
	out->data = bodyStart;
	out->size = (size_t)(hit - bodyStart);
	*offset = (size_t)(hit - body);
	return true;
}

/* Part 字段名匹配（name 为 NUL 结尾的比较键）。 */
static bool MultipartNameIs(const MultipartPart* part, const char* name)
{
	size_t n = strlen(name);
	return part->name && part->nameLen == n && memcmp(part->name, name, n) == 0;
}
