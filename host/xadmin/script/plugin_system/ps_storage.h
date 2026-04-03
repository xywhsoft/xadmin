#ifndef XADMIN_PLUGIN_SYSTEM_STORAGE_H
#define XADMIN_PLUGIN_SYSTEM_STORAGE_H

#include "ps_types.h"

void PS_StorageExecIgnore(const char* sSQL)
{
	if ( (G_DB == NULL) || (sSQL == NULL) ) {
		return;
	}
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
}

const char* PS_StoragePackageXid(PluginSystemPackage* pPackage)
{
	return PS_PackageLogId(pPackage);
}

const char* PS_StorageGenerationXid(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->pPackage == NULL) ) {
		return "(unknown)";
	}
	return PS_StoragePackageXid(pGeneration->pPackage);
}

void PS_StorageMigrateLegacyState()
{
	PS_StorageExecIgnore("INSERT OR IGNORE INTO plugin_runtime (package_id, xid, mount_path, data_path, private_db_path, enabled, installed, config_json, status, active_generation, create_time, update_time) SELECT COALESCE(package_id, xid, instance_id), COALESCE(xid, package_id, instance_id), mount_path, data_path, private_db_path, enabled, installed, config_json, status, active_generation, create_time, update_time FROM plugin_instance");
	PS_StorageExecIgnore("UPDATE plugin_generation SET xid = instance_id WHERE (xid IS NULL OR xid = '') AND instance_id IS NOT NULL");
	PS_StorageExecIgnore("UPDATE plugin_resource SET xid = instance_id WHERE (xid IS NULL OR xid = '') AND instance_id IS NOT NULL");
	PS_StorageExecIgnore("UPDATE plugin_migration_log SET xid = instance_id WHERE (xid IS NULL OR xid = '') AND instance_id IS NOT NULL");
	PS_StorageExecIgnore("UPDATE plugin_service SET xid = instance_id WHERE (xid IS NULL OR xid = '') AND instance_id IS NOT NULL");
	PS_StorageExecIgnore("UPDATE menu SET plugin_xid = plugin_instance_id WHERE (plugin_xid IS NULL OR plugin_xid = '') AND plugin_instance_id IS NOT NULL");
	PS_StorageExecIgnore("UPDATE uris SET plugin_xid = plugin_instance_id WHERE (plugin_xid IS NULL OR plugin_xid = '') AND plugin_instance_id IS NOT NULL");
	PS_StorageExecIgnore("UPDATE authGroup SET plugin_xid = plugin_instance_id WHERE (plugin_xid IS NULL OR plugin_xid = '') AND plugin_instance_id IS NOT NULL");
	PS_StorageExecIgnore("UPDATE auth SET plugin_xid = plugin_instance_id WHERE (plugin_xid IS NULL OR plugin_xid = '') AND plugin_instance_id IS NOT NULL");
	PS_StorageExecIgnore("UPDATE memberAuthGroup SET plugin_xid = plugin_instance_id WHERE (plugin_xid IS NULL OR plugin_xid = '') AND plugin_instance_id IS NOT NULL");
	PS_StorageExecIgnore("UPDATE memberAuth SET plugin_xid = plugin_instance_id WHERE (plugin_xid IS NULL OR plugin_xid = '') AND plugin_instance_id IS NOT NULL");
}

void PS_StorageInit()
{
	if ( G_DB == NULL ) {
		return;
	}

	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_package (package_id TEXT PRIMARY KEY, xid TEXT, plugin_id TEXT, version TEXT, source_type TEXT, install_path TEXT, checksum TEXT, signature TEXT, trust_level TEXT, manifest_json TEXT, install_time INTEGER)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_runtime (package_id TEXT PRIMARY KEY, xid TEXT, mount_path TEXT, data_path TEXT, private_db_path TEXT, enabled INTEGER, installed INTEGER, config_json TEXT, status TEXT, active_generation INTEGER, create_time INTEGER, update_time INTEGER)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_instance (instance_id TEXT PRIMARY KEY, package_id TEXT, xid TEXT, instance_name TEXT, mount_path TEXT, data_path TEXT, private_db_path TEXT, enabled INTEGER, installed INTEGER, config_json TEXT, status TEXT, active_generation INTEGER, create_time INTEGER, update_time INTEGER)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_generation (id INTEGER PRIMARY KEY AUTOINCREMENT, xid TEXT, generation INTEGER, package_version TEXT, state TEXT, compile_hash TEXT, load_time INTEGER, start_time INTEGER, stop_time INTEGER, health_status TEXT, error_message TEXT)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_dependency (id INTEGER PRIMARY KEY AUTOINCREMENT, package_id TEXT, dependency_type TEXT, dependency_name TEXT, min_version TEXT, max_version TEXT)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_resource (id INTEGER PRIMARY KEY AUTOINCREMENT, xid TEXT, generation INTEGER, owner_scope TEXT, resource_type TEXT, resource_key TEXT, resource_ref TEXT, destroy_policy TEXT, create_time INTEGER, status TEXT)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_migration_log (id INTEGER PRIMARY KEY AUTOINCREMENT, xid TEXT, migration_name TEXT, direction TEXT, status TEXT, message TEXT, exec_time INTEGER)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_service (id INTEGER PRIMARY KEY AUTOINCREMENT, xid TEXT, generation INTEGER, service_name TEXT, major_version INTEGER, minor_version INTEGER, status TEXT)");
	PS_StorageExecIgnore("ALTER TABLE plugin_package ADD COLUMN xid TEXT");
	PS_StorageExecIgnore("ALTER TABLE plugin_runtime ADD COLUMN xid TEXT");
	PS_StorageExecIgnore("ALTER TABLE plugin_runtime ADD COLUMN data_path TEXT");
	PS_StorageExecIgnore("ALTER TABLE plugin_runtime ADD COLUMN private_db_path TEXT");
	PS_StorageExecIgnore("ALTER TABLE plugin_runtime ADD COLUMN installed INTEGER");
	PS_StorageExecIgnore("ALTER TABLE plugin_generation ADD COLUMN xid TEXT");
	PS_StorageExecIgnore("ALTER TABLE plugin_resource ADD COLUMN xid TEXT");
	PS_StorageExecIgnore("ALTER TABLE plugin_migration_log ADD COLUMN xid TEXT");
	PS_StorageExecIgnore("ALTER TABLE plugin_service ADD COLUMN xid TEXT");

	PS_StorageExecIgnore("ALTER TABLE menu ADD COLUMN plugin_xid TEXT");
	PS_StorageExecIgnore("ALTER TABLE menu ADD COLUMN plugin_generation INTEGER");
	PS_StorageExecIgnore("ALTER TABLE uris ADD COLUMN plugin_xid TEXT");
	PS_StorageExecIgnore("ALTER TABLE uris ADD COLUMN plugin_generation INTEGER");
	PS_StorageExecIgnore("ALTER TABLE authGroup ADD COLUMN plugin_xid TEXT");
	PS_StorageExecIgnore("ALTER TABLE authGroup ADD COLUMN plugin_generation INTEGER");
	PS_StorageExecIgnore("ALTER TABLE auth ADD COLUMN plugin_xid TEXT");
	PS_StorageExecIgnore("ALTER TABLE auth ADD COLUMN plugin_generation INTEGER");
	PS_StorageExecIgnore("ALTER TABLE memberAuthGroup ADD COLUMN plugin_xid TEXT");
	PS_StorageExecIgnore("ALTER TABLE memberAuthGroup ADD COLUMN plugin_generation INTEGER");
	PS_StorageExecIgnore("ALTER TABLE memberAuth ADD COLUMN plugin_xid TEXT");
	PS_StorageExecIgnore("ALTER TABLE memberAuth ADD COLUMN plugin_generation INTEGER");

	PS_StorageMigrateLegacyState();
}

void PS_StorageUnit()
{
}

void PS_StorageBindText(sqlite3_stmt* stmt, int iIndex, const char* sText)
{
	sqlite3_bind_text(stmt, iIndex, sText ? sText : "", -1, NULL);
}

int PS_StorageTrackResource(PluginSystemGeneration* pGeneration, const char* sOwnerScope, const char* sResourceType, const char* sResourceKey, const char* sResourceRef, const char* sDestroyPolicy)
{
	sqlite3_stmt* stmt = NULL;
	int iResourceId = 0;
	const char* sXid;

	if ( (G_DB == NULL) || (pGeneration == NULL) || (pGeneration->pPackage == NULL) ) {
		return 0;
	}

	sXid = PS_StorageGenerationXid(pGeneration);
	if ( sqlite3_prepare_v3(G_DB, "INSERT INTO plugin_resource (xid, generation, owner_scope, resource_type, resource_key, resource_ref, destroy_policy, create_time, status) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'active')", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return 0;
	}

	if ( sResourceType && sResourceKey ) {
		sqlite3_stmt* stmtCleanup = NULL;
		if ( sqlite3_prepare_v3(G_DB, "UPDATE plugin_resource SET status = 'removed' WHERE xid = ? AND resource_type = ? AND resource_key = ? AND status = 'active'", -1, SQL_PREPARE_DEFAULT, &stmtCleanup, NULL) == SQLITE_OK ) {
			PS_StorageBindText(stmtCleanup, 1, sXid);
			PS_StorageBindText(stmtCleanup, 2, sResourceType);
			PS_StorageBindText(stmtCleanup, 3, sResourceKey);
			sqlite3_step(stmtCleanup);
			sqlite3_finalize(stmtCleanup);
		}
	}

	PS_StorageBindText(stmt, 1, sXid);
	sqlite3_bind_int(stmt, 2, (int)pGeneration->iGeneration);
	PS_StorageBindText(stmt, 3, sOwnerScope ? sOwnerScope : "generation");
	PS_StorageBindText(stmt, 4, sResourceType);
	PS_StorageBindText(stmt, 5, sResourceKey);
	PS_StorageBindText(stmt, 6, sResourceRef);
	PS_StorageBindText(stmt, 7, sDestroyPolicy ? sDestroyPolicy : "auto_unload");
	sqlite3_bind_int64(stmt, 8, xrtNow());
	if ( sqlite3_step(stmt) == SQLITE_DONE ) {
		iResourceId = (int)sqlite3_last_insert_rowid(G_DB);
	}

	sqlite3_finalize(stmt);
	return iResourceId;
}

bool PS_StorageUpdateResourceStatus(int iResourceId, const char* sStatus)
{
	sqlite3_stmt* stmt = NULL;
	bool bOK = FALSE;

	if ( (G_DB == NULL) || (iResourceId <= 0) ) {
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB, "UPDATE plugin_resource SET status = ? WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}

	PS_StorageBindText(stmt, 1, sStatus ? sStatus : "removed");
	sqlite3_bind_int(stmt, 2, iResourceId);
	bOK = (sqlite3_step(stmt) == SQLITE_DONE);
	sqlite3_finalize(stmt);
	return bOK;
}

int PS_StorageSaveService(PluginSystemGeneration* pGeneration, const char* sServiceName, int iMajorVersion, int iMinorVersion, const char* sStatus)
{
	sqlite3_stmt* stmt = NULL;
	int iServiceId = 0;
	const char* sXid;

	if ( (G_DB == NULL) || (pGeneration == NULL) || (pGeneration->pPackage == NULL) || (sServiceName == NULL) || (sServiceName[0] == '\0') ) {
		return 0;
	}

	sXid = PS_StorageGenerationXid(pGeneration);
	if ( sqlite3_prepare_v3(G_DB, "INSERT OR REPLACE INTO plugin_service (id, xid, generation, service_name, major_version, minor_version, status) VALUES ((SELECT id FROM plugin_service WHERE xid = ? AND generation = ? AND service_name = ? AND major_version = ? AND minor_version = ? LIMIT 1), ?, ?, ?, ?, ?, ?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return 0;
	}

	PS_StorageBindText(stmt, 1, sXid);
	sqlite3_bind_int(stmt, 2, (int)pGeneration->iGeneration);
	PS_StorageBindText(stmt, 3, sServiceName);
	sqlite3_bind_int(stmt, 4, iMajorVersion);
	sqlite3_bind_int(stmt, 5, iMinorVersion);
	PS_StorageBindText(stmt, 6, sXid);
	sqlite3_bind_int(stmt, 7, (int)pGeneration->iGeneration);
	PS_StorageBindText(stmt, 8, sServiceName);
	sqlite3_bind_int(stmt, 9, iMajorVersion);
	sqlite3_bind_int(stmt, 10, iMinorVersion);
	PS_StorageBindText(stmt, 11, sStatus ? sStatus : "active");
	if ( sqlite3_step(stmt) == SQLITE_DONE ) {
		iServiceId = (int)sqlite3_last_insert_rowid(G_DB);
	}

	sqlite3_finalize(stmt);
	return iServiceId;
}

bool PS_StorageUpdateServiceStatus(int iServiceId, const char* sStatus)
{
	sqlite3_stmt* stmt = NULL;
	bool bOK = FALSE;

	if ( (G_DB == NULL) || (iServiceId <= 0) ) {
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB, "UPDATE plugin_service SET status = ? WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}

	PS_StorageBindText(stmt, 1, sStatus ? sStatus : "removed");
	sqlite3_bind_int(stmt, 2, iServiceId);
	bOK = (sqlite3_step(stmt) == SQLITE_DONE);
	sqlite3_finalize(stmt);
	return bOK;
}

bool PS_StorageSavePackage(PluginSystemPackage* pPackage)
{
	sqlite3_stmt* stmt = NULL;
	str sManifestJson = NULL;
	str sPackageId;
	bool bOK = FALSE;

	if ( (G_DB == NULL) || (pPackage == NULL) ) {
		return FALSE;
	}

	sPackageId = PS_PackageKey(pPackage);
	sManifestJson = pPackage->tblManifest ? xrtStringifyJSON(pPackage->tblManifest, FALSE, NULL) : xrtCopyStr("{}", 0);
	if ( sManifestJson == NULL ) {
		sManifestJson = xrtCopyStr("{}", 0);
	}

	if ( sqlite3_prepare_v3(G_DB, "INSERT OR REPLACE INTO plugin_package (package_id, xid, plugin_id, version, source_type, install_path, checksum, signature, trust_level, manifest_json, install_time) VALUES (?, ?, ?, ?, 'local', ?, '', '', 'system', ?, ?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, (const char*)(sPackageId ? sPackageId : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 2, (const char*)(pPackage->sXid ? pPackage->sXid : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 3, (const char*)(pPackage->sXid ? pPackage->sXid : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 4, (const char*)(pPackage->sVersion ? pPackage->sVersion : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 5, (const char*)(pPackage->sRootPath ? pPackage->sRootPath : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 6, (const char*)(sManifestJson ? sManifestJson : (str)"{}"), -1, NULL);
		sqlite3_bind_int64(stmt, 7, xrtNow());
		bOK = (sqlite3_step(stmt) == SQLITE_DONE);
	}

	if ( stmt ) {
		sqlite3_finalize(stmt);
	}
	if ( sManifestJson ) {
		xrtFree(sManifestJson);
	}
	return bOK;
}

bool PS_StorageLoadRuntime(PluginSystemPackage* pPackage)
{
	sqlite3_stmt* stmt = NULL;
	bool bFound = FALSE;
	const char* sPackageId;

	if ( (G_DB == NULL) || (pPackage == NULL) ) {
		return FALSE;
	}

	sPackageId = PS_StoragePackageXid(pPackage);
	if ( sqlite3_prepare_v3(G_DB, "SELECT enabled, installed, status, active_generation, config_json, create_time, update_time, data_path, private_db_path FROM plugin_runtime WHERE package_id = ? OR xid = ? LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}

	sqlite3_bind_text(stmt, 1, sPackageId, -1, NULL);
	sqlite3_bind_text(stmt, 2, sPackageId, -1, NULL);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		const unsigned char* sConfigJson = sqlite3_column_text(stmt, 4);
		xvalue tblConfig = NULL;
		bFound = TRUE;
		pPackage->bEnabled = sqlite3_column_int(stmt, 0) ? TRUE : FALSE;
		pPackage->bInstalled = sqlite3_column_int(stmt, 1) ? TRUE : FALSE;
		pPackage->iStatus = PS_PackageStatusFromText((str)sqlite3_column_text(stmt, 2));
		pPackage->iActiveGeneration = (uint32_t)sqlite3_column_int(stmt, 3);
		pPackage->iCreateTime = sqlite3_column_int64(stmt, 5);
		pPackage->iUpdateTime = sqlite3_column_int64(stmt, 6);
		PS_FreeString(&pPackage->sDataPath);
		PS_FreeString(&pPackage->sPrivateDbPath);
		pPackage->sDataPath = xrtCopyStr((str)(sqlite3_column_text(stmt, 7) ? sqlite3_column_text(stmt, 7) : (const unsigned char*)""), 0);
		pPackage->sPrivateDbPath = xrtCopyStr((str)(sqlite3_column_text(stmt, 8) ? sqlite3_column_text(stmt, 8) : (const unsigned char*)""), 0);
		if ( pPackage->iActiveGeneration >= pPackage->iNextGeneration ) {
			pPackage->iNextGeneration = pPackage->iActiveGeneration + 1;
		}
		if ( sConfigJson && sConfigJson[0] ) {
			tblConfig = PS_ValueParseJsonShared((str)sConfigJson, strlen((const char*)sConfigJson));
		}
		if ( tblConfig && (xvoType(tblConfig) == XVO_DT_TABLE) ) {
			if ( pPackage->tblConfig ) {
				xvoUnref(pPackage->tblConfig);
			}
			pPackage->tblConfig = tblConfig;
			tblConfig = NULL;
		}
		if ( tblConfig ) {
			xvoUnref(tblConfig);
		}
	}

	sqlite3_finalize(stmt);
	return bFound;
}

bool PS_StorageSaveRuntime(PluginSystemPackage* pPackage)
{
	sqlite3_stmt* stmt = NULL;
	bool bOK = FALSE;
	int64 iNow;
	const char* sPackageId;
	str sConfigJson = NULL;

	if ( (G_DB == NULL) || (pPackage == NULL) ) {
		return FALSE;
	}

	iNow = xrtNow();
	if ( pPackage->iCreateTime <= 0 ) {
		pPackage->iCreateTime = iNow;
	}
	pPackage->iUpdateTime = iNow;
	sPackageId = PS_StoragePackageXid(pPackage);
	sConfigJson = pPackage->tblConfig ? xrtStringifyJSON(pPackage->tblConfig, FALSE, NULL) : xrtCopyStr("{}", 0);
	if ( sConfigJson == NULL ) {
		sConfigJson = xrtCopyStr("{}", 0);
	}

	if ( sqlite3_prepare_v3(G_DB, "INSERT OR REPLACE INTO plugin_runtime (package_id, xid, mount_path, data_path, private_db_path, enabled, installed, config_json, status, active_generation, create_time, update_time) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sPackageId, -1, NULL);
		sqlite3_bind_text(stmt, 2, sPackageId, -1, NULL);
		sqlite3_bind_text(stmt, 3, (const char*)(pPackage->sMountPath ? pPackage->sMountPath : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 4, (const char*)(pPackage->sDataPath ? pPackage->sDataPath : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 5, (const char*)(pPackage->sPrivateDbPath ? pPackage->sPrivateDbPath : (str)""), -1, NULL);
		sqlite3_bind_int(stmt, 6, pPackage->bEnabled ? 1 : 0);
		sqlite3_bind_int(stmt, 7, pPackage->bInstalled ? 1 : 0);
		sqlite3_bind_text(stmt, 8, (const char*)(sConfigJson ? sConfigJson : (str)"{}"), -1, NULL);
		sqlite3_bind_text(stmt, 9, PS_PackageStatusText(pPackage->iStatus), -1, NULL);
		sqlite3_bind_int(stmt, 10, (int)pPackage->iActiveGeneration);
		sqlite3_bind_int64(stmt, 11, pPackage->iCreateTime);
		sqlite3_bind_int64(stmt, 12, pPackage->iUpdateTime);
		bOK = (sqlite3_step(stmt) == SQLITE_DONE);
	}

	if ( stmt ) {
		sqlite3_finalize(stmt);
	}
	if ( sConfigJson ) {
		xrtFree(sConfigJson);
	}
	return bOK;
}

bool PS_StorageSaveGeneration(PluginSystemPackage* pPackage, PluginSystemGeneration* pGeneration)
{
	sqlite3_stmt* stmt = NULL;
	bool bOK = FALSE;
	const char* sXid;

	if ( (G_DB == NULL) || (pPackage == NULL) || (pGeneration == NULL) ) {
		return FALSE;
	}

	sXid = PS_StoragePackageXid(pPackage);
	if ( sqlite3_prepare_v3(G_DB, "INSERT OR REPLACE INTO plugin_generation (id, xid, generation, package_version, state, compile_hash, load_time, start_time, stop_time, health_status, error_message) VALUES ((SELECT id FROM plugin_generation WHERE xid = ? AND generation = ? LIMIT 1), ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sXid, -1, NULL);
		sqlite3_bind_int(stmt, 2, (int)pGeneration->iGeneration);
		sqlite3_bind_text(stmt, 3, sXid, -1, NULL);
		sqlite3_bind_int(stmt, 4, (int)pGeneration->iGeneration);
		sqlite3_bind_text(stmt, 5, (const char*)(pPackage->sVersion ? pPackage->sVersion : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 6, PS_GenerationStateText(pGeneration->iState), -1, NULL);
		sqlite3_bind_text(stmt, 7, (const char*)(pGeneration->sCompileHash ? pGeneration->sCompileHash : (str)""), -1, NULL);
		sqlite3_bind_int64(stmt, 8, pGeneration->iLoadTime);
		sqlite3_bind_int64(stmt, 9, pGeneration->iStartTime);
		sqlite3_bind_int64(stmt, 10, pGeneration->iStopTime);
		sqlite3_bind_text(stmt, 11, (pGeneration->iState == PS_GENERATION_STATE_ACTIVE) ? "ok" : ((pGeneration->iState == PS_GENERATION_STATE_DRAINING) ? "draining" : ""), -1, NULL);
		sqlite3_bind_text(stmt, 12, (const char*)(pGeneration->sErrorMessage ? pGeneration->sErrorMessage : (str)""), -1, NULL);
		bOK = (sqlite3_step(stmt) == SQLITE_DONE);
	}

	if ( stmt ) {
		sqlite3_finalize(stmt);
	}
	return bOK;
}

#endif
