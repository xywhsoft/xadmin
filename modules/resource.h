/*
 * 受控资源读取
 *
 * page 和 template 都位于 wwwroot 之外。xroot 把目录锚定到真实句柄，
 * 后续即使文件名来自请求，也不能通过 ..、绝对路径或链接跳出该目录。
 */



#define RESOURCE_FILE_MAX (4u * 1024u * 1024u)



/* 从已经打开的目录根中读取一个普通文件。
 * 返回缓冲多分配一个结尾 NUL，真实大小通过 pSize 返回；调用方 xrtFree。 */
static bytes RootFileReadAll(xroot Root, const char* sPath, size_t* pSize)
{
	xfileoptions tOptions;
	xfileinfo tInfo;
	xfile File;
	bytes pData = NULL;
	size_t iSize;
	size_t iRead = 0;

	if ( pSize != NULL ) *pSize = 0;
	if ( Root == NULL || sPath == NULL || sPath[0] == '\0' ) {
		return NULL;
	}
	xrtFileOptionsInit(&tOptions);
	tOptions.Flags = XFILE_READ;
	tOptions.Share = XFILE_SHARE_READ;
	File = xrtRootFileOpen(Root, sPath, &tOptions);
	if ( File == NULL ) {
		return NULL;
	}
	if ( !xrtFileStat(File, &tInfo) || tInfo.Type != XFILE_TYPE_FILE ||
	     tInfo.Size > RESOURCE_FILE_MAX ||
	     tInfo.Size > (uint64)((size_t)-1) - 1u ) {
		xrtClose(File);
		return NULL;
	}

	iSize = (size_t)tInfo.Size;
	pData = (bytes)xrtMalloc(iSize + 1u);
	if ( pData == NULL ||
	     (iSize != 0 && !xrtReadFull(File, pData, iSize, &iRead)) ||
	     iRead != iSize ) {
		xrtFree(pData);
		xrtClose(File);
		return NULL;
	}
	pData[iSize] = 0;
	xrtClose(File);
	if ( pSize != NULL ) *pSize = iSize;
	return pData;
}
