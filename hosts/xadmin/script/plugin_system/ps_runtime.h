#ifndef XADMIN_PLUGIN_SYSTEM_RUNTIME_H
#define XADMIN_PLUGIN_SYSTEM_RUNTIME_H

#include "ps_abi.h"
#include "ps_storage.h"
#include "ps_compiler_tcc.h"

void PS_RuntimeCleanupRoutes(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstRouteTokens == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstRouteTokens); i++ ) {
		ptr pTokenPath = xrtListGetPtr(pGeneration->lstRouteTokens, i);
		if ( pTokenPath ) {
			PS_HostUnregisterRoute((XAdminRouteToken)(uintptr_t)pTokenPath);
		}
	}
}

void PS_RuntimeCleanupMenus(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstMenuTokens == NULL) ) {
		return;
	}

	for ( int i = xrtListCount(pGeneration->lstMenuTokens) - 1; i >= 0; i-- ) {
		ptr pToken = xrtListGetPtr(pGeneration->lstMenuTokens, i);
		if ( pToken ) {
			PS_HostUnregisterMenu((XAdminMenuToken)(uintptr_t)pToken);
			xrtListSetPtr(pGeneration->lstMenuTokens, i, NULL, NULL);
		}
	}
}

void PS_RuntimeCleanupUriAuths(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstUriAuthTokens == NULL) ) {
		return;
	}

	for ( int i = xrtListCount(pGeneration->lstUriAuthTokens) - 1; i >= 0; i-- ) {
		ptr pToken = xrtListGetPtr(pGeneration->lstUriAuthTokens, i);
		if ( pToken ) {
			PS_HostUnregisterUriAuth((XAdminUriAuthToken)(uintptr_t)pToken);
			xrtListSetPtr(pGeneration->lstUriAuthTokens, i, NULL, NULL);
		}
	}
}

void PS_RuntimeCleanupAuths(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstAuthTokens == NULL) ) {
		return;
	}

	for ( int i = xrtListCount(pGeneration->lstAuthTokens) - 1; i >= 0; i-- ) {
		ptr pToken = xrtListGetPtr(pGeneration->lstAuthTokens, i);
		if ( pToken ) {
			PS_HostUnregisterAuth((XAdminAuthToken)(uintptr_t)pToken);
			xrtListSetPtr(pGeneration->lstAuthTokens, i, NULL, NULL);
		}
	}
}

void PS_RuntimeCleanupAuthGroups(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstAuthGroupTokens == NULL) ) {
		return;
	}

	for ( int i = xrtListCount(pGeneration->lstAuthGroupTokens) - 1; i >= 0; i-- ) {
		ptr pToken = xrtListGetPtr(pGeneration->lstAuthGroupTokens, i);
		if ( pToken ) {
			PS_HostUnregisterAuthGroup((XAdminAuthGroupToken)(uintptr_t)pToken);
			xrtListSetPtr(pGeneration->lstAuthGroupTokens, i, NULL, NULL);
		}
	}
}

void PS_RuntimeCleanupGenerationEntryPoints(PluginSystemGeneration* pGeneration)
{
	PS_RuntimeCleanupUriAuths(pGeneration);
	PS_RuntimeCleanupAuths(pGeneration);
	PS_RuntimeCleanupAuthGroups(pGeneration);
	PS_RuntimeCleanupMenus(pGeneration);
	PS_RuntimeCleanupRoutes(pGeneration);
}

void PS_RuntimeCleanupGenerationResources(PluginSystemGeneration* pGeneration)
{
	PS_RuntimeCleanupGenerationEntryPoints(pGeneration);
	PS_HookDestroyGenerationRegistrations(pGeneration);
	PS_EventDestroyGenerationRegistrations(pGeneration);
	PS_ServiceDestroyGenerationRegistrations(pGeneration);
}

void PS_RuntimeTrackDrainingGeneration(PluginSystemPackage* pPackage, PluginSystemGeneration* pGeneration)
{
	if ( (pPackage == NULL) || (pGeneration == NULL) || (pPackage->lstDrainingGenerations == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(pPackage->lstDrainingGenerations); i++ ) {
		if ( xrtListGetPtr(pPackage->lstDrainingGenerations, i) == pGeneration ) {
			return;
		}
	}

	xrtListSetPtr(pPackage->lstDrainingGenerations, xrtListCount(pPackage->lstDrainingGenerations), pGeneration, NULL);
}

void PS_RuntimeDetachDrainingGeneration(PluginSystemPackage* pPackage, PluginSystemGeneration* pGeneration)
{
	if ( (pPackage == NULL) || (pGeneration == NULL) || (pPackage->lstDrainingGenerations == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(pPackage->lstDrainingGenerations); i++ ) {
		if ( xrtListGetPtr(pPackage->lstDrainingGenerations, i) == pGeneration ) {
			xrtListSetPtr(pPackage->lstDrainingGenerations, i, NULL, NULL);
			return;
		}
	}
}

void PS_RuntimeFinalizeGeneration(PluginSystemPackage* pPackage, PluginSystemGeneration* pGeneration)
{
	if ( pGeneration == NULL ) {
		return;
	}

	printf("        [PluginSystem] Finalize generation: package=%s generation=%u state=%d refs=%d\n",
		PS_PackageLogId(pPackage),
		pGeneration->iGeneration,
		pGeneration->iState,
		pGeneration->iRefCount);

	PS_ServiceUnpublishGeneration(pGeneration, "removed");
	if ( pGeneration->pDescriptor && pGeneration->pDescriptor->OnStop ) {
		pGeneration->pDescriptor->OnStop((XAdminPluginHandle)pGeneration);
	}
	PS_RuntimeCleanupGenerationResources(pGeneration);
	if ( pGeneration->pDescriptor && pGeneration->pDescriptor->OnUnload ) {
		pGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pGeneration);
	}
	PS_HostDestroyGenerationRouteTokens(pGeneration);
	pGeneration->iStopTime = xrtNow();
	pGeneration->iState = PS_GENERATION_STATE_STOPPED;
	PS_StorageSaveGeneration(pPackage, pGeneration);
	PS_RuntimeDetachDrainingGeneration(pPackage, pGeneration);
	if ( pPackage && (pPackage->pActiveGeneration == pGeneration) ) {
		pPackage->pActiveGeneration = NULL;
		pPackage->iActiveGeneration = 0;
	}
	PS_DestroyGeneration(pGeneration);
}

void PS_RuntimeOnGenerationRefReleased(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->iState != PS_GENERATION_STATE_DRAINING) || (pGeneration->iRefCount > 0) ) {
		return;
	}

	PS_RuntimeFinalizeGeneration(pGeneration->pPackage, pGeneration);
}

void PS_RuntimeForceDrainPackage(PluginSystemPackage* pPackage)
{
	if ( (pPackage == NULL) || (pPackage->lstDrainingGenerations == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(pPackage->lstDrainingGenerations); i++ ) {
		PluginSystemGeneration* pGeneration = xrtListGetPtr(pPackage->lstDrainingGenerations, i);
		if ( pGeneration ) {
			pGeneration->iRefCount = 0;
			PS_RuntimeFinalizeGeneration(pPackage, pGeneration);
		}
	}
}

#define PS_RUNTIME_DRAIN_WAIT_SLICE_MS 10
#define PS_RUNTIME_DRAIN_WAIT_TIMEOUT_SECONDS 30

bool PS_RuntimeWaitForPackageDrain(PluginSystemPackage* pPackage, PluginSystemGeneration* pExcludedGeneration)
{
	int64 iDeadline;

	if ( (pPackage == NULL) || (pPackage->lstDrainingGenerations == NULL) ) {
		return TRUE;
	}

	iDeadline = xrtNow() + PS_RUNTIME_DRAIN_WAIT_TIMEOUT_SECONDS;
	while ( TRUE ) {
		bool bWaiting = FALSE;

		for ( int i = 0; i < xrtListCount(pPackage->lstDrainingGenerations); i++ ) {
			PluginSystemGeneration* pGeneration = xrtListGetPtr(pPackage->lstDrainingGenerations, i);
			if ( (pGeneration == NULL) || (pGeneration == pExcludedGeneration) ) {
				continue;
			}
			if ( pGeneration->iState != PS_GENERATION_STATE_DRAINING ) {
				continue;
			}
			if ( pGeneration->iRefCount <= 0 ) {
				PS_RuntimeFinalizeGeneration(pPackage, pGeneration);
				bWaiting = TRUE;
				break;
			}

			bWaiting = TRUE;
			break;
		}

		if ( !bWaiting ) {
			return TRUE;
		}
		if ( xrtNow() >= iDeadline ) {
			printf("        [PluginSystem] Drain wait timeout: package=%s excluded_generation=%u\n",
				PS_PackageLogId(pPackage),
				pExcludedGeneration ? pExcludedGeneration->iGeneration : 0);
			return FALSE;
		}

		xrtSleep(PS_RUNTIME_DRAIN_WAIT_SLICE_MS);
	}
}

bool PS_RuntimeFailGeneration(PluginSystemPackage* pPackage, PluginSystemGeneration* pGeneration, str sMessage, bool bMarkPackageFailed)
{
	str sSafeMessage = NULL;

	printf("        [PluginSystem] Generation failed: package=%s generation=%u reason=%s\n",
		PS_PackageLogId(pPackage),
		pGeneration ? pGeneration->iGeneration : 0,
		sMessage ? (const char*)sMessage : "(null)");

	if ( pGeneration ) {
		/* Compile failures often pass pGeneration->sErrorMessage back into this function. */
		if ( sMessage == pGeneration->sErrorMessage ) {
			sSafeMessage = xrtCopyStr(sMessage, 0);
			sMessage = sSafeMessage;
		}
		PS_FreeString(&pGeneration->sErrorMessage);
		pGeneration->sErrorMessage = xrtCopyStr(sMessage, 0);
		pGeneration->iState = PS_GENERATION_STATE_FAILED;
		PS_StorageSaveGeneration(pPackage, pGeneration);
		if ( sSafeMessage ) {
			xrtFree(sSafeMessage);
		}
	}
	if ( bMarkPackageFailed && pPackage ) {
		pPackage->iStatus = PS_PACKAGE_STATUS_FAILED;
		pPackage->iUpdateTime = xrtNow();
		PS_StorageSaveRuntime(pPackage);
	}
	return FALSE;
}

void PS_RuntimePersistInstalled(PluginSystemPackage* pPackage)
{
	if ( (pPackage == NULL) || pPackage->bInstalled ) {
		return;
	}

	pPackage->bInstalled = TRUE;
	pPackage->iUpdateTime = xrtNow();
	if ( !PS_StorageSaveRuntime(pPackage) ) {
		printf("        [PluginSystem] Persist install state failed: package=%s\n", PS_PackageLogId(pPackage));
	}
}

void PS_RuntimeDeactivateGeneration(PluginSystemPackage* pPackage, PluginSystemGeneration* pGeneration)
{
	if ( pGeneration == NULL ) {
		return;
	}

	printf("        [PluginSystem] Stopping generation: package=%s generation=%u state=%d\n",
		PS_PackageLogId(pPackage),
		pGeneration->iGeneration,
		pGeneration->iState);

	PS_HookUnpublishGeneration(pGeneration);
	PS_EventUnpublishGeneration(pGeneration);
	PS_ServiceUnpublishGeneration(pGeneration, (pGeneration->iRefCount > 0) ? "draining" : "removed");
	if ( pGeneration->iRefCount > 0 ) {
		PS_RuntimeCleanupGenerationEntryPoints(pGeneration);
		pGeneration->iStopTime = xrtNow();
		pGeneration->iState = PS_GENERATION_STATE_DRAINING;
		PS_StorageSaveGeneration(pPackage, pGeneration);
		PS_RuntimeTrackDrainingGeneration(pPackage, pGeneration);
		printf("        [PluginSystem] Generation draining: package=%s generation=%u refs=%d\n",
			PS_PackageLogId(pPackage),
			pGeneration->iGeneration,
			pGeneration->iRefCount);
		return;
	}

	PS_RuntimeFinalizeGeneration(pPackage, pGeneration);
}

bool PS_RuntimePrepareGeneration(PluginSystemPackage* pPackage, PluginSystemGeneration* pNewGeneration, bool bAffectPackage)
{
	XAdminHealthReport report;
	xvalue tblConfig = NULL;

	if ( (pPackage == NULL) || (pNewGeneration == NULL) ) {
		return FALSE;
	}

	pNewGeneration->pPackage = pPackage;

	printf("        [PluginSystem] Preparing generation: package=%s generation=%u\n",
		PS_PackageLogId(pPackage),
		pNewGeneration->iGeneration);

	if ( !PS_CompileGeneration(pPackage, pNewGeneration) ) {
		PS_RuntimeFailGeneration(pPackage, pNewGeneration, pNewGeneration->sErrorMessage ? pNewGeneration->sErrorMessage : (str)"compile failed", bAffectPackage);
		PS_HostDestroyGenerationRouteTokens(pNewGeneration);
		PS_DestroyGeneration(pNewGeneration);
		return FALSE;
	}

	pNewGeneration->iState = PS_GENERATION_STATE_LOADED;
	pNewGeneration->hPlugin = (XAdminPluginHandle)pNewGeneration;
	if ( pNewGeneration->pDescriptor->OnLoad ) {
		if ( pNewGeneration->pDescriptor->OnLoad(&pNewGeneration->hPlugin) != 0 ) {
			PS_RuntimeFailGeneration(pPackage, pNewGeneration, "plugin OnLoad failed", bAffectPackage);
			PS_RuntimeCleanupGenerationResources(pNewGeneration);
			if ( pNewGeneration->pDescriptor->OnUnload ) {
				pNewGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pNewGeneration);
			}
			PS_HostDestroyGenerationRouteTokens(pNewGeneration);
			PS_DestroyGeneration(pNewGeneration);
			return FALSE;
		}
	}
	pNewGeneration->hPlugin = (XAdminPluginHandle)pNewGeneration;

	if ( !pPackage->bInstalled && pNewGeneration->pDescriptor->OnInstall ) {
		if ( pNewGeneration->pDescriptor->OnInstall((XAdminPluginHandle)pNewGeneration) != 0 ) {
			PS_RuntimeFailGeneration(pPackage, pNewGeneration, "plugin OnInstall failed", bAffectPackage);
			PS_RuntimeCleanupGenerationResources(pNewGeneration);
			if ( pNewGeneration->pDescriptor->OnUnload ) {
				pNewGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pNewGeneration);
			}
			PS_HostDestroyGenerationRouteTokens(pNewGeneration);
			PS_DestroyGeneration(pNewGeneration);
			return FALSE;
		}
		PS_RuntimePersistInstalled(pPackage);
	}

	if ( pNewGeneration->pDescriptor->OnConfigChanged ) {
		tblConfig = PS_PackageConfigRef(pPackage);
		if ( pNewGeneration->pDescriptor->OnConfigChanged((XAdminPluginHandle)pNewGeneration, tblConfig) != 0 ) {
			if ( tblConfig ) {
				xvoUnref(tblConfig);
				tblConfig = NULL;
			}
			PS_RuntimeFailGeneration(pPackage, pNewGeneration, "plugin OnConfigChanged failed", bAffectPackage);
			PS_RuntimeCleanupGenerationResources(pNewGeneration);
			if ( pNewGeneration->pDescriptor->OnUnload ) {
				pNewGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pNewGeneration);
			}
			PS_HostDestroyGenerationRouteTokens(pNewGeneration);
			PS_DestroyGeneration(pNewGeneration);
			return FALSE;
		}
		if ( tblConfig ) {
			xvoUnref(tblConfig);
			tblConfig = NULL;
		}
	}

	if ( pNewGeneration->pDescriptor->OnStart ) {
		if ( pNewGeneration->pDescriptor->OnStart((XAdminPluginHandle)pNewGeneration) != 0 ) {
			PS_RuntimeFailGeneration(pPackage, pNewGeneration, "plugin OnStart failed", bAffectPackage);
			PS_RuntimeCleanupGenerationResources(pNewGeneration);
			if ( pNewGeneration->pDescriptor->OnUnload ) {
				pNewGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pNewGeneration);
			}
			PS_HostDestroyGenerationRouteTokens(pNewGeneration);
			PS_DestroyGeneration(pNewGeneration);
			return FALSE;
		}
	}

	if ( pNewGeneration->pDescriptor->OnHealthCheck ) {
		memset(&report, 0, sizeof(report));
		if ( pNewGeneration->pDescriptor->OnHealthCheck((XAdminPluginHandle)pNewGeneration, &report) != 0 ) {
			PS_RuntimeFailGeneration(pPackage, pNewGeneration, "plugin health check failed", bAffectPackage);
			if ( pNewGeneration->pDescriptor->OnStop ) {
				pNewGeneration->pDescriptor->OnStop((XAdminPluginHandle)pNewGeneration);
			}
			PS_RuntimeCleanupGenerationResources(pNewGeneration);
			if ( pNewGeneration->pDescriptor->OnUnload ) {
				pNewGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pNewGeneration);
			}
			PS_HostDestroyGenerationRouteTokens(pNewGeneration);
			PS_DestroyGeneration(pNewGeneration);
			return FALSE;
		}
	}

	pNewGeneration->iState = PS_GENERATION_STATE_STARTED;
	if ( pNewGeneration->iStartTime <= 0 ) {
		pNewGeneration->iStartTime = xrtNow();
	}
	PS_RuntimePersistInstalled(pPackage);

	return TRUE;
}

bool PS_RuntimeDiscardPreparedGeneration(PluginSystemPackage* pPackage, PluginSystemGeneration* pGeneration, const char* sMessage, bool bMarkPackageFailed)
{
	if ( pGeneration == NULL ) {
		return FALSE;
	}

	PS_RuntimeFailGeneration(pPackage, pGeneration, (str)(sMessage ? sMessage : "generation activation failed"), bMarkPackageFailed);
	if ( pGeneration->pDescriptor && pGeneration->pDescriptor->OnStop ) {
		pGeneration->pDescriptor->OnStop((XAdminPluginHandle)pGeneration);
	}
	PS_RuntimeCleanupGenerationResources(pGeneration);
	if ( pGeneration->pDescriptor && pGeneration->pDescriptor->OnUnload ) {
		pGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pGeneration);
	}
	PS_HostDestroyGenerationRouteTokens(pGeneration);
	PS_DestroyGeneration(pGeneration);
	return FALSE;
}

bool PS_RuntimeActivateGeneration(PluginSystemPackage* pPackage, PluginSystemGeneration* pGeneration, PluginSystemGeneration* pPreviousGeneration)
{
	bool bPreviousRoutesUnpublished = FALSE;

	if ( (pPackage == NULL) || (pGeneration == NULL) ) {
		return FALSE;
	}

	if ( pPreviousGeneration ) {
		PS_HostUnpublishGenerationRoutes(pPreviousGeneration);
		bPreviousRoutesUnpublished = TRUE;
	}

	if ( !PS_HostPublishGenerationEntryPoints(pGeneration) ) {
		printf("        [PluginSystem] Activate publish failed: package=%s generation=%u\n",
			PS_PackageLogId(pPackage),
			pGeneration->iGeneration);
		if ( bPreviousRoutesUnpublished && !PS_HostRestoreGenerationEntryPoints(pPreviousGeneration) ) {
			printf("        [PluginSystem] Restore previous generation entry points failed: package=%s generation=%u\n",
				PS_PackageLogId(pPackage),
				pPreviousGeneration->iGeneration);
		}
		return FALSE;
	}

	pGeneration->iState = PS_GENERATION_STATE_ACTIVE;
	if ( pGeneration->iStartTime <= 0 ) {
		pGeneration->iStartTime = xrtNow();
	}
	PS_HookPublishGeneration(pGeneration);
	PS_EventPublishGeneration(pGeneration);
	PS_ServicePublishGeneration(pGeneration);

	pPackage->pActiveGeneration = pGeneration;
	pPackage->iActiveGeneration = pGeneration->iGeneration;
	pPackage->iNextGeneration = pGeneration->iGeneration + 1;
	pPackage->bEnabled = TRUE;
	pPackage->iStatus = PS_PACKAGE_STATUS_ACTIVE;
	pPackage->iUpdateTime = xrtNow();

	PS_StorageSaveRuntime(pPackage);
	PS_StorageSaveGeneration(pPackage, pGeneration);
	return TRUE;
}

bool PS_RuntimeStartPackage(PluginSystemPackage* pPackage)
{
	PluginSystemGeneration* pNewGeneration;

	if ( pPackage == NULL ) {
		return FALSE;
	}

	pNewGeneration = PS_CreateGeneration(pPackage->iNextGeneration);
	if ( pNewGeneration == NULL ) {
		return FALSE;
	}

	if ( !PS_RuntimePrepareGeneration(pPackage, pNewGeneration, TRUE) ) {
		printf("        [PluginSystem] Start failed: package=%s\n", PS_PackageLogId(pPackage));
		return FALSE;
	}

	if ( !PS_RuntimeActivateGeneration(pPackage, pNewGeneration, NULL) ) {
		printf("        [PluginSystem] Start failed while activating: package=%s\n", PS_PackageLogId(pPackage));
		return PS_RuntimeDiscardPreparedGeneration(pPackage, pNewGeneration, "plugin activation failed", TRUE);
	}
	printf("        [PluginSystem] Start ok: package=%s generation=%u\n",
		PS_PackageLogId(pPackage),
		pNewGeneration->iGeneration);
	return TRUE;
}

bool PS_RuntimeStopPackage(PluginSystemPackage* pPackage, PluginSystemGeneration* pActorGeneration)
{
	PluginSystemGeneration* pGeneration;

	if ( pPackage == NULL ) {
		return FALSE;
	}

	pGeneration = pPackage->pActiveGeneration;
	if ( pGeneration ) {
		pPackage->pActiveGeneration = NULL;
		PS_RuntimeDeactivateGeneration(pPackage, pGeneration);
	}

	pPackage->bEnabled = FALSE;
	pPackage->iStatus = PS_PACKAGE_STATUS_DISABLED;
	pPackage->iActiveGeneration = 0;
	pPackage->iUpdateTime = xrtNow();
	PS_StorageSaveRuntime(pPackage);
	PS_RuntimeWaitForPackageDrain(pPackage, pActorGeneration);
	return TRUE;
}

bool PS_RuntimeReloadPackage(PluginSystemPackage* pPackage, PluginSystemGeneration* pActorGeneration)
{
	PluginSystemGeneration* pOldGeneration;
	PluginSystemGeneration* pNewGeneration;

	if ( (pPackage == NULL) || !pPackage->bEnabled ) {
		return FALSE;
	}

	pOldGeneration = pPackage->pActiveGeneration;
	pNewGeneration = PS_CreateGeneration(pPackage->iNextGeneration);
	if ( pNewGeneration == NULL ) {
		return FALSE;
	}

	if ( !PS_RuntimePrepareGeneration(pPackage, pNewGeneration, FALSE) ) {
		printf("        [PluginSystem] Reload failed while preparing: package=%s\n", PS_PackageLogId(pPackage));
		return FALSE;
	}

	if ( !PS_RuntimeActivateGeneration(pPackage, pNewGeneration, pOldGeneration) ) {
		printf("        [PluginSystem] Reload failed while activating: package=%s\n", PS_PackageLogId(pPackage));
		return PS_RuntimeDiscardPreparedGeneration(pPackage, pNewGeneration, "plugin activation failed", FALSE);
	}
	if ( pOldGeneration ) {
		PS_RuntimeDeactivateGeneration(pPackage, pOldGeneration);
	}
	PS_RuntimeWaitForPackageDrain(pPackage, pActorGeneration);
	printf("        [PluginSystem] Reload ok: package=%s generation=%u\n",
		PS_PackageLogId(pPackage),
		pNewGeneration->iGeneration);
	return TRUE;
}

#endif
