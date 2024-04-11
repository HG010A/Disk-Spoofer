#include "util.h"


PDRIVER_DISPATCH DiskDeviceControl = NULL;
PDRIVER_DISPATCH UsbDeviceControl = NULL;
char NumTable[] = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char SpoofedHWID[] = "XYXYXYYYYYXYXXYXYYYXXYYXXXXYYXYYYXYYX\0";
BOOL HWIDGenerated = 0;

char* newDiskId = "Hello\0";

typedef struct _WIN32_FIND_DATA {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
    DWORD    dwReserved0;
    DWORD    dwReserved1;
    TCHAR    cFileName[MAX_PATH];
    TCHAR    cAlternateFileName[14];
} WIN32_FIND_DATA, * PWIN32_FIND_DATA, * LPWIN32_FIND_DATA;

NTSTATUS SpoofSerialNumber(char* serialNumber)
{
    //RtlSecureZeroMemory
    if (!HWIDGenerated)
    {
        HWIDGenerated = 1;
        size_t newDiskIdLen = 0;
        RtlStringCchLengthA(newDiskId, NTSTRSAFE_MAX_CCH, &newDiskIdLen);
        RtlCopyMemory((void*)serialNumber, (void*)newDiskId, ++newDiskIdLen);
    }
    return STATUS_SUCCESS;
}

NTSTATUS StorageQueryCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    PIO_COMPLETION_ROUTINE OldCompletionRoutine = NULL;
    PVOID OldContext = NULL;
    ULONG OutputBufferLength = 0;
    PSTORAGE_DEVICE_DESCRIPTOR descriptor = NULL;

    if (Context != NULL)
    {
        REQUEST_STRUCT* pRequest = (REQUEST_STRUCT*)Context;
        OldCompletionRoutine = pRequest->OldRoutine;
        OldContext = pRequest->OldContext;
        OutputBufferLength = pRequest->OutputBufferLength;
        descriptor = pRequest->StorageDescriptor;

        ExFreePool(Context);
    }

    if (FIELD_OFFSET(STORAGE_DEVICE_DESCRIPTOR, SerialNumberOffset) < OutputBufferLength && descriptor->SerialNumberOffset > 0 && descriptor->SerialNumberOffset < OutputBufferLength)
    {
        char* SerialNumber = ((char*)descriptor) + descriptor->SerialNumberOffset;
        size_t SerialNumberLen = 0;
        RtlStringCchLengthA(SerialNumber, NTSTRSAFE_MAX_CCH, &SerialNumberLen);
        RtlSecureZeroMemory(SerialNumber, SerialNumberLen);//用0填充内存缓冲区
        SpoofSerialNumber(SerialNumber);
    }

    if ((Irp->StackCount > (ULONG)1) && (OldCompletionRoutine != NULL))
        return OldCompletionRoutine(DeviceObject, Irp, OldContext);

    return STATUS_SUCCESS;
}

NTSTATUS SmartCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_COMPLETION_ROUTINE OldCompletionRoutine = NULL;

    PVOID OldContext = NULL;

    if (Context != NULL)
    {
        REQUEST_STRUCT* pRequest = (REQUEST_STRUCT*)Context;

        OldCompletionRoutine = pRequest->OldRoutine;
        
        OldContext = pRequest->OldContext;
        
        ExFreePool(Context);

    }

    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

    return Irp->IoStatus.Status;
}

NTSTATUS DiskDriverDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION Io = IoGetCurrentIrpStackLocation(Irp);

    switch (Io->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_STORAGE_QUERY_PROPERTY:
    {
        PSTORAGE_PROPERTY_QUERY query = (PSTORAGE_PROPERTY_QUERY)Irp->AssociatedIrp.SystemBuffer;//PSTORAGE_PROPERTY_QUERY是存储硬件类的结构体（设备描述、容量、序列号）

        if (query->PropertyId == StorageDeviceProperty)
        {
            Io->Control = 0;
            Io->Control |= SL_INVOKE_ON_SUCCESS;

            PVOID OldContext = Io->Context;
            Io->Context = (PVOID)ExAllocatePool(NonPagedPool, sizeof(REQUEST_STRUCT));
            REQUEST_STRUCT* pRequest = (REQUEST_STRUCT*)Io->Context;
            pRequest->OldRoutine = Io->CompletionRoutine;
            pRequest->OldContext = OldContext;
            pRequest->OutputBufferLength = Io->Parameters.DeviceIoControl.OutputBufferLength;
            pRequest->StorageDescriptor = (PSTORAGE_DEVICE_DESCRIPTOR)Irp->AssociatedIrp.SystemBuffer;

            Io->CompletionRoutine = (PIO_COMPLETION_ROUTINE)StorageQueryCompletionRoutine;
        }

        break;

    }

    case SMART_RCV_DRIVE_DATA:
    {
        Io->Control = 0;

        Io->Control |= SL_INVOKE_ON_SUCCESS;

        PVOID OldContext = Io->Context;

        Io->Context = (PVOID)ExAllocatePool(NonPagedPool, sizeof(REQUEST_STRUCT));

        REQUEST_STRUCT* pRequest = (REQUEST_STRUCT*)Io->Context;

        pRequest->OldRoutine = Io->CompletionRoutine;

        pRequest->OldContext = OldContext;

        Io->CompletionRoutine = (PIO_COMPLETION_ROUTINE)SmartCompletionRoutine;

        break;
    }
    }

    return DiskDeviceControl(DeviceObject, Irp);
}

NTSTATUS UnsupportedDispatch(
    _In_ struct _DEVICE_OBJECT* DeviceObject,
    _Inout_ struct _IRP* Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

NTSTATUS CreateDispatch(
    _In_ struct _DEVICE_OBJECT* DeviceObject,
    _Inout_ struct _IRP* Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}

NTSTATUS CloseDispatch(_In_ struct _DEVICE_OBJECT* DeviceObject, _Inout_ struct _IRP* Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Irp->IoStatus.Status;
}


auto SpoodDisk(_In_  struct _DRIVER_OBJECT* DriverObject, _In_  PUNICODE_STRING RegistryPath)->NTSTATUS {

    NTSTATUS Result = STATUS_SUCCESS;

    UNICODE_STRING ObjeName = RTL_CONSTANT_STRING(L"\\Driver\\disk");

    PDRIVER_OBJECT DriverObj = NULL;

    Result = ObReferenceObjectByName(&ObjeName, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, 0, (PVOID*)&DriverObj);

    if (!NT_SUCCESS(Result)) {

        Result = STATUS_UNSUCCESSFUL;

        log_s("open disk faid!\r\n");

        return Result;
    }

    DiskDeviceControl = DriverObj->MajorFunction[IRP_MJ_DEVICE_CONTROL];

    DriverObj->DriverInit = &DriverEntry;

    DriverObj->DriverStart = (PVOID)DriverObject;

    DriverObj->DriverSize = (ULONG)RegistryPath;

    DriverObj->DriverStartIo = NULL;

    DriverObj->DriverUnload = NULL;

    DriverObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = &DiskDriverDispatch;

    return Result;

}

auto USBCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)->NTSTATUS {

    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_COMPLETION_ROUTINE OldCompletionRoutine = NULL;

	PVOID OldContext = NULL;

	ULONG OutputBufferLength = 0;

	PREQUEST_STRUCT_USB PDisk = (PREQUEST_STRUCT_USB)Context;

	OldCompletionRoutine = PDisk->OldRoutine;

	OldContext = PDisk->OldContext;

	OutputBufferLength = PDisk->OutputBufferLength;



	LPSTR USBProcessName = PsGetProcessImageFileName(IoGetCurrentProcess());
	//符合以下某个名称的进程才会被欺骗
    if (0 != _stricmp(USBProcessName, "xxx.exe"))
    {

        if (Context != NULL)
        {
            PDISK_GEOMETRY descriptor = PDisk->StorageDescriptor;

            if (descriptor->Cylinders.QuadPart > 0)
            {
                LONGLONG* Cylinder = &descriptor->Cylinders.QuadPart;

                MEDIA_TYPE* disktype = &descriptor->MediaType;

                ULONG* TracksPer = &descriptor->TracksPerCylinder;

                ULONG* SectorsPerTrack = &descriptor->SectorsPerTrack;

                ULONG* BytesPerSector = &descriptor->BytesPerSector;

                RtlZeroMemory(TracksPer, sizeof(ULONG));

                RtlZeroMemory(SectorsPerTrack, sizeof(ULONG));

                RtlZeroMemory(Cylinder, sizeof(LONGLONG));

                LONGLONG newDiskId = 10;
            }

        }
    }

	if ((Irp->StackCount > (ULONG)1) && (OldCompletionRoutine != NULL))
		return OldCompletionRoutine(DeviceObject, Irp, OldContext);
    return STATUS_SUCCESS;
}

auto USBDriverDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)->NTSTATUS {

    UNREFERENCED_PARAMETER(DeviceObject);

	PIO_STACK_LOCATION Io = IoGetCurrentIrpStackLocation(Irp);

    switch (Io->Parameters.DeviceIoControl.IoControlCode)
    {
       
    case IOCTL_DISK_GET_DRIVE_GEOMETRY:
	{

		PVOID OldContext = Io->Context;

		Io->Control = 0;

		Io->Control |= SL_INVOKE_ON_SUCCESS;

		Io->Context = (PVOID)ExAllocatePool(NonPagedPool, sizeof(REQUEST_STRUCT_USB));

		REQUEST_STRUCT_USB* pRequest = (REQUEST_STRUCT_USB*)Io->Context;

		pRequest->OldRoutine = Io->CompletionRoutine;

		pRequest->OldContext = OldContext;

		pRequest->OutputBufferLength = Io->Parameters.DeviceIoControl.OutputBufferLength;

		pRequest->StorageDescriptor = (PDISK_GEOMETRY)Irp->AssociatedIrp.SystemBuffer;

		Io->CompletionRoutine = (PIO_COMPLETION_ROUTINE)USBCompletionRoutine;

        break;

	}
 }
    return UsbDeviceControl(DeviceObject, Irp);
}

auto UsbScan(_In_  struct _DRIVER_OBJECT* DriverObject, _In_  PUNICODE_STRING RegistryPath)->NTSTATUS {

    NTSTATUS Result = STATUS_SUCCESS;

    UNICODE_STRING ObjeName = RTL_CONSTANT_STRING(L"\\Driver\\disk");

    PDRIVER_OBJECT DriverObj = NULL;

    Result = ObReferenceObjectByName(&ObjeName, OBJ_CASE_INSENSITIVE, 0, 0, *IoDriverObjectType, KernelMode, 0, (PVOID*)&DriverObj);

    if (!NT_SUCCESS(Result)) {

        Result = STATUS_UNSUCCESSFUL;

        log_s("open disk faid!\r\n");

        return Result;
    }

    UsbDeviceControl = DriverObj->MajorFunction[IRP_MJ_DEVICE_CONTROL];

    DriverObj->DriverInit = &DriverEntry;

    DriverObj->DriverStart = (PVOID)DriverObject;

    DriverObj->DriverSize = (ULONG)RegistryPath;

    DriverObj->DriverStartIo = NULL;

    DriverObj->DriverUnload = NULL;

    DriverObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = &USBDriverDispatch;

    return Result;
}


auto  UnloadDriver(IN PDRIVER_OBJECT driverObject)->void
{
    log_s("驱动卸载!\r\n");
}

NTSTATUS DriverEntry(_In_  struct _DRIVER_OBJECT* DriverObject, _In_  PUNICODE_STRING RegistryPath)
{
    NTSTATUS status = STATUS_SUCCESS;

    UsbScan(DriverObject,RegistryPath);

    return status;
}