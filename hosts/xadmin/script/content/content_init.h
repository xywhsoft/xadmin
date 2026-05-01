#ifndef XADMIN_CONTENT_INIT_H
#define XADMIN_CONTENT_INIT_H

#include "content_db.h"
#include "content_capability.h"

bool Content_EnsureMenu()
{
	sqlite3_stmt* stmt = NULL;
	int iMenuId = 0;
	int iNow = (int)xrtNow();

	if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE isDelete = 0 AND href = '/admin/view/content' LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iMenuId = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}

	if ( iMenuId > 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE menu SET title='内容模型', icon='layui-icon layui-icon-template-1', type=1, openType='_component', visible=1, remark='内容模型与业务插件生成器', updateTime=? WHERE id=?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return FALSE;
		}
		sqlite3_bind_int(stmt, 1, iNow);
		sqlite3_bind_int(stmt, 2, iMenuId);
	} else {
		if ( sqlite3_prepare_v3(G_DB, "INSERT INTO menu (parent,title,icon,type,openType,href,sort,visible,remark,createTime,updateTime,isDelete) VALUES (0,'内容模型','layui-icon layui-icon-template-1',1,'_component','/admin/view/content',540100,1,'内容模型与业务插件生成器',?,?,0)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return FALSE;
		}
		sqlite3_bind_int(stmt, 1, iNow);
		sqlite3_bind_int(stmt, 2, iNow);
	}

	bool bOK = (sqlite3_step(stmt) == SQLITE_DONE);
	sqlite3_finalize(stmt);
	return bOK;
}

bool Content_Init()
{
	printf("[xadmin:init] Content_Init begin\n");
	fflush(stdout);

	if ( !ContentDB_Init() ) {
		printf("[content] database init failed\n");
		return FALSE;
	}
	if ( !ContentCapability_InitBuiltins() ) {
		printf("[content] capability init failed\n");
		return FALSE;
	}
	if ( !Content_EnsureMenu() ) {
		printf("[content] menu init failed\n");
		return FALSE;
	}

	printf("[xadmin:init] Content_Init done\n");
	fflush(stdout);
	return TRUE;
}

#endif
