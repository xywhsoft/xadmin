#ifndef XADMIN_CONTENT_INIT_H
#define XADMIN_CONTENT_INIT_H

#include "content_db.h"
#include "content_pack.h"

bool Content_EnsureMenuItem(int iParent, const char* sTitle, const char* sHref, const char* sIcon, int iSort, const char* sRemark)
{
	sqlite3_stmt* stmt = NULL;
	int iMenuId = 0;
	int iNow = (int)xrtNow();
	bool bOK;

	if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE isDelete = 0 AND href = ? LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sHref ? sHref : "", -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iMenuId = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}

	if ( iMenuId > 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE menu SET parent=?,title=?,icon=?,type=1,openType='_component',href=?,sort=?,visible=1,remark=?,updateTime=? WHERE id=?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return FALSE;
		}
		sqlite3_bind_int(stmt, 1, iParent);
		sqlite3_bind_text(stmt, 2, sTitle, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sIcon ? sIcon : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sHref, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 5, iSort);
		sqlite3_bind_text(stmt, 6, sRemark ? sRemark : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 7, iNow);
		sqlite3_bind_int(stmt, 8, iMenuId);
	} else {
		if ( sqlite3_prepare_v3(G_DB, "INSERT INTO menu (parent,title,icon,type,openType,href,sort,visible,remark,createTime,updateTime,isDelete) VALUES (?,?,?,1,'_component',?,?,1,?,?,?,0)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return FALSE;
		}
		sqlite3_bind_int(stmt, 1, iParent);
		sqlite3_bind_text(stmt, 2, sTitle, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sIcon ? sIcon : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sHref, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 5, iSort);
		sqlite3_bind_text(stmt, 6, sRemark ? sRemark : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 7, iNow);
		sqlite3_bind_int(stmt, 8, iNow);
	}
	bOK = (sqlite3_step(stmt) == SQLITE_DONE);
	sqlite3_finalize(stmt);
	return bOK;
}

bool Content_EnsureMenu()
{
	sqlite3_stmt* stmt = NULL;
	int iMenuId = 0;
	int iNow = (int)xrtNow();

	if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE isDelete = 0 AND (href = '/admin/view/content' OR title = ? OR title = ? OR title = 'Content Model') ORDER BY id ASC LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, "内容管理", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, "内容模型", -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iMenuId = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}

	if ( iMenuId > 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE menu SET parent=0,title=?,icon='layui-icon layui-icon-template-1',type=0,openType='',href='',sort=540100,visible=1,remark=?,updateTime=? WHERE id=?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return FALSE;
		}
		sqlite3_bind_text(stmt, 1, "内容管理", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, "内容管理与能力包系统", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 3, iNow);
		sqlite3_bind_int(stmt, 4, iMenuId);
	} else {
		if ( sqlite3_prepare_v3(G_DB, "INSERT INTO menu (parent,title,icon,type,openType,href,sort,visible,remark,createTime,updateTime,isDelete) VALUES (0,?,'layui-icon layui-icon-template-1',0,'','',540100,1,?,?,?,0)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return FALSE;
		}
		sqlite3_bind_text(stmt, 1, "内容管理", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, "内容管理与能力包系统", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 3, iNow);
		sqlite3_bind_int(stmt, 4, iNow);
	}
	if ( sqlite3_step(stmt) != SQLITE_DONE ) {
		sqlite3_finalize(stmt);
		return FALSE;
	}
	sqlite3_finalize(stmt);

	if ( iMenuId <= 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE isDelete = 0 AND title = ? ORDER BY id ASC LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, "内容管理", -1, SQLITE_TRANSIENT);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				iMenuId = sqlite3_column_int(stmt, 0);
			}
			sqlite3_finalize(stmt);
		}
	}
	if ( iMenuId > 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE menu SET isDelete=1,updateTime=? WHERE isDelete=0 AND type=0 AND id<>? AND (title=? OR title='Content Model')", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int(stmt, 1, iNow);
			sqlite3_bind_int(stmt, 2, iMenuId);
			sqlite3_bind_text(stmt, 3, "内容模型", -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	return Content_EnsureMenuItem(iMenuId, "独立页面", "/admin/view/content/page", "layui-icon layui-icon-template", 540105, "独立页面管理")
		&& Content_EnsureMenuItem(iMenuId, "模型管理", "/admin/view/content", "layui-icon layui-icon-list", 540110, "内容模型管理")
		&& Content_EnsureMenuItem(iMenuId, "能力包商店", "/admin/view/content/pack-store", "layui-icon layui-icon-cart", 540120, "能力包商店")
		&& Content_EnsureMenuItem(iMenuId, "能力包管理", "/admin/view/content/packs", "layui-icon layui-icon-component", 540130, "本机能力包管理");
}

bool Content_Init()
{
	printf("[xadmin:init] Content_Init begin\n");
	fflush(stdout);

	if ( !ContentDB_Init() ) {
		printf("[content] database init failed\n");
		return FALSE;
	}
	if ( !ContentPack_Init() ) {
		printf("[content] capability pack init failed\n");
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
