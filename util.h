#pragma once

#include <ntifs.h>
#include <Ntdddisk.h>
#include <windef.h>
#include <sys/stat.h>
#include <tchar.h>
#include <stdio.h>
#include <winapifamily.h>
#include <iostream.h>
#include <sys/stat.h>
#include <Ntstrsafe.h>
#include <ntdef.h>
#include <ntddk.h>
#include <winioctl.h>

#ifdef _DEBUG
#define log( s, ... ) DbgPrintEx( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, ("[DEBUG-LOG] " s))
#define log_s( s, ... ) DbgPrintEx( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, ("[+] " s))
#define log_e( s, ... ) DbgPrintEx( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, ("[-] " s))
#define log_w( s, ... ) DbgPrintEx( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, ("[!] " s))
#else
#define log( s, ... ) DbgPrintEx( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, ("[DEBUG-LOG] " s))
#define log_s( s, ... ) DbgPrintEx( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, ("[+] " s))
#define log_e( s, ... ) DbgPrintEx( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, ("[-]" s))
#define log_w( s, ... ) DbgPrintEx( DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, ("[!] " s))
#endif

typedef struct _REQUEST_STRUCT
{
	PIO_COMPLETION_ROUTINE OldRoutine;
	PVOID OldContext;
	ULONG OutputBufferLength;
	PSTORAGE_DEVICE_DESCRIPTOR StorageDescriptor;
} REQUEST_STRUCT, * PREQUEST_STRUCT;


typedef struct _REQUEST_STRUCT_USB
{
	PIO_COMPLETION_ROUTINE OldRoutine;
	PVOID OldContext;
	ULONG OutputBufferLength;
	PDISK_GEOMETRY StorageDescriptor;
} REQUEST_STRUCT_USB, * PREQUEST_STRUCT_USB;

extern "C" { 
	extern POBJECT_TYPE* IoDriverObjectType;
	extern POBJECT_TYPE* IoDeviceObjectType;
	NTSTATUS ObReferenceObjectByName(PUNICODE_STRING ObjectName,
		ULONG Attributes,
		PACCESS_STATE AccessState,
		ACCESS_MASK DesiredAccess,
		POBJECT_TYPE ObjectType,
		KPROCESSOR_MODE AccessMode,
		PVOID ParseContext,
		PVOID* Object);

NTKERNELAPI PCHAR PsGetProcessImageFileName(PEPROCESS process);
NTSTATUS DriverEntry(_In_  struct _DRIVER_OBJECT* DriverObject, _In_  PUNICODE_STRING RegistryPath);
}