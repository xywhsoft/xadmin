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

void PS_StorageInit()
{
	if ( G_DB == NULL ) {
		return;
	}

	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_package (package_id TEXT PRIMARY KEY, plugin_id TEXT, version TEXT, source_type TEXT, install_path TEXT, checksum TEXT, signature TEXT, trust_level TEXT, manifest_json TEXT, install_time INTEGER)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_instance (instance_id TEXT PRIMARY KEY, package_id TEXT, instance_name TEXT, mount_path TEXT, enabled INTEGER, config_json TEXT, status TEXT, active_generation INTEGER, create_time INTEGER, update_time INTEGER)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_generation (id INTEGER PRIMARY KEY AUTOINCREMENT, instance_id TEXT, generation INTEGER, package_version TEXT, state TEXT, compile_hash TEXT, load_time INTEGER, start_time INTEGER, stop_time INTEGER, health_status TEXT, error_message TEXT)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_dependency (id INTEGER PRIMARY KEY AUTOINCREMENT, package_id TEXT, dependency_type TEXT, dependency_name TEXT, min_version TEXT, max_version TEXT)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_resource (id INTEGER PRIMARY KEY AUTOINCREMENT, instance_id TEXT, generation INTEGER, owner_scope TEXT, resource_type TEXT, resource_key TEXT, resource_ref TEXT, destroy_policy TEXT, create_time INTEGER, status TEXT)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_migration_log (id INTEGER PRIMARY KEY AUTOINCREMENT, instance_id TEXT, migration_name TEXT, direction TEXT, status TEXT, message TEXT, exec_time INTEGER)");
	PS_StorageExecIgnore("CREATE TABLE IF NOT EXISTS plugin_service (id INTEGER PRIMARY KEY AUTOINCREMENT, instance_id TEXT, generation INTEGER, service_name TEXT, major_version INTEGER, minor_version INTEGER, status TEXT)");

	PS_StorageExecIgnore("ALTER TABLE menu ADD COLUMN plugin_instance_id TEXT");
	PS_StorageExecIgnore("ALTER TABLE menu ADD COLUMN plugin_generation INTEGER");
	PS_StorageExecIgnore("ALTER TABLE uris ADD COLUMN plugin_instance_id TEXT");
	PS_StorageExecIgnore("ALTER TABLE uris ADD COLUMN plugin_generation INTEGER");
	PS_StorageExecIgnore("ALTER TABLE authGroup ADD COLUMN plugin_instance_id TEXT");
	PS_StorageExecIgnore("ALTER TABLE authGroup ADD COLUMN plugin_generation INTEGER");
	PS_StorageExecIgnore("ALTER TABLE auth ADD COLUMN plugin_instance_id TEXT");
	PS_StorageExecIgnore("ALTER TABLE auth ADD COLUMN plugin_generation INTEGER");
	PS_StorageExecIgnore("ALTER TABLE memberAuthGroup ADD COLUMN plugin_instance_id TEXT");
	PS_StorageExecIgnore("ALTER TABLE memberAuthGroup ADD COLUMN plugin_generation INTEGER");
	PS_StorageExecIgnore("ALTER TABLE memberAuth ADD COLUMN plugin_instance_id TEXT");
	PS_StorageExecIgnore("ALTER TABLE memberAuth ADD COLUMN plugin_generation INTEGER");
}

void PS_StorageUnit()
{
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

	if ( sqlite3_prepare_v3(G_DB, "INSERT OR REPLACE INTO plugin_package (package_id, plugin_id, version, source_type, install_path, checksum, signature, trust_level, manifest_json, install_time) VALUES (?, ?, ?, 'local', ?, '', '', 'system', ?, ?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, (const char*)(sPackageId ? sPackageId : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 2, (const char*)(pPackage->sPluginId ? pPackage->sPluginId : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 3, (const char*)(pPackage->sVersion ? pPackage->sVersion : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 4, (const char*)(pPackage->sRootPath ? pPackage->sRootPath : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 5, (const char*)(sManifestJson ? sManifestJson : (str)"{}"), -1, NULL);
		sqlite3_bind_int64(stmt, 6, xrtNow());
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

bool PS_StorageLoadInstanceState(PluginSystemInstance* pInstance)
{
	sqlite3_stmt* stmt = NULL;
	bool bFound = FALSE;

	if ( (G_DB == NULL) || (pInstance == NULL) || (pInstance->sInstanceId == NULL) ) {
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT enabled, status, active_generation, config_json, create_time, update_time FROM plugin_instance WHERE instance_id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}

	sqlite3_bind_text(stmt, 1, pInstance->sInstanceId, -1, NULL);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		const unsigned char* sConfigJson = sqlite3_column_text(stmt, 3);
		xvalue tblConfig = NULL;

		bFound = TRUE;
		pInstance->bEnabled = sqlite3_column_int(stmt, 0) ? TRUE : FALSE;
		pInstance->iStatus = PS_InstanceStatusFromText((str)sqlite3_column_text(stmt, 1));
		pInstance->iActiveGeneration = (uint32_t)sqlite3_column_int(stmt, 2);
		pInstance->iCreateTime = sqlite3_column_int64(stmt, 4);
		pInstance->iUpdateTime = sqlite3_column_int64(stmt, 5);
		if ( pInstance->iActiveGeneration >= pInstance->iNextGeneration ) {
			pInstance->iNextGeneration = pInstance->iActiveGeneration + 1;
		}

		if ( sConfigJson && sConfigJson[0] ) {
			tblConfig = xrtParseJSON((str)sConfigJson, strlen((str)sConfigJson));
			if ( tblConfig ) {
				if ( pInstance->tblConfig ) {
					xvoUnref(pInstance->tblConfig);
				}
				pInstance->tblConfig = tblConfig;
			}
		}
	}

	sqlite3_finalize(stmt);
	return bFound;
}

bool PS_StorageSaveInstance(PluginSystemInstance* pInstance)
{
	sqlite3_stmt* stmt = NULL;
	str sConfigJson = NULL;
	bool bOK = FALSE;
	int64 iNow;

	if ( (G_DB == NULL) || (pInstance == NULL) ) {
		return FALSE;
	}

	if ( pInstance->tblConfig ) {
		sConfigJson = xrtStringifyJSON(pInstance->tblConfig, FALSE, NULL);
	}
	if ( sConfigJson == NULL ) {
		sConfigJson = xrtCopyStr("{}", 0);
	}

	iNow = xrtNow();
	if ( pInstance->iCreateTime <= 0 ) {
		pInstance->iCreateTime = iNow;
	}
	pInstance->iUpdateTime = iNow;

	if ( sqlite3_prepare_v3(G_DB, "INSERT OR REPLACE INTO plugin_instance (instance_id, package_id, instance_name, mount_path, enabled, config_json, status, active_generation, create_time, update_time) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, (const char*)(pInstance->sInstanceId ? pInstance->sInstanceId : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 2, (const char*)(pInstance->sPackageId ? pInstance->sPackageId : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 3, (const char*)(pInstance->sInstanceName ? pInstance->sInstanceName : (str)""), -1, NULL);
		sqlite3_bind_text(stmt, 4, (const char*)(pInstance->sMountPath ? pInstance->sMountPath : (str)""), -1, NULL);
		sqlite3_bind_int(stmt, 5, pInstance->bEnabled ? 1 : 0);
		sqlite3_bind_text(stmt, 6, sConfigJson, -1, NULL);
		sqlite3_bind_text(stmt, 7, PS_InstanceStatusText(pInstance->iStatus), -1, NULL);
		sqlite3_bind_int(stmt, 8, (int)pInstance->iActiveGeneration);
		sqlite3_bind_int64(stmt, 9, pInstance->iCreateTime);
		sqlite3_bind_int64(stmt, 10, pInstance->iUpdateTime);
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

bool PS_StorageSaveGeneration(PluginSystemInstance* pInstance, PluginSystemGeneration* pGeneration)
{
	sqlite3_stmt* stmt = NULL;
	bool bOK = FALSE;

	if ( (G_DB == NULL) || (pInstance == NULL) || (pGeneration == NULL) ) {
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB, "INSERT OR REPLACE INTO plugin_generation (id, instance_id, generation, package_version, state, compile_hash, load_time, start_time, stop_time, health_status, error_message) VALUES ((SELECT id FROM plugin_generation WHERE instance_id = ? AND generation = ? LIMIT 1), ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, (const char*)(pInstance->sInstanceId ? pInstance->sInstanceId : (str)""), -1, NULL);
		sqlite3_bind_int(stmt, 2, (int)pGeneration->iGeneration);
		sqlite3_bind_text(stmt, 3, (const char*)(pInstance->sInstanceId ? pInstance->sInstanceId : (str)""), -1, NULL);
		sqlite3_bind_int(stmt, 4, (int)pGeneration->iGeneration);
		sqlite3_bind_text(stmt, 5, "", -1, NULL);
		sqlite3_bind_text(stmt, 6, (pGeneration->iState == PS_GENERATION_STATE_ACTIVE) ? "active" : ((pGeneration->iState == PS_GENERATION_STATE_FAILED) ? "failed" : ((pGeneration->iState == PS_GENERATION_STATE_STOPPED) ? "stopped" : "loaded")), -1, NULL);
		sqlite3_bind_text(stmt, 7, (const char*)(pGeneration->sCompileHash ? pGeneration->sCompileHash : (str)""), -1, NULL);
		sqlite3_bind_int64(stmt, 8, pGeneration->iLoadTime);
		sqlite3_bind_int64(stmt, 9, pGeneration->iStartTime);
		sqlite3_bind_int64(stmt, 10, pGeneration->iStopTime);
		sqlite3_bind_text(stmt, 11, (pGeneration->iState == PS_GENERATION_STATE_ACTIVE) ? "ok" : "", -1, NULL);
		sqlite3_bind_text(stmt, 12, (const char*)(pGeneration->sErrorMessage ? pGeneration->sErrorMessage : (str)""), -1, NULL);
		bOK = (sqlite3_step(stmt) == SQLITE_DONE);
	}

	if ( stmt ) {
		sqlite3_finalize(stmt);
	}
	return bOK;
}

#endif
