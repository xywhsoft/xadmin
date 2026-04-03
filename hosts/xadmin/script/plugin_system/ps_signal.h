#ifndef XADMIN_PLUGIN_SYSTEM_SIGNAL_H
#define XADMIN_PLUGIN_SYSTEM_SIGNAL_H

#include "ps_storage.h"

void PS_RuntimeOnGenerationRefReleased(PluginSystemGeneration* pGeneration);

bool PS_SignalTextEquals(const char* sLeft, const char* sRight)
{
	if ( sLeft == sRight ) {
		return TRUE;
	}
	if ( (sLeft == NULL) || (sRight == NULL) ) {
		return FALSE;
	}
	return strcmp(sLeft, sRight) == 0;
}

void PS_SignalAppendPtr(xlist lstItems, ptr pVal)
{
	if ( (lstItems == NULL) || (pVal == NULL) ) {
		return;
	}
	xrtListSetPtr(lstItems, xrtListCount(lstItems), pVal, NULL);
}

void PS_SignalDetachPtr(xlist lstItems, ptr pVal)
{
	if ( (lstItems == NULL) || (pVal == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(lstItems); i++ ) {
		if ( xrtListGetPtr(lstItems, i) == pVal ) {
			xrtListSetPtr(lstItems, i, NULL, NULL);
			return;
		}
	}
}

void PS_SignalFormatPtr(const void* pValue, char sBuf[64])
{
	snprintf(sBuf, 64, "%p", pValue);
}

void PS_EventRebuildPublishedSnapshot()
{
	PluginSystemEventRegistration** ppSnapshot = NULL;
	int iCount = 0;
	int iWrite = 0;

	if ( (G_PluginSystem == NULL) || (G_PluginSystem->lstEvents == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(G_PluginSystem->lstEvents); i++ ) {
		PluginSystemEventRegistration* pRegistration = xrtListGetPtr(G_PluginSystem->lstEvents, i);
		if ( pRegistration && pRegistration->bPublished ) {
			iCount++;
		}
	}

	ppSnapshot = xrtMalloc(sizeof(ptr) * (iCount + 1));
	if ( ppSnapshot == NULL ) {
		return;
	}
	memset(ppSnapshot, 0, sizeof(ptr) * (iCount + 1));

	for ( int i = 0; i < xrtListCount(G_PluginSystem->lstEvents); i++ ) {
		PluginSystemEventRegistration* pRegistration = xrtListGetPtr(G_PluginSystem->lstEvents, i);
		if ( pRegistration && pRegistration->bPublished ) {
			ppSnapshot[iWrite++] = pRegistration;
		}
	}

	G_PluginSystem->ppPublishedEvents = ppSnapshot;
	if ( G_PluginSystem->lstEventSnapshots ) {
		xrtListSetPtr(G_PluginSystem->lstEventSnapshots, xrtListCount(G_PluginSystem->lstEventSnapshots), ppSnapshot, NULL);
	}
}

int PS_HookCompare(PluginSystemHookRegistration* pLeft, PluginSystemHookRegistration* pRight)
{
	int iNameCompare;
	const char* sLeftName;
	const char* sRightName;

	if ( pLeft == pRight ) {
		return 0;
	}
	if ( pLeft == NULL ) {
		return 1;
	}
	if ( pRight == NULL ) {
		return -1;
	}

	sLeftName = pLeft->sHookName ? (const char*)pLeft->sHookName : "";
	sRightName = pRight->sHookName ? (const char*)pRight->sHookName : "";
	iNameCompare = strcmp(sLeftName, sRightName);
	if ( iNameCompare != 0 ) {
		return iNameCompare;
	}
	if ( pLeft->iSort != pRight->iSort ) {
		return pLeft->iSort - pRight->iSort;
	}
	if ( pLeft->pGeneration && pRight->pGeneration && (pLeft->pGeneration->iGeneration != pRight->pGeneration->iGeneration) ) {
		return (int)pLeft->pGeneration->iGeneration - (int)pRight->pGeneration->iGeneration;
	}
	return 0;
}

void PS_HookRebuildPublishedSnapshot()
{
	PluginSystemHookRegistration** ppSnapshot = NULL;
	int iCount = 0;
	int iWrite = 0;

	if ( (G_PluginSystem == NULL) || (G_PluginSystem->lstHooks == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(G_PluginSystem->lstHooks); i++ ) {
		PluginSystemHookRegistration* pRegistration = xrtListGetPtr(G_PluginSystem->lstHooks, i);
		if ( pRegistration && pRegistration->bPublished ) {
			iCount++;
		}
	}

	ppSnapshot = xrtMalloc(sizeof(ptr) * (iCount + 1));
	if ( ppSnapshot == NULL ) {
		return;
	}
	memset(ppSnapshot, 0, sizeof(ptr) * (iCount + 1));

	for ( int i = 0; i < xrtListCount(G_PluginSystem->lstHooks); i++ ) {
		PluginSystemHookRegistration* pRegistration = xrtListGetPtr(G_PluginSystem->lstHooks, i);
		if ( pRegistration && pRegistration->bPublished ) {
			ppSnapshot[iWrite++] = pRegistration;
		}
	}

	for ( int i = 1; i < iWrite; i++ ) {
		PluginSystemHookRegistration* pCurrent = ppSnapshot[i];
		int j = i - 1;
		while ( (j >= 0) && (PS_HookCompare(ppSnapshot[j], pCurrent) > 0) ) {
			ppSnapshot[j + 1] = ppSnapshot[j];
			j--;
		}
		ppSnapshot[j + 1] = pCurrent;
	}

	G_PluginSystem->ppPublishedHooks = ppSnapshot;
	if ( G_PluginSystem->lstHookSnapshots ) {
		xrtListSetPtr(G_PluginSystem->lstHookSnapshots, xrtListCount(G_PluginSystem->lstHookSnapshots), ppSnapshot, NULL);
	}
}

PluginSystemEventRegistration* PS_EventCreateRegistration(PluginSystemGeneration* pGeneration, const XAdminEventDecl* decl)
{
	PluginSystemEventRegistration* pRegistration;

	if ( (pGeneration == NULL) || (decl == NULL) || (decl->event_name == NULL) || (decl->event_name[0] == '\0') || (decl->proc == NULL) ) {
		return NULL;
	}

	pRegistration = xrtMalloc(sizeof(PluginSystemEventRegistration));
	if ( pRegistration == NULL ) {
		return NULL;
	}
	memset(pRegistration, 0, sizeof(PluginSystemEventRegistration));

	pRegistration->pGeneration = pGeneration;
	pRegistration->sEventName = xrtCopyStr((str)decl->event_name, 0);
	pRegistration->pProc = decl->proc;
	if ( pRegistration->sEventName == NULL ) {
		xrtFree(pRegistration);
		return NULL;
	}

	return pRegistration;
}

void PS_EventFreeRegistration(PluginSystemEventRegistration* pRegistration)
{
	if ( pRegistration == NULL ) {
		return;
	}

	PS_FreeString(&pRegistration->sEventName);
	xrtFree(pRegistration);
}

void PS_EventUnpublishRegistration(PluginSystemEventRegistration* pRegistration)
{
	if ( pRegistration == NULL ) {
		return;
	}

	pRegistration->bPublished = FALSE;
	PS_EventRebuildPublishedSnapshot();
}

void PS_EventPublishRegistration(PluginSystemEventRegistration* pRegistration)
{
	if ( pRegistration == NULL ) {
		return;
	}

	if ( !pRegistration->bListed && G_PluginSystem && G_PluginSystem->lstEvents ) {
		PS_SignalAppendPtr(G_PluginSystem->lstEvents, pRegistration);
		pRegistration->bListed = TRUE;
	}
	pRegistration->bPublished = TRUE;
	PS_EventRebuildPublishedSnapshot();
}

void PS_EventDestroyRegistration(PluginSystemEventRegistration* pRegistration)
{
	if ( pRegistration == NULL ) {
		return;
	}

	PS_EventUnpublishRegistration(pRegistration);
	PS_StorageUpdateResourceStatus(pRegistration->iResourceId, "removed");
	if ( !pRegistration->bListed ) {
		PS_EventFreeRegistration(pRegistration);
		return;
	}
	pRegistration->pGeneration = NULL;
	pRegistration->pProc = NULL;
}

void PS_EventPublishGeneration(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstEventRegistrations == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstEventRegistrations); i++ ) {
		PluginSystemEventRegistration* pRegistration = xrtListGetPtr(pGeneration->lstEventRegistrations, i);
		if ( pRegistration ) {
			PS_EventPublishRegistration(pRegistration);
		}
	}
}

void PS_EventUnpublishGeneration(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstEventRegistrations == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstEventRegistrations); i++ ) {
		PluginSystemEventRegistration* pRegistration = xrtListGetPtr(pGeneration->lstEventRegistrations, i);
		if ( pRegistration ) {
			PS_EventUnpublishRegistration(pRegistration);
		}
	}
}

void PS_EventDestroyGenerationRegistrations(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstEventRegistrations == NULL) ) {
		return;
	}

	for ( int i = xrtListCount(pGeneration->lstEventRegistrations) - 1; i >= 0; i-- ) {
		PluginSystemEventRegistration* pRegistration = xrtListGetPtr(pGeneration->lstEventRegistrations, i);
		if ( pRegistration ) {
			PS_EventDestroyRegistration(pRegistration);
		}
		xrtListSetPtr(pGeneration->lstEventRegistrations, i, NULL, NULL);
	}
}

PluginSystemHookRegistration* PS_HookCreateRegistration(PluginSystemGeneration* pGeneration, const XAdminHookDecl* decl)
{
	PluginSystemHookRegistration* pRegistration;

	if ( (pGeneration == NULL) || (decl == NULL) || (decl->hook_name == NULL) || (decl->hook_name[0] == '\0') || (decl->proc == NULL) ) {
		return NULL;
	}

	pRegistration = xrtMalloc(sizeof(PluginSystemHookRegistration));
	if ( pRegistration == NULL ) {
		return NULL;
	}
	memset(pRegistration, 0, sizeof(PluginSystemHookRegistration));

	pRegistration->pGeneration = pGeneration;
	pRegistration->sHookName = xrtCopyStr((str)decl->hook_name, 0);
	pRegistration->iSort = decl->sort;
	pRegistration->pProc = decl->proc;
	if ( pRegistration->sHookName == NULL ) {
		xrtFree(pRegistration);
		return NULL;
	}

	return pRegistration;
}

void PS_HookFreeRegistration(PluginSystemHookRegistration* pRegistration)
{
	if ( pRegistration == NULL ) {
		return;
	}

	PS_FreeString(&pRegistration->sHookName);
	xrtFree(pRegistration);
}

void PS_HookUnpublishRegistration(PluginSystemHookRegistration* pRegistration)
{
	if ( pRegistration == NULL ) {
		return;
	}

	pRegistration->bPublished = FALSE;
	PS_HookRebuildPublishedSnapshot();
}

void PS_HookPublishRegistration(PluginSystemHookRegistration* pRegistration)
{
	if ( pRegistration == NULL ) {
		return;
	}

	if ( !pRegistration->bListed && G_PluginSystem && G_PluginSystem->lstHooks ) {
		PS_SignalAppendPtr(G_PluginSystem->lstHooks, pRegistration);
		pRegistration->bListed = TRUE;
	}
	pRegistration->bPublished = TRUE;
	PS_HookRebuildPublishedSnapshot();
}

void PS_HookDestroyRegistration(PluginSystemHookRegistration* pRegistration)
{
	if ( pRegistration == NULL ) {
		return;
	}

	PS_HookUnpublishRegistration(pRegistration);
	PS_StorageUpdateResourceStatus(pRegistration->iResourceId, "removed");
	if ( !pRegistration->bListed ) {
		PS_HookFreeRegistration(pRegistration);
		return;
	}
	pRegistration->pGeneration = NULL;
	pRegistration->pProc = NULL;
}

void PS_HookPublishGeneration(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstHookRegistrations == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstHookRegistrations); i++ ) {
		PluginSystemHookRegistration* pRegistration = xrtListGetPtr(pGeneration->lstHookRegistrations, i);
		if ( pRegistration ) {
			PS_HookPublishRegistration(pRegistration);
		}
	}
}

void PS_HookUnpublishGeneration(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstHookRegistrations == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstHookRegistrations); i++ ) {
		PluginSystemHookRegistration* pRegistration = xrtListGetPtr(pGeneration->lstHookRegistrations, i);
		if ( pRegistration ) {
			PS_HookUnpublishRegistration(pRegistration);
		}
	}
}

void PS_HookDestroyGenerationRegistrations(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstHookRegistrations == NULL) ) {
		return;
	}

	for ( int i = xrtListCount(pGeneration->lstHookRegistrations) - 1; i >= 0; i-- ) {
		PluginSystemHookRegistration* pRegistration = xrtListGetPtr(pGeneration->lstHookRegistrations, i);
		if ( pRegistration ) {
			PS_HookDestroyRegistration(pRegistration);
		}
		xrtListSetPtr(pGeneration->lstHookRegistrations, i, NULL, NULL);
	}
}

int PS_HostListenEvent(void* plugin_handle, const XAdminEventDecl* decl, XAdminEventToken* token)
{
	PluginSystemGeneration* pGeneration = (PluginSystemGeneration*)plugin_handle;
	PluginSystemEventRegistration* pRegistration;
	char sProcRef[64];

	if ( (pGeneration == NULL) || (decl == NULL) || (decl->event_name == NULL) || (decl->event_name[0] == '\0') || (decl->proc == NULL) ) {
		return -1;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstEventRegistrations); i++ ) {
		PluginSystemEventRegistration* pExisting = xrtListGetPtr(pGeneration->lstEventRegistrations, i);
		if ( pExisting && PS_SignalTextEquals(pExisting->sEventName, decl->event_name) && (pExisting->pProc == decl->proc) ) {
			return -1;
		}
	}

	pRegistration = PS_EventCreateRegistration(pGeneration, decl);
	if ( pRegistration == NULL ) {
		return -1;
	}

	PS_SignalFormatPtr((const void*)decl->proc, sProcRef);
	pRegistration->iResourceId = PS_StorageTrackResource(pGeneration, "generation", "event_listener", decl->event_name, sProcRef, "auto_unload");
	PS_SignalAppendPtr(pGeneration->lstEventRegistrations, pRegistration);
	if ( pGeneration->iState == PS_GENERATION_STATE_ACTIVE ) {
		PS_EventPublishRegistration(pRegistration);
	}

	if ( token ) {
		*token = (XAdminEventToken)(uintptr_t)pRegistration;
	}
	return 0;
}

int PS_HostUnlistenEvent(XAdminEventToken token)
{
	PluginSystemEventRegistration* pRegistration = (PluginSystemEventRegistration*)(uintptr_t)token;

	if ( pRegistration == NULL ) {
		return -1;
	}
	if ( pRegistration->pGeneration ) {
		PS_SignalDetachPtr(pRegistration->pGeneration->lstEventRegistrations, pRegistration);
	}
	PS_EventDestroyRegistration(pRegistration);
	return 0;
}

int PS_HostEmitEvent(void* plugin_handle, const char* event_name, void* payload, size_t payload_size)
{
	PluginSystemGeneration* pGeneration = (PluginSystemGeneration*)plugin_handle;
	PluginSystemEventRegistration** ppSnapshot;
	int iInvokeCount = 0;

	(void)payload;
	(void)payload_size;

	if ( (pGeneration == NULL) || (event_name == NULL) || (event_name[0] == '\0') || (G_PluginSystem == NULL) ) {
		return -1;
	}

	ppSnapshot = G_PluginSystem->ppPublishedEvents;
	if ( ppSnapshot == NULL ) {
		return 0;
	}

	for ( int i = 0; ppSnapshot[i] != NULL; i++ ) {
		PluginSystemEventRegistration* pRegistration = ppSnapshot[i];
		if ( (pRegistration == NULL) || !pRegistration->bPublished || (pRegistration->pGeneration == NULL) || (pRegistration->pProc == NULL) ) {
			continue;
		}
		if ( !PS_SignalTextEquals(pRegistration->sEventName, event_name) || (pRegistration->pGeneration->iState != PS_GENERATION_STATE_ACTIVE) ) {
			continue;
		}

		pRegistration->pGeneration->iRefCount++;
		pRegistration->pProc(event_name, payload, payload_size);
		pRegistration->pGeneration->iRefCount--;
		if ( (pRegistration->pGeneration->iState == PS_GENERATION_STATE_DRAINING) && (pRegistration->pGeneration->iRefCount <= 0) ) {
			PS_RuntimeOnGenerationRefReleased(pRegistration->pGeneration);
		}
		iInvokeCount++;
	}

	return iInvokeCount;
}

int PS_HostRegisterHook(void* plugin_handle, const XAdminHookDecl* decl, XAdminHookToken* token)
{
	PluginSystemGeneration* pGeneration = (PluginSystemGeneration*)plugin_handle;
	PluginSystemHookRegistration* pRegistration;
	char sProcRef[64];

	if ( (pGeneration == NULL) || (decl == NULL) || (decl->hook_name == NULL) || (decl->hook_name[0] == '\0') || (decl->proc == NULL) ) {
		return -1;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstHookRegistrations); i++ ) {
		PluginSystemHookRegistration* pExisting = xrtListGetPtr(pGeneration->lstHookRegistrations, i);
		if ( pExisting && PS_SignalTextEquals(pExisting->sHookName, decl->hook_name) && (pExisting->pProc == decl->proc) ) {
			return -1;
		}
	}

	pRegistration = PS_HookCreateRegistration(pGeneration, decl);
	if ( pRegistration == NULL ) {
		return -1;
	}

	PS_SignalFormatPtr((const void*)decl->proc, sProcRef);
	pRegistration->iResourceId = PS_StorageTrackResource(pGeneration, "generation", "hook", decl->hook_name, sProcRef, "auto_unload");
	PS_SignalAppendPtr(pGeneration->lstHookRegistrations, pRegistration);
	if ( pGeneration->iState == PS_GENERATION_STATE_ACTIVE ) {
		PS_HookPublishRegistration(pRegistration);
	}

	if ( token ) {
		*token = (XAdminHookToken)(uintptr_t)pRegistration;
	}
	return 0;
}

int PS_HostUnregisterHook(XAdminHookToken token)
{
	PluginSystemHookRegistration* pRegistration = (PluginSystemHookRegistration*)(uintptr_t)token;

	if ( pRegistration == NULL ) {
		return -1;
	}
	if ( pRegistration->pGeneration ) {
		PS_SignalDetachPtr(pRegistration->pGeneration->lstHookRegistrations, pRegistration);
	}
	PS_HookDestroyRegistration(pRegistration);
	return 0;
}

int PS_HostInvokeHook(void* plugin_handle, const char* hook_name, void* payload, size_t payload_size)
{
	PluginSystemGeneration* pGeneration = (PluginSystemGeneration*)plugin_handle;
	PluginSystemHookRegistration** ppSnapshot;

	(void)payload;
	(void)payload_size;

	if ( (pGeneration == NULL) || (hook_name == NULL) || (hook_name[0] == '\0') || (G_PluginSystem == NULL) ) {
		return XADMIN_HOOK_ERROR;
	}

	ppSnapshot = G_PluginSystem->ppPublishedHooks;
	if ( ppSnapshot == NULL ) {
		return XADMIN_HOOK_CONTINUE;
	}

	for ( int i = 0; ppSnapshot[i] != NULL; i++ ) {
		PluginSystemHookRegistration* pRegistration = ppSnapshot[i];
		int iResult;

		if ( (pRegistration == NULL) || !pRegistration->bPublished || (pRegistration->pGeneration == NULL) || (pRegistration->pProc == NULL) ) {
			continue;
		}
		if ( !PS_SignalTextEquals(pRegistration->sHookName, hook_name) || (pRegistration->pGeneration->iState != PS_GENERATION_STATE_ACTIVE) ) {
			continue;
		}

		pRegistration->pGeneration->iRefCount++;
		iResult = pRegistration->pProc(hook_name, payload, payload_size);
		pRegistration->pGeneration->iRefCount--;
		if ( (pRegistration->pGeneration->iState == PS_GENERATION_STATE_DRAINING) && (pRegistration->pGeneration->iRefCount <= 0) ) {
			PS_RuntimeOnGenerationRefReleased(pRegistration->pGeneration);
		}
		if ( iResult != XADMIN_HOOK_CONTINUE ) {
			return iResult;
		}
	}

	return XADMIN_HOOK_CONTINUE;
}

#endif
