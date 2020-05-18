#include "Driver.h"
#include "Mouse.h"
#include "ntddk.h"
#include "ntddmou.h"
#include "wdm.h"

#define UNLOAD_WAIT_INTERVAL (-10 * 1000 * 1000)

NTSTATUS ObReferenceObjectByName(
	IN PUNICODE_STRING ObjectName,
	IN ULONG Attributes,
	IN PACCESS_STATE AccessState,
	IN ACCESS_MASK DesiredAccess,
	IN POBJECT_TYPE ObjectType,
	IN KPROCESSOR_MODE AccessMode,
	IN PVOID ParseContext,
	OUT PVOID* Object
);

typedef struct {
	PDEVICE_OBJECT LowerDevice;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

extern POBJECT_TYPE* IoDriverObjectType;

LONG PendingIrpCount = 0;

VOID DetachDeviceList(PDRIVER_OBJECT DriverObject)
{
	PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
	while (deviceObject)
	{
		IoDetachDevice(((PDEVICE_EXTENSION)deviceObject->DeviceExtension)->LowerDevice);
		deviceObject = deviceObject->NextDevice;
	}
}

VOID DeleteDeviceList(PDRIVER_OBJECT DriverObject)
{
	PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
	while (deviceObject)
	{
		IoDeleteDevice(deviceObject);
		deviceObject = deviceObject->NextDevice;
	}
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	DetachDeviceList(DriverObject);

	LARGE_INTEGER interval = { 0 };
	interval.QuadPart = UNLOAD_WAIT_INTERVAL;
	while (PendingIrpCount) 
	{
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
	}

	DeleteDeviceList(DriverObject);

	KdPrint(("Driver unloaded\r\n"));
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	NTSTATUS status;
	DriverObject->DriverUnload = DriverUnload;

	for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i)
	{
		DriverObject->MajorFunction[i] = DispatchPassThrough;
	}

	DriverObject->MajorFunction[IRP_MJ_READ] = DispatchRead;

	status = AttachToDevice(DriverObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("AttachToDevice failed\r\n"));
		DetachDeviceList(DriverObject);
		DeleteDeviceList(DriverObject);
	}

	KdPrint(("Driver loaded\r\n"));
	return STATUS_SUCCESS;
}

NTSTATUS ReadComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Context);
	PMOUSE_INPUT_DATA inputs = (PMOUSE_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
	size_t inputNumber = Irp->IoStatus.Information / sizeof(MOUSE_INPUT_DATA);
	static BOOLEAN inverse = FALSE;

	if (Irp->IoStatus.Status == STATUS_SUCCESS)
	{
		for (int i = 0; i < inputNumber; ++i) 
		{
			HandleMouseInput(&inputs[i]);
		}
	}

	if (Irp->PendingReturned) 
	{
		IoMarkIrpPending(Irp);
	}
	
	InterlockedDecrement(&PendingIrpCount);

	return Irp->IoStatus.Status;
}

NTSTATUS DispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(Irp, ReadComplete, NULL, TRUE, TRUE, TRUE);

	InterlockedIncrement(&PendingIrpCount);

	return IoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerDevice, Irp);
}

NTSTATUS DispatchPassThrough(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	IoCopyCurrentIrpStackLocationToNext(Irp);
	return IoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerDevice, Irp);
}

NTSTATUS AttachToDevice(PDRIVER_OBJECT DriverObject)
{
	NTSTATUS status;
	UNICODE_STRING mcName = RTL_CONSTANT_STRING(L"\\Driver\\Mouclass");
	PDRIVER_OBJECT targetDriverObject = NULL;
	PDEVICE_OBJECT currentDeviceObject = NULL;
	PDEVICE_OBJECT newDeviceObject = NULL;

	status = ObReferenceObjectByName(&mcName, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&targetDriverObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("ObReference failed\r\n"));
		return status;
	}

	ObDereferenceObject(targetDriverObject);

	currentDeviceObject = targetDriverObject->DeviceObject;
	while (currentDeviceObject) 
	{
		status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), NULL, FILE_DEVICE_MOUSE, 0, FALSE, &newDeviceObject);
		if (!NT_SUCCESS(status))
		{
			return status;
		}

		RtlZeroMemory(newDeviceObject->DeviceExtension, sizeof(DEVICE_EXTENSION));
		status = IoAttachDeviceToDeviceStackSafe(newDeviceObject, currentDeviceObject, &((PDEVICE_EXTENSION)newDeviceObject->DeviceExtension)->LowerDevice);
		if (!NT_SUCCESS(status))
		{
			IoDeleteDevice(newDeviceObject);
			return status;
		}

		newDeviceObject->Flags |= DO_BUFFERED_IO;
		newDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

		currentDeviceObject = currentDeviceObject->NextDevice;
	}

	return STATUS_SUCCESS;
}