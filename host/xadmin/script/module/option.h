



// 扫描配置文件的回调函数
int ScanOptionFileProc(str sPath, size_t iSize, int bDir, ptr pData, size_t iPathSize)
{
	if ( bDir == 0 ) {
		// 只处理 .json 文件
		str sExt = xrtPathGetExt(sPath, 0);
		if ( (sExt != NULL) && (xrtStrComp(sExt, "json", 4, FALSE) == 0) ) {
			// 解析 JSON 文件
			xvalue tblConfig = xrtParseJSON_File(sPath);
			if ( tblConfig != NULL ) {
				// 获取 namespace
				str sNamespace = xvoTableGetText(tblConfig, "namespace", 9);
				if ( sNamespace != NULL ) {
					// 创建 namespace 子表
					xvalue tblNamespace = xvoTableGetValue(G_Option, sNamespace, 0);
					if ( tblNamespace == NULL ) {
						tblNamespace = xvoCreateTable();
						xvoTableSetValue(G_Option, sNamespace, 0, tblNamespace, TRUE);
					}
					
					// 遍历 classList
					xvalue arrClassList = xvoTableGetValue(tblConfig, "classList", 9);
					if ( arrClassList != NULL ) {
						uint32 iClassCount = xvoArrayItemCount(arrClassList);
						for ( uint32 i = 0; i < iClassCount; i++ ) {
							xvalue tblClass = xvoArrayGetValue(arrClassList, i);
							if ( tblClass != NULL ) {
								// 遍历 options
								xvalue arrOptions = xvoTableGetValue(tblClass, "options", 7);
								if ( arrOptions != NULL ) {
									uint32 iOptCount = xvoArrayItemCount(arrOptions);
									for ( uint32 j = 0; j < iOptCount; j++ ) {
										xvalue tblOpt = xvoArrayGetValue(arrOptions, j);
										if ( tblOpt != NULL ) {
											str sName = xvoTableGetText(tblOpt, "name", 4);
											xvalue varValue = xvoTableGetValue(tblOpt, "value", 5);
											if ( sName != NULL ) {
												// 将配置值存入 G_Option[namespace][name]
												if ( varValue != NULL ) {
													xvoAddRef(varValue);
													xvoTableSetValue(tblNamespace, sName, 0, varValue, TRUE);
												} else {
													// 没有 value 字段时设置为空字符串
													xvoTableSetText(tblNamespace, sName, 0, "", 0, FALSE);
												}
											}
										}
									}
								}
							}
						}
					}
				}
				xvoUnref(tblConfig);
			} else {
				printf("!!! ERROR !!! Option_Init - Failed to parse config file: %s\n", sPath);
			}
		}
		xrtFree(sExt);
	}
	return FALSE;
}



// 加载指定配置文件，返回完整的配置结构（用于页面渲染）
xvalue Option_LoadFile(str sFileName)
{
	str sFilePath = xrtPathJoin(2, OptionPath, sFileName);
	xvalue tblConfig = xrtParseJSON_File(sFilePath);
	xrtFree(sFilePath);
	return tblConfig;
}



// 保存配置文件
bool Option_SaveFile(str sFileName, xvalue tblFormData)
{
	str sFilePath = xrtPathJoin(2, OptionPath, sFileName);
	
	// 读取原配置文件
	xvalue tblConfig = xrtParseJSON_File(sFilePath);
	if ( tblConfig == NULL ) {
		xrtFree(sFilePath);
		return FALSE;
	}
	
	// 获取 namespace
	str sNamespace = xvoTableGetText(tblConfig, "namespace", 9);
	xvalue tblNamespace = NULL;
	if ( sNamespace != NULL ) {
		tblNamespace = xvoTableGetValue(G_Option, sNamespace, 0);
	}
	
	// 遍历 classList，更新 value 字段
	xvalue arrClassList = xvoTableGetValue(tblConfig, "classList", 9);
	if ( arrClassList != NULL ) {
		uint32 iClassCount = xvoArrayItemCount(arrClassList);
		for ( uint32 i = 0; i < iClassCount; i++ ) {
			xvalue tblClass = xvoArrayGetValue(arrClassList, i);
			if ( tblClass != NULL ) {
				xvalue arrOptions = xvoTableGetValue(tblClass, "options", 7);
				if ( arrOptions != NULL ) {
					uint32 iOptCount = xvoArrayItemCount(arrOptions);
					for ( uint32 j = 0; j < iOptCount; j++ ) {
						xvalue tblOpt = xvoArrayGetValue(arrOptions, j);
						if ( tblOpt != NULL ) {
							str sName = xvoTableGetText(tblOpt, "name", 4);
							if ( sName != NULL ) {
								// 从表单数据中获取新值
								xvalue varNewValue = xvoTableGetValue(tblFormData, sName, 0);
								if ( varNewValue != NULL ) {
									// 更新配置文件中的 value
									xvoAddRef(varNewValue);
									xvoTableSetValue(tblOpt, "value", 5, varNewValue, TRUE);
									
									// 同时更新 G_Option
									if ( tblNamespace != NULL ) {
										xvoAddRef(varNewValue);
										xvoTableSetValue(tblNamespace, sName, 0, varNewValue, TRUE);
									}
								}
							}
						}
					}
				}
			}
		}
	}
	
	// 写回文件
	int iRet = xrtStringifyJSON_File(sFilePath, tblConfig, TRUE);
	
	xvoUnref(tblConfig);
	xrtFree(sFilePath);
	
	return iRet;
}



// 初始化配置模块
void Option_Init()
{
	printf("        Option_Init \n");
	
	// 创建全局配置表
	G_Option = xvoCreateTable();
	
	// 扫描配置目录，加载所有配置文件
	xrtDirScan(OptionPath, FALSE, ScanOptionFileProc, NULL);
}



// 卸载配置模块
void Option_Unit()
{
	printf("        Option_Unit \n");
	
	// 释放全局配置表
	if ( G_Option != NULL ) {
		xvoUnref(G_Option);
		G_Option = NULL;
	}
}


