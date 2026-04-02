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
			xrtListSetPtr(pGeneration->lstRouteTokens, i, NULL, NULL);
		}
	}
}

bool PS_RuntimeFailInstance(PluginSystemInstance* pInstance, PluginSystemGeneration* pGeneration, str sMessage, bool bMarkInstanceFailed)
{
	printf("        [PluginSystem] Generation failed: instance=%s generation=%u reason=%s\n",
		(pInstance && pInstance->sInstanceId) ? (const char*)pInstance->sInstanceId : "(unknown)",
		pGeneration ? pGeneration->iGeneration : 0,
		sMessage ? (const char*)sMessage : "(null)");

	if ( pGeneration ) {
		PS_FreeString(&pGeneration->sErrorMessage);
		pGeneration->sErrorMessage = xrtCopyStr(sMessage, 0);
		pGeneration->iState = PS_GENERATION_STATE_FAILED;
		PS_StorageSaveGeneration(pInstance, pGeneration);
	}
	if ( bMarkInstanceFailed && pInstance ) {
		pInstance->iStatus = PS_INSTANCE_STATUS_FAILED;
		pInstance->iUpdateTime = xrtNow();
		PS_StorageSaveInstance(pInstance);
	}
	return FALSE;
}

void PS_RuntimeDeactivateGeneration(PluginSystemInstance* pInstance, PluginSystemGeneration* pGeneration)
{
	if ( pGeneration == NULL ) {
		return;
	}

	printf("        [PluginSystem] Stopping generation: instance=%s generation=%u state=%d\n",
		(pInstance && pInstance->sInstanceId) ? (const char*)pInstance->sInstanceId : "(unknown)",
		pGeneration->iGeneration,
		pGeneration->iState);

	if ( pGeneration->pDescriptor && pGeneration->pDescriptor->OnStop ) {
		pGeneration->pDescriptor->OnStop((XAdminPluginHandle)pGeneration);
	}
	PS_RuntimeCleanupRoutes(pGeneration);
	if ( pGeneration->pDescriptor && pGeneration->pDescriptor->OnUnload ) {
		pGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pGeneration);
	}
	pGeneration->iStopTime = xrtNow();
	pGeneration->iState = PS_GENERATION_STATE_STOPPED;
	PS_StorageSaveGeneration(pInstance, pGeneration);
	PS_DestroyGeneration(pGeneration);
}

bool PS_RuntimePrepareGeneration(PluginSystemPackage* pPackage, PluginSystemInstance* pInstance, PluginSystemGeneration* pNewGeneration, bool bAffectInstance)
{
	XAdminHealthReport report;

	if ( (pPackage == NULL) || (pInstance == NULL) ) {
		return FALSE;
	}

	if ( pNewGeneration == NULL ) {
		return FALSE;
	}

	printf("        [PluginSystem] Preparing generation: package=%s instance=%s generation=%u\n",
		PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)",
		pInstance->sInstanceId ? (const char*)pInstance->sInstanceId : "(unknown)",
		pNewGeneration->iGeneration);

	if ( !PS_CompileGeneration(pPackage, pInstance, pNewGeneration) ) {
		PS_RuntimeFailInstance(pInstance, pNewGeneration, pNewGeneration->sErrorMessage ? pNewGeneration->sErrorMessage : (str)"compile failed", bAffectInstance);
		PS_DestroyGeneration(pNewGeneration);
		return FALSE;
	}

	pNewGeneration->iState = PS_GENERATION_STATE_LOADED;
	pNewGeneration->hPlugin = (XAdminPluginHandle)pNewGeneration;
	if ( pNewGeneration->pDescriptor->OnLoad ) {
		if ( pNewGeneration->pDescriptor->OnLoad(&G_PluginSystemHostAPI, &pNewGeneration->hPlugin) != 0 ) {
			PS_RuntimeFailInstance(pInstance, pNewGeneration, "plugin OnLoad failed", bAffectInstance);
			if ( pNewGeneration->pDescriptor->OnUnload ) {
				pNewGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pNewGeneration);
			}
			PS_DestroyGeneration(pNewGeneration);
			return FALSE;
		}
	}
	pNewGeneration->hPlugin = (XAdminPluginHandle)pNewGeneration;

	if ( !pInstance->bInstalled && pNewGeneration->pDescriptor->OnInstall ) {
		if ( pNewGeneration->pDescriptor->OnInstall((XAdminPluginHandle)pNewGeneration) != 0 ) {
			PS_RuntimeFailInstance(pInstance, pNewGeneration, "plugin OnInstall failed", bAffectInstance);
			if ( pNewGeneration->pDescriptor->OnUnload ) {
				pNewGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pNewGeneration);
			}
			PS_DestroyGeneration(pNewGeneration);
			return FALSE;
		}
		pInstance->bInstalled = TRUE;
	}

	if ( pNewGeneration->pDescriptor->OnConfigChanged ) {
		if ( pNewGeneration->pDescriptor->OnConfigChanged((XAdminPluginHandle)pNewGeneration, pInstance->tblConfig) != 0 ) {
			PS_RuntimeFailInstance(pInstance, pNewGeneration, "plugin OnConfigChanged failed", bAffectInstance);
			if ( pNewGeneration->pDescriptor->OnUnload ) {
				pNewGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pNewGeneration);
			}
			PS_DestroyGeneration(pNewGeneration);
			return FALSE;
		}
	}

	if ( pNewGeneration->pDescriptor->OnStart ) {
		if ( pNewGeneration->pDescriptor->OnStart((XAdminPluginHandle)pNewGeneration) != 0 ) {
			PS_RuntimeFailInstance(pInstance, pNewGeneration, "plugin OnStart failed", bAffectInstance);
			PS_RuntimeCleanupRoutes(pNewGeneration);
			if ( pNewGeneration->pDescriptor->OnUnload ) {
				pNewGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pNewGeneration);
			}
			PS_DestroyGeneration(pNewGeneration);
			return FALSE;
		}
	}

	if ( pNewGeneration->pDescriptor->OnHealthCheck ) {
		memset(&report, 0, sizeof(report));
		if ( pNewGeneration->pDescriptor->OnHealthCheck((XAdminPluginHandle)pNewGeneration, &report) != 0 ) {
			PS_RuntimeFailInstance(pInstance, pNewGeneration, "plugin health check failed", bAffectInstance);
			PS_RuntimeCleanupRoutes(pNewGeneration);
			if ( pNewGeneration->pDescriptor->OnStop ) {
				pNewGeneration->pDescriptor->OnStop((XAdminPluginHandle)pNewGeneration);
			}
			if ( pNewGeneration->pDescriptor->OnUnload ) {
				pNewGeneration->pDescriptor->OnUnload((XAdminPluginHandle)pNewGeneration);
			}
			PS_DestroyGeneration(pNewGeneration);
			return FALSE;
		}
	}

	pNewGeneration->iState = PS_GENERATION_STATE_STARTED;
	if ( pNewGeneration->iStartTime <= 0 ) {
		pNewGeneration->iStartTime = xrtNow();
	}

	return TRUE;
}

void PS_RuntimeActivateGeneration(PluginSystemInstance* pInstance, PluginSystemGeneration* pGeneration)
{
	if ( (pInstance == NULL) || (pGeneration == NULL) ) {
		return;
	}

	pGeneration->iState = PS_GENERATION_STATE_ACTIVE;
	if ( pGeneration->iStartTime <= 0 ) {
		pGeneration->iStartTime = xrtNow();
	}

	pInstance->pActiveGeneration = pGeneration;
	pInstance->iActiveGeneration = pGeneration->iGeneration;
	pInstance->iNextGeneration = pGeneration->iGeneration + 1;
	pInstance->bEnabled = TRUE;
	pInstance->iStatus = PS_INSTANCE_STATUS_ACTIVE;
	pInstance->iUpdateTime = xrtNow();

	PS_StorageSaveInstance(pInstance);
	PS_StorageSaveGeneration(pInstance, pGeneration);
}

bool PS_RuntimeStartInstance(PluginSystemPackage* pPackage, PluginSystemInstance* pInstance)
{
	PluginSystemGeneration* pNewGeneration;

	if ( (pPackage == NULL) || (pInstance == NULL) ) {
		return FALSE;
	}

	pNewGeneration = PS_CreateGeneration(pInstance->iNextGeneration);
	if ( pNewGeneration == NULL ) {
		return FALSE;
	}

	if ( !PS_RuntimePrepareGeneration(pPackage, pInstance, pNewGeneration, TRUE) ) {
		printf("        [PluginSystem] Start failed: package=%s instance=%s\n",
			PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)",
			pInstance->sInstanceId ? (const char*)pInstance->sInstanceId : "(unknown)");
		return FALSE;
	}

	PS_RuntimeActivateGeneration(pInstance, pNewGeneration);
	printf("        [PluginSystem] Start ok: package=%s instance=%s generation=%u\n",
		PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)",
		pInstance->sInstanceId ? (const char*)pInstance->sInstanceId : "(unknown)",
		pNewGeneration->iGeneration);
	return TRUE;
}

bool PS_RuntimeStopInstance(PluginSystemInstance* pInstance)
{
	PluginSystemGeneration* pGeneration;

	if ( pInstance == NULL ) {
		return FALSE;
	}

	pGeneration = pInstance->pActiveGeneration;
	if ( pGeneration ) {
		pInstance->pActiveGeneration = NULL;
		PS_RuntimeDeactivateGeneration(pInstance, pGeneration);
	}

	pInstance->bEnabled = FALSE;
	pInstance->iStatus = PS_INSTANCE_STATUS_DISABLED;
	pInstance->iActiveGeneration = 0;
	pInstance->iUpdateTime = xrtNow();
	PS_StorageSaveInstance(pInstance);
	return TRUE;
}

bool PS_RuntimeReloadInstance(PluginSystemPackage* pPackage, PluginSystemInstance* pInstance)
{
	PluginSystemGeneration* pOldGeneration;
	PluginSystemGeneration* pNewGeneration;

	if ( (pInstance == NULL) || !pInstance->bEnabled ) {
		return FALSE;
	}

	pOldGeneration = pInstance->pActiveGeneration;
	pNewGeneration = PS_CreateGeneration(pInstance->iNextGeneration);
	if ( pNewGeneration == NULL ) {
		return FALSE;
	}

	if ( !PS_RuntimePrepareGeneration(pPackage, pInstance, pNewGeneration, FALSE) ) {
		printf("        [PluginSystem] Reload failed while preparing: package=%s instance=%s\n",
			PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)",
			pInstance->sInstanceId ? (const char*)pInstance->sInstanceId : "(unknown)");
		return FALSE;
	}

	PS_RuntimeActivateGeneration(pInstance, pNewGeneration);
	if ( pOldGeneration ) {
		PS_RuntimeDeactivateGeneration(pInstance, pOldGeneration);
	}
	printf("        [PluginSystem] Reload ok: package=%s instance=%s generation=%u\n",
		PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)",
		pInstance->sInstanceId ? (const char*)pInstance->sInstanceId : "(unknown)",
		pNewGeneration->iGeneration);
	return TRUE;
}

#endif
