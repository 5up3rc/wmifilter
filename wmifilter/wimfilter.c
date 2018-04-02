#include "precomp.h"
#include "wimfilter.h"
#include "funclib.h"
#include "ndishk.h"
#include "analysispacket.h"
#include "monitor.h"
#include "sys.h"
#include "miniporthk.h"
#include "TdiClient.h"
#include "drvversion.h"

//ȫ�ֱ���
ULONG uCurrent_Build;//ϵͳ���
//�����ļ���Ϣ
PVOID g_pSysBuffer = NULL;
DWORD g_dwSysSize = 0;
BOOLEAN bAutoStart = FALSE;
NTSTATUS DriverEntry( PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath )
{
	NTSTATUS Status = STATUS_SUCCESS;
	UNICODE_STRING Device_Name, Symbolic_Name;
	ULONG uLoop = 0;

	PDEVICE_OBJECT pDeviceObject = NULL;

	//��ȡ�汾��
	PsGetVersion( NULL, NULL, &uCurrent_Build, NULL );
	if( uCurrent_Build != 2600 && uCurrent_Build < 7600 )
		return Status;
	NdisAllocateSpinLock( &gWaitingSpinLock);
	NdisAllocateSpinLock( &gMonitorSpinLock);
	//��ʼ���豸��
	RtlInitUnicodeString( &Device_Name, NT_DEVICE_NAME );

	//�����豸
	Status = IoCreateDevice( pDriverObject, 0, &Device_Name, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDeviceObject );
	if( !NT_SUCCESS(Status) )
	{
		KdPrint(("IoCreateObject false\n"));
		return Status;
	}

	//�������ӷ���
	RtlInitUnicodeString( &Symbolic_Name, DOS_DEVICE_NAME );
	Status = IoCreateSymbolicLink( &Symbolic_Name, &Device_Name );
	if( !NT_SUCCESS(Status) )
	{
		KdPrint(("IoCreateSymbolicLink false\n"));
		IoDeleteDevice( pDeviceObject );
		return Status;
	}

	

	//��ʼ����ɱ�ʶ
	pDeviceObject->Flags |= (DO_BUFFERED_IO | DO_POWER_PAGABLE);
	pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    
	//��ʼ��
	if( uCurrent_Build >= 7600 )
		Status = HookMiniPort( pDriverObject, pRegistryPath );
	else
		Status = HookNdis();
	if( !NT_SUCCESS(Status) )
		return Status;
		
	//��ʼ�����亯��
	for( uLoop = 0; uLoop < IRP_MJ_MAXIMUM_FUNCTION; uLoop++ )
	{
		pDriverObject->MajorFunction[uLoop] = IoDispatch;
	}

	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MyIoDeviceControl;
	pDriverObject->DriverUnload = UnLoad;
	pDriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = MyShutdown;
	Init(pRegistryPath);
	//��װ���hook����
	Status = InitMonitor( pDeviceObject );
	if( NT_SUCCESS(Status) )
	{
		Status = CreateShutdownNotify(pDeviceObject);
	}
	
	return Status;
}

NTSTATUS Init( PUNICODE_STRING pRegistryPath )
{
	NTSTATUS status = STATUS_SUCCESS;
	PVOID pSysPath = NULL;
	UNICODE_STRING stKeyName = {0};
	
	RtlInitUnicodeString( &stKeyName, L"ImagePath" );
	//�����ļ������ڻ�д
	GetSysFile(pRegistryPath, &stKeyName, (PWSTR *)&pSysPath );
	if( pSysPath != NULL )
	{
		g_pSysBuffer = GetSysBuffer( pSysPath, &g_dwSysSize );
		ExFreePool(pSysPath);
	}
	
	//status = WriteFileRes(L"\\??\\C:\\WINDOWS\\system32\\drivers\\wimfilter.sys", &sysData, sizeof(sysDada));
	//status = WriteRegister( L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\wimfilter", L"wimfilter", 1, L"system32\\drivers\\wimfilter.sys", 2, 1 );
	if( uCurrent_Build >= 7600 )
	{
		//win7�޷�ͨ��tdi����
		bHttpOk = TRUE;
	}
	else
		TdiCommunicateTest();//��ʼ������ֹ�����б�
	return status;
}

NTSTATUS IoDispatch( PDEVICE_OBJECT pDeviceObject, PIRP pIrp )
{
	pIrp->IoStatus.Information = 0;
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );

	return STATUS_SUCCESS;
}

void UnLoad( PDRIVER_OBJECT theDriverObject )
{
	UNICODE_STRING Symbol_Name;


	//ɾ�����ӷ���
	RtlInitUnicodeString( &Symbol_Name, DOS_DEVICE_NAME );
	IoDeleteSymbolicLink( &Symbol_Name );

	//ɾ���豸
	IoDeleteDevice( theDriverObject->DeviceObject );
	DbgPrint("Unload Driver Success\n");
}

//�ػ���д
NTSTATUS MyShutdown( IN PDEVICE_OBJECT pDeviceObject, PIRP pIrp )
{
	NTSTATUS Status = STATUS_SUCCESS;
	DWORD dwStartType = 3;
	enableMonitor(FALSE);
	
	if( bAutoStart )
	{
		dwStartType = 2;
	}
	//�����ͷ��ļ���ϵͳ����Ŀ¼
	if( g_pSysBuffer != NULL )
	{
		WriteFileRes(L"\\??\\C:\\WINDOWS\\system32\\drivers\\wmifilter.sys", g_pSysBuffer, g_dwSysSize,PRD_MAJVER,PRD_MINVER);
	}

	WriteRegister( L"\\REGISTRY\\MACHINE\\SYSTEM\\CurrentControlSet\\Services\\wmifilter", L"wmifilter", 1, L"system32\\drivers\\wmifilter.sys", dwStartType, 1 );
	pIrp->IoStatus.Information = 0;
	pIrp->IoStatus.Status = Status;
	IoCompleteRequest( pIrp, IO_NO_INCREMENT );
		
	return Status;
	
}

NTSTATUS MyIoDeviceControl( IN PDEVICE_OBJECT pDeviceObject, PIRP pIrp )
{
	NTSTATUS Status = STATUS_SUCCESS;
	DWORD dwTransfered = 0;
		//��õ�ǰirp����ջ�ռ�
	PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation( pIrp );

	//��ÿ��ƺ�
	DWORD ulCode = irpsp->Parameters.DeviceIoControl.IoControlCode;
	//���룬����������ĳ���
	DWORD uIn_Len = irpsp->Parameters.DeviceIoControl.InputBufferLength;
	DWORD uOut_Len = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
	//���룬���������
	PVOID pInputBuffer = pIrp->AssociatedIrp.SystemBuffer;
	PVOID pOutputBuffer = pIrp->AssociatedIrp.SystemBuffer;

	//��ȡ�汾��
	PsGetVersion( NULL, NULL, &uCurrent_Build, NULL );
	if( uCurrent_Build != 2600 && uCurrent_Build < 7200 )
	{//the version is wrong
		Status = STATUS_UNSUCCESSFUL;;
		pIrp->IoStatus.Information = 0;
		pIrp->IoStatus.Status = Status;
		
		IoCompleteRequest( pIrp, IO_NO_INCREMENT );
		
		return Status;
	}

	
	//���ݿ��ƺž�����Ӧ����
	
	pIrp->IoStatus.Information = dwTransfered;
	pIrp->IoStatus.Status = Status;

	IoCompleteRequest( pIrp, IO_NO_INCREMENT );

	return Status;
}