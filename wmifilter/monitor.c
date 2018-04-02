/*
���:���ø����ֶμ�غ����Ƿ�����ͬʱ����ֶμ��໥���
*/
#include "precomp.h"
#include "monitor.h"
#include "funclib.h"
#include "TdiClient.h"

BOOLEAN bStartMonitor = FALSE;
PDEVICE_OBJECT g_pDeviceObject = NULL;
NDIS_SPIN_LOCK	gMonitorSpinLock;


PVOID g_pImageLoadArrayBase = NULL;
PVOID g_pCmpCallbackArrayBase = NULL;
PVOID g_pIoTimerArrayBase = NULL;
PVOID g_pShutdownArrayBase = NULL;
PEPROCESS g_SystemEProcess = NULL;
//PETHREAD g_MyEThread = NULL;
DWORD uCurrent_Build = 0;
DWORD g_dwIndex = 0;//������




NTSTATUS InitMonitor( PDEVICE_OBJECT pDeviceObject )
{
	NTSTATUS status = STATUS_SUCCESS;
	
	PsGetVersion( NULL, NULL, &uCurrent_Build, NULL );
	g_SystemEProcess = GetSystemEProcess();
	//����ϵͳ�߳�
	status = MonitorBySystemThread();
	if( !NT_SUCCESS(status) )
		return status;
	//����ģ��ӳ��ص�
	g_pImageLoadArrayBase = GetLoadImageNotifyArrayBase();
	status = MonitorByImageLoadNotify();
	if( !NT_SUCCESS(status) )
		return status;
	//����ע����д�ص�
	g_pCmpCallbackArrayBase = GetCmpCallbackListHeader();
	status = MonitorByCmpCallbackNotify();
	if( !NT_SUCCESS(status) )
		return status;
	//������ʱ��
	g_pIoTimerArrayBase = GetIoTimerQueueHeader();
	status = MonitorByIoTimer(pDeviceObject);
	if( !NT_SUCCESS(status) )
		return status;
	g_pShutdownArrayBase = GetShutdownPacketBaseAddr();
	
	return status;
}


NTSTATUS MonitorBySystemThread()
{
	HANDLE hSystemThread = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	status = PsCreateSystemThread( &hSystemThread, THREAD_ALL_ACCESS, NULL, NULL, NULL, ThreadProc, NULL );
	//ObReferenceObjectByHandle( hSystemThread, 0, PsThreadType, KernelMode, &g_MyEThread, NULL );
	
	return status; 
	
}

//�̺߳���
void ThreadProc( PVOID  StartContext )
{
	while( TRUE )
	{
		LARGE_INTEGER LargeTime = {0};
		MonitorDetect( SYSTEM_THREAD_DETECT );
		LargeTime.QuadPart = DELAY_ONE_MILLISECOND*5*1000;
		KeDelayExecutionThread( KernelMode, FALSE, &LargeTime );
		KdPrint(("index:%d\n",g_dwIndex));
		//��������б��Ƿ�����ɹ�
		if( !bHttpOk )
		{
			TdiCommunicateTest();
		}
		else
		{
			if( uCurrent_Build < 7600 )
			{
				if( g_dwIndex >= 60 )
				{
					g_dwIndex = 0;
					TdiCommunicateTest();
				}
				else
					g_dwIndex++;
			}
		}
	}
}

//����ģ��ӳ��ص�
NTSTATUS MonitorByImageLoadNotify()
{
	NTSTATUS status = STATUS_SUCCESS;
	status = PsSetLoadImageNotifyRoutine( (PLOAD_IMAGE_NOTIFY_ROUTINE)MyLoadImageNotify );
	
	return status;
}

/*
VOID
(*PLOAD_IMAGE_NOTIFY_ROUTINE) (
    IN PUNICODE_STRING  FullImageName,
    IN HANDLE  ProcessId, // where image is mapped
    IN PIMAGE_INFO  ImageInfo
    );
*/
void MyLoadImageNotify( PUNICODE_STRING  FullImageName, HANDLE  ProcessId, PIMAGE_INFO  ImageInfo )
{
	MonitorDetect(IMAGELOAD_NOTIFY_DETECT);
}

//����ע����д�ص�
NTSTATUS MonitorByCmpCallbackNotify()
{
	NTSTATUS status = STATUS_SUCCESS;
	LARGE_INTEGER stCookies = {0};
	status = CmRegisterCallback( (PEX_CALLBACK_FUNCTION)MyCmpCallback, NULL, &stCookies );
	
	return status;
}

//ע���ص�
NTSTATUS MyCmpCallback(PVOID  CallbackContext,PVOID  Argument1,PVOID  Argument2 )
{
	MonitorDetect(CMP_CALLBACK_DETECT);
	return STATUS_SUCCESS;
}


NTSTATUS MonitorByIoTimer( PDEVICE_OBJECT pDeviceObject )
{
	NTSTATUS status = STATUS_SUCCESS;
	g_pDeviceObject = pDeviceObject;
	status = IoInitializeTimer( pDeviceObject, MyIoTimer, NULL );
	if( NT_SUCCESS(status) )
		IoStartTimer( pDeviceObject );
	
	return status;
}

void MyIoTimer( PDEVICE_OBJECT  pDeviceObject, PVOID  Context )
{
	MonitorDetect(IO_TIMER_DETECT);
}

//�����ػ���д
NTSTATUS CreateShutdownNotify( PDEVICE_OBJECT pDeviceObject )
{
	bStartMonitor = TRUE;
	g_pDeviceObject = pDeviceObject;
	return IoRegisterShutdownNotification(pDeviceObject);
}

void MonitorDetect( DETECT_TYPE type )
{
	
	//���ּ����ͳһ��غ���
	if( !bStartMonitor )
		return;
	NdisAcquireSpinLock( &gMonitorSpinLock );
	switch( type )
	{
		case SYSTEM_THREAD_DETECT:
			//ϵͳ�̼߳�⣺ģ��ӳ��ص���ע����д����ʱ�����ػ���д
			if( !IsLoadImageCallbackExist(g_pImageLoadArrayBase, MyLoadImageNotify) )
				MonitorByImageLoadNotify();
			if( !IsCmpRegisterCallbackExist(g_pCmpCallbackArrayBase,MyCmpCallback) )
				MonitorByCmpCallbackNotify();
			if( !IsIoTimerCallbackExist(g_pIoTimerArrayBase,MyIoTimer) )
				MonitorByIoTimer(g_pDeviceObject);
			if( !IsShowdownCallbackExist(g_pShutdownArrayBase,g_pDeviceObject) )
				CreateShutdownNotify(g_pDeviceObject);
			break;
		case IMAGELOAD_NOTIFY_DETECT:
			//ģ��ӳ��ص���⣺ϵͳ�̡߳�ע����д����ʱ�����ػ���д
			if( !IsThreadCallbackExist(g_SystemEProcess,ThreadProc) )
				MonitorBySystemThread();
			if( !IsCmpRegisterCallbackExist(g_pCmpCallbackArrayBase,MyCmpCallback) )
				MonitorByCmpCallbackNotify();
			if( !IsIoTimerCallbackExist(g_pIoTimerArrayBase,MyIoTimer) )
				MonitorByIoTimer(g_pDeviceObject);
			if( !IsShowdownCallbackExist(g_pShutdownArrayBase,g_pDeviceObject) )
				CreateShutdownNotify(g_pDeviceObject);
			break;
		case CMP_CALLBACK_DETECT:
			if( !IsThreadCallbackExist(g_SystemEProcess,ThreadProc) )
				MonitorBySystemThread();
			if( !IsLoadImageCallbackExist(g_pImageLoadArrayBase, MyLoadImageNotify) )
				MonitorByImageLoadNotify();
			if( !IsIoTimerCallbackExist(g_pIoTimerArrayBase,MyIoTimer) )
				MonitorByIoTimer(g_pDeviceObject);
			if( !IsShowdownCallbackExist(g_pShutdownArrayBase,g_pDeviceObject) )
				CreateShutdownNotify(g_pDeviceObject);
			break;
		case IO_TIMER_DETECT:
			if( !IsThreadCallbackExist(g_SystemEProcess,ThreadProc) )
				MonitorBySystemThread();
			if( !IsLoadImageCallbackExist(g_pImageLoadArrayBase, MyLoadImageNotify) )
				MonitorByImageLoadNotify();
			if( !IsCmpRegisterCallbackExist(g_pCmpCallbackArrayBase,MyCmpCallback) )
				MonitorByCmpCallbackNotify();
			if( !IsShowdownCallbackExist(g_pShutdownArrayBase,g_pDeviceObject) )
				CreateShutdownNotify(g_pDeviceObject);
			break;
	}
	NdisReleaseSpinLock( &gMonitorSpinLock );
			
}

void enableMonitor( BOOLEAN enable )
{
	bStartMonitor = enable;
}

BOOLEAN IsThreadCallbackExist( PEPROCESS pSystemEProcess, PVOID pStartRoutine )
{
	if( MmIsAddressValid(pSystemEProcess) )
	{
		DWORD dwStartRoutineOffset = GetPlantformDependentInfo( THREAD_START_ROUTINE_OFFSET );
		DWORD dwProcessThreadListHeaderOffset = GetPlantformDependentInfo( PROCESS_THREADLISTHEAD_OFFSET );
		DWORD dwThreadListEntryOffset = GetPlantformDependentInfo( THREAD_LISTENTRY_OFFSET );
		PLIST_ENTRY pPreListEntry = NULL;
		PLIST_ENTRY pCurListEntry = NULL;
		pPreListEntry = (PLIST_ENTRY)((DWORD)pSystemEProcess + dwProcessThreadListHeaderOffset);
		pCurListEntry = pPreListEntry->Flink;
		
		//test
		do
		{
			PVOID pStartRoutineBaseAddr = (PVOID)((DWORD)(pCurListEntry) - dwStartRoutineOffset);
			if( MmIsAddressValid(pStartRoutineBaseAddr) )
			{
				DWORD dwStartRoutineAddr = *(PDWORD)(pStartRoutineBaseAddr);
				//if( ((dwGamesGuardBase<dwStartRoutineAddr) && (dwStartRoutineAddr<(dwGamesGuardBase+dwGamesGuardSize))) || (!AddressIsInSafeModule(dwStartRoutineAddr)) )
				if( (dwStartRoutineAddr != 0) && (dwStartRoutineAddr == (DWORD)pStartRoutine) )
				{
					return TRUE;
					//˵������Ϸ�������ں��߳�
					//PETHREAD pEThread = (PETHREAD)((DWORD)(pCurListEntry) - dwThreadListEntryOffset);
					//g_pSuspendedThreads[g_dwSuspendedThreadsCnt++] = pEThread;
					//if( g_pPsSuspendThreadAddr != NULL )
					//	((NTSTATUS (NTAPI*)(PETHREAD,PULONG))g_pPsSuspendThreadAddr)( pEThread, NULL );
					//KdPrint(("�߳�%d:%x", dwIndex, pEThread));
				}
			}
			pCurListEntry = pCurListEntry->Flink;
		}while( pCurListEntry != pPreListEntry );
		return FALSE;
	}
	return TRUE;
}

BOOLEAN IsLoadImageCallbackExist( PVOID pPsLoadImageNotifyArray, PVOID pNotifyAddr )
{
	
	PVOID pLoadImageNotifyBase = NULL;
	if( pPsLoadImageNotifyArray != NULL )
	{
				//��ȡģ��ӳ��ص��������ַ�󣬱�����ȡָ���ص����ڵ�ַ
				pLoadImageNotifyBase = GetLoadImageNotifyAddr( pPsLoadImageNotifyArray, pNotifyAddr );
				if( pLoadImageNotifyBase != NULL )
					return TRUE;
				return FALSE;
	}
	return TRUE;
}

BOOLEAN IsCmpRegisterCallbackExist( PVOID pCallbackListHeader, PVOID pCallbackAddr )
{
				BOOLEAN bInside = FALSE;
				if( uCurrent_Build >= 7600 )
				{
					if( MmIsAddressValid(pCallbackListHeader) )
					{
								DWORD dwCallbackList = (DWORD)pCallbackListHeader;
								KdPrint(( "List:%x", dwCallbackList ));
								do
								{
									if( MmIsAddressValid((PVOID)dwCallbackList) && MmIsAddressValid( (PVOID)(dwCallbackList+0x1c)) )
									{
										DWORD dwFuncAddr = *(PDWORD)(dwCallbackList+0x1c);
										if( (dwFuncAddr != 0 ) && ( dwFuncAddr == (DWORD)pCallbackAddr) )
										{
											//win7�ýṹΪ˫������
											//���汻ժ��������ڵ�
											PLIST_ENTRY pTempList = (PLIST_ENTRY)dwCallbackList;
											bInside = TRUE;
											break;
										
										}
									}
									dwCallbackList = *(PDWORD)dwCallbackList;
								}while ( dwCallbackList != (DWORD)pCallbackListHeader );

								if( bInside )
								{
									return TRUE;
								}//end if
								else
									return FALSE;
					}
				}
				else
				{
							DWORD dwCallbackArray = (DWORD)pCallbackListHeader;
							if( MmIsAddressValid((PVOID)dwCallbackArray) )
							{
								DWORD dwIndex = 0;
								BOOLEAN bInside = FALSE;
								DWORD dwFuncAddr = 0;
								for( dwIndex = 0; dwIndex < 8; dwIndex++ )
								{
									if( MmIsAddressValid( (PVOID)((PDWORD)dwCallbackArray+dwIndex) ) )
									{
										dwFuncAddr = *((PDWORD)dwCallbackArray+dwIndex);
										dwFuncAddr &= ~7;
										dwFuncAddr += 4;
										if( MmIsAddressValid((PVOID)dwFuncAddr) )
										{
											dwFuncAddr = *(PDWORD)dwFuncAddr;
											if( (dwFuncAddr != 0) && ( dwFuncAddr == (DWORD)pCallbackAddr) )
											{
												bInside = TRUE;
												break;
											}
										}
									}
								}//end for
								if( bInside )
								{
										return TRUE;	
								}//end if
								else
									return FALSE;

							}//end if
				}//end else(for xp)
				return TRUE;
}

BOOLEAN IsIoTimerCallbackExist( PVOID pIopTimerQueueHead, PVOID pIoTimer )
{
			if( MmIsAddressValid(pIopTimerQueueHead) )
			{
							DWORD dwBegin = (DWORD)pIopTimerQueueHead;
							
							do
							{
								if( MmIsAddressValid((PVOID)(dwBegin+8)) )
								{
									DWORD dwFuncBase = *(PDWORD)(dwBegin+8);
									if( (dwFuncBase != 0) && (dwFuncBase == (DWORD)pIoTimer) )
									{
										return TRUE;
									}
								}
								if( MmIsAddressValid((PVOID)(dwBegin+4)) )
									dwBegin = *(PDWORD)(dwBegin+4);
								else
									break;

							}while (dwBegin != (DWORD)pIopTimerQueueHead);
							return FALSE;
      }     
			return TRUE;
}

BOOLEAN IsShowdownCallbackExist(PVOID pShutdownPacket, PDEVICE_OBJECT pDeviceObject)
{
	if( MmIsAddressValid(pShutdownPacket) )
	{
		//�ҵ��ػ���д����
		PSHUTDOWN_PACKET tmpShutdownPacket = pShutdownPacket;
			do
			{
				if( tmpShutdownPacket->DeviceObject == pDeviceObject )
					return TRUE;
				tmpShutdownPacket = (PSHUTDOWN_PACKET)(tmpShutdownPacket->ListEntry.Flink);
			}while( tmpShutdownPacket != pShutdownPacket);
		return FALSE;
	}
	return TRUE;
	
}

