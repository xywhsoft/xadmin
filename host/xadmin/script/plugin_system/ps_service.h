#ifndef XADMIN_PLUGIN_SYSTEM_SERVICE_H
#define XADMIN_PLUGIN_SYSTEM_SERVICE_H

#include "ps_storage.h"

void PS_RuntimeOnGenerationRefReleased(PluginSystemGeneration* pGeneration);

bool PS_ServiceTextEquals(const char* sLeft, const char* sRight)
{
	if ( sLeft == sRight ) {
		return TRUE;
	}
	if ( (sLeft == NULL) || (sRight == NULL) ) {
		return FALSE;
	}
	return strcmp(sLeft, sRight) == 0;
}

void PS_ServiceAppendPtr(xlist lstItems, ptr pVal)
{
	if ( (lstItems == NULL) || (pVal == NULL) ) {
		return;
	}
	xrtListSetPtr(lstItems, xrtListCount(lstItems), pVal, NULL);
}

void PS_ServiceDetachPtr(xlist lstItems, ptr pVal)
{
	if ( (lstItems == NULL) || (pVal == NULL) ) {
		return;
	}

	for ( int i = xrtListCount(lstItems) - 1; i >= 0; i-- ) {
		if ( xrtListGetPtr(lstItems, i) == pVal ) {
			xrtListSetPtr(lstItems, i, NULL, NULL);
			return;
		}
	}
}

void PS_ServiceRebuildPublishedSnapshot()
{
	PluginSystemServiceRegistration** ppSnapshot = NULL;
	int iCount = 0;
	int iWrite = 0;

	if ( (G_PluginSystem == NULL) || (G_PluginSystem->lstServices == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(G_PluginSystem->lstServices); i++ ) {
		PluginSystemServiceRegistration* pRegistration = xrtListGetPtr(G_PluginSystem->lstServices, i);
		if ( pRegistration && pRegistration->bPublished ) {
			iCount++;
		}
	}

	ppSnapshot = xrtMalloc(sizeof(ptr) * (iCount + 1));
	if ( ppSnapshot == NULL ) {
		return;
	}
	memset(ppSnapshot, 0, sizeof(ptr) * (iCount + 1));

	for ( int i = 0; i < xrtListCount(G_PluginSystem->lstServices); i++ ) {
		PluginSystemServiceRegistration* pRegistration = xrtListGetPtr(G_PluginSystem->lstServices, i);
		if ( pRegistration && pRegistration->bPublished ) {
			ppSnapshot[iWrite++] = pRegistration;
		}
	}

	G_PluginSystem->ppPublishedServices = ppSnapshot;
	if ( G_PluginSystem->lstServiceSnapshots ) {
		xrtListSetPtr(G_PluginSystem->lstServiceSnapshots, xrtListCount(G_PluginSystem->lstServiceSnapshots), ppSnapshot, NULL);
	}
}

str PS_ServiceMakeKey(const char* sServiceName, int iMajorVersion)
{
	if ( (sServiceName == NULL) || (sServiceName[0] == '\0') ) {
		return NULL;
	}
	return xrtFormat("%s@%d", sServiceName, iMajorVersion);
}

PluginSystemServiceRegistration* PS_ServiceCreateRegistration(PluginSystemGeneration* pGeneration, const XAdminServiceDecl* decl, const void* pVTable)
{
	PluginSystemServiceRegistration* pRegistration;

	if ( (pGeneration == NULL) || (decl == NULL) || (decl->service_name == NULL) || (decl->service_name[0] == '\0') || (pVTable == NULL) ) {
		return NULL;
	}

	pRegistration = xrtMalloc(sizeof(PluginSystemServiceRegistration));
	if ( pRegistration == NULL ) {
		return NULL;
	}
	memset(pRegistration, 0, sizeof(PluginSystemServiceRegistration));

	const char* sProviderInstanceId = decl->provider_instance_id ? decl->provider_instance_id : (const char*)pGeneration->pInstance->sInstanceId;
	const char* sCapabilitiesRequired = decl->capabilities_required ? decl->capabilities_required : "";

	pRegistration->pGeneration = pGeneration;
	pRegistration->sServiceName = xrtCopyStr((str)decl->service_name, 0);
	pRegistration->sProviderInstanceId = xrtCopyStr((str)sProviderInstanceId, 0);
	pRegistration->sCapabilitiesRequired = xrtCopyStr((str)sCapabilitiesRequired, 0);
	pRegistration->iMajorVersion = decl->major_version;
	pRegistration->iMinorVersion = decl->minor_version;
	pRegistration->iLifecycleScope = decl->lifecycle_scope;
	pRegistration->iVtableSize = decl->vtable_size;
	pRegistration->pVtable = pVTable;
	pRegistration->bPublished = FALSE;

	if ( (pRegistration->sServiceName == NULL) || (pRegistration->sProviderInstanceId == NULL) || (pRegistration->sCapabilitiesRequired == NULL) ) {
		PS_FreeString(&pRegistration->sServiceName);
		PS_FreeString(&pRegistration->sProviderInstanceId);
		PS_FreeString(&pRegistration->sCapabilitiesRequired);
		xrtFree(pRegistration);
		return NULL;
	}

	return pRegistration;
}

void PS_ServiceFreeRegistration(PluginSystemServiceRegistration* pRegistration)
{
	if ( pRegistration == NULL ) {
		return;
	}

	PS_FreeString(&pRegistration->sServiceName);
	PS_FreeString(&pRegistration->sProviderInstanceId);
	PS_FreeString(&pRegistration->sCapabilitiesRequired);
	xrtFree(pRegistration);
}

void PS_ServiceUnpublishRegistration(PluginSystemServiceRegistration* pRegistration, const char* sStatus)
{
	if ( pRegistration == NULL ) {
		return;
	}

	pRegistration->bPublished = FALSE;
	PS_StorageUpdateServiceStatus(pRegistration->iServiceRowId, sStatus ? sStatus : "draining");
	PS_ServiceRebuildPublishedSnapshot();
}

void PS_ServicePublishRegistration(PluginSystemServiceRegistration* pRegistration)
{
	if ( pRegistration == NULL ) {
		return;
	}

	if ( !pRegistration->bListed && G_PluginSystem && G_PluginSystem->lstServices ) {
		PS_ServiceAppendPtr(G_PluginSystem->lstServices, pRegistration);
		pRegistration->bListed = TRUE;
	}

	pRegistration->bPublished = TRUE;
	PS_StorageUpdateServiceStatus(pRegistration->iServiceRowId, "active");
	PS_ServiceRebuildPublishedSnapshot();
}

void PS_ServiceDestroyRegistration(PluginSystemServiceRegistration* pRegistration)
{
	if ( pRegistration == NULL ) {
		return;
	}

	PS_ServiceUnpublishRegistration(pRegistration, "removed");
	PS_StorageUpdateResourceStatus(pRegistration->iResourceId, "removed");
	if ( !pRegistration->bListed ) {
		PS_ServiceFreeRegistration(pRegistration);
		return;
	}
	pRegistration->pGeneration = NULL;
	pRegistration->pVtable = NULL;
}

PluginSystemServiceRegistration* PS_ServiceResolve(const char* sServiceName, int iMajorVersion)
{
	PluginSystemServiceRegistration* pBest = NULL;
	PluginSystemServiceRegistration** ppSnapshot;

	if ( (G_PluginSystem == NULL) || (sServiceName == NULL) || (sServiceName[0] == '\0') ) {
		return NULL;
	}
	ppSnapshot = G_PluginSystem->ppPublishedServices;
	if ( ppSnapshot == NULL ) {
		return NULL;
	}

	for ( int i = 0; ppSnapshot[i] != NULL; i++ ) {
		PluginSystemServiceRegistration* pRegistration = ppSnapshot[i];
		if ( (pRegistration == NULL) || !pRegistration->bPublished || (pRegistration->pGeneration == NULL) ) {
			continue;
		}
		if ( !PS_ServiceTextEquals(pRegistration->sServiceName, sServiceName) || (pRegistration->iMajorVersion != iMajorVersion) ) {
			continue;
		}
		if ( pRegistration->pGeneration->iState != PS_GENERATION_STATE_ACTIVE ) {
			continue;
		}

		if ( (pBest == NULL)
			|| (pRegistration->iMinorVersion > pBest->iMinorVersion)
			|| ((pRegistration->iMinorVersion == pBest->iMinorVersion) && (pRegistration->pGeneration->iStartTime > pBest->pGeneration->iStartTime)) ) {
			pBest = pRegistration;
		}
	}

	return pBest;
}

void PS_ServicePublishGeneration(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstServiceRegistrations == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstServiceRegistrations); i++ ) {
		PluginSystemServiceRegistration* pRegistration = xrtListGetPtr(pGeneration->lstServiceRegistrations, i);
		if ( pRegistration ) {
			PS_ServicePublishRegistration(pRegistration);
		}
	}
}

void PS_ServiceUnpublishGeneration(PluginSystemGeneration* pGeneration, const char* sStatus)
{
	if ( (pGeneration == NULL) || (pGeneration->lstServiceRegistrations == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstServiceRegistrations); i++ ) {
		PluginSystemServiceRegistration* pRegistration = xrtListGetPtr(pGeneration->lstServiceRegistrations, i);
		if ( pRegistration ) {
			PS_ServiceUnpublishRegistration(pRegistration, sStatus ? sStatus : "draining");
		}
	}
}

void PS_ServiceDestroyGenerationRegistrations(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstServiceRegistrations == NULL) ) {
		return;
	}

	for ( int i = xrtListCount(pGeneration->lstServiceRegistrations) - 1; i >= 0; i-- ) {
		PluginSystemServiceRegistration* pRegistration = xrtListGetPtr(pGeneration->lstServiceRegistrations, i);
		if ( pRegistration ) {
			PS_ServiceDestroyRegistration(pRegistration);
		}
		xrtListSetPtr(pGeneration->lstServiceRegistrations, i, NULL, NULL);
	}
}

int PS_HostRegisterService(void* plugin_handle, const XAdminServiceDecl* decl, const void* vtable)
{
	PluginSystemGeneration* pGeneration = (PluginSystemGeneration*)plugin_handle;
	PluginSystemServiceRegistration* pRegistration;
	str sResourceKey;

	if ( (pGeneration == NULL) || (pGeneration->pInstance == NULL) || (decl == NULL) || (decl->service_name == NULL) || (decl->service_name[0] == '\0') || (decl->major_version < 0) || (decl->minor_version < 0) || (decl->vtable_size <= 0) || (vtable == NULL) ) {
		return -1;
	}
	if ( decl->provider_instance_id && !PS_ServiceTextEquals(decl->provider_instance_id, pGeneration->pInstance->sInstanceId) ) {
		return -1;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstServiceRegistrations); i++ ) {
		PluginSystemServiceRegistration* pExisting = xrtListGetPtr(pGeneration->lstServiceRegistrations, i);
		if ( pExisting && PS_ServiceTextEquals(pExisting->sServiceName, decl->service_name) && (pExisting->iMajorVersion == decl->major_version) ) {
			return -1;
		}
	}

	pRegistration = PS_ServiceCreateRegistration(pGeneration, decl, vtable);
	if ( pRegistration == NULL ) {
		return -1;
	}

	sResourceKey = PS_ServiceMakeKey(decl->service_name, decl->major_version);
	pRegistration->iResourceId = PS_StorageTrackResource(
		pGeneration,
		"generation",
		"service",
		sResourceKey ? sResourceKey : (str)decl->service_name,
		pRegistration->sProviderInstanceId,
		"lease_drain"
	);
	pRegistration->iServiceRowId = PS_StorageSaveService(pGeneration, decl->service_name, decl->major_version, decl->minor_version, "staged");
	if ( sResourceKey ) {
		xrtFree(sResourceKey);
	}
	if ( pRegistration->iServiceRowId <= 0 ) {
		PS_ServiceFreeRegistration(pRegistration);
		return -1;
	}

	PS_ServiceAppendPtr(pGeneration->lstServiceRegistrations, pRegistration);
	if ( pGeneration->iState == PS_GENERATION_STATE_ACTIVE ) {
		PS_ServicePublishRegistration(pRegistration);
	}
	return 0;
}

int PS_HostAcquireService(void* plugin_handle, const char* name, int major, XAdminServiceLease* out_lease, const void** out_vtable)
{
	PluginSystemGeneration* pConsumerGeneration = (PluginSystemGeneration*)plugin_handle;
	PluginSystemServiceRegistration* pRegistration;
	PluginSystemServiceLeaseData* pLease;

	if ( out_lease ) {
		*out_lease = NULL;
	}
	if ( out_vtable ) {
		*out_vtable = NULL;
	}
	if ( (pConsumerGeneration == NULL) || (name == NULL) || (name[0] == '\0') || (out_lease == NULL) || (out_vtable == NULL) ) {
		return -1;
	}

	pRegistration = PS_ServiceResolve(name, major);
	if ( (pRegistration == NULL) || (pRegistration->pVtable == NULL) || (pRegistration->pGeneration == NULL) ) {
		return -1;
	}

	pLease = xrtMalloc(sizeof(PluginSystemServiceLeaseData));
	if ( pLease == NULL ) {
		return -1;
	}
	memset(pLease, 0, sizeof(PluginSystemServiceLeaseData));

	pLease->pConsumerGeneration = pConsumerGeneration;
	pLease->pProviderGeneration = pRegistration->pGeneration;
	pLease->pRegistration = pRegistration;
	pRegistration->pGeneration->iRefCount++;

	*out_lease = (XAdminServiceLease)pLease;
	*out_vtable = pRegistration->pVtable;
	return 0;
}

int PS_HostReleaseService(XAdminServiceLease lease)
{
	PluginSystemServiceLeaseData* pLease = (PluginSystemServiceLeaseData*)lease;
	PluginSystemGeneration* pProviderGeneration;

	if ( pLease == NULL ) {
		return -1;
	}
	if ( pLease->bReleased ) {
		return 0;
	}

	pLease->bReleased = TRUE;
	pProviderGeneration = pLease->pProviderGeneration;
	if ( pProviderGeneration && (pProviderGeneration->iRefCount > 0) ) {
		pProviderGeneration->iRefCount--;
		if ( (pProviderGeneration->iState == PS_GENERATION_STATE_DRAINING) && (pProviderGeneration->iRefCount <= 0) ) {
			PS_RuntimeOnGenerationRefReleased(pProviderGeneration);
		}
	}

	xrtFree(pLease);
	return 0;
}

#endif
