#pragma once

#include "ntddk.h"

VOID DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
NTSTATUS DispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchPassThrough(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS AttachToDevice(PDRIVER_OBJECT DriverObject);