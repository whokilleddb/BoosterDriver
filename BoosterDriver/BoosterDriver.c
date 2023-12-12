#include <ntifs.h>
#include "BoosterCommon.h"
#define DRIVER_PREFIX "BoosterDriver: "

void BoosterUnload(PDRIVER_OBJECT);
NTSTATUS BoosterCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS BoosterWrite(PDEVICE_OBJECT, PIRP Irp);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING _RegistryPath) {
	UNREFERENCED_PARAMETER(_RegistryPath);
	KdPrint((DRIVER_PREFIX "DriverEntry 0x%p\n", DriverObject));

	// Set major functions to indicate supported functions
	DriverObject->DriverUnload = BoosterUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = BoosterCreateClose;					// Required to open a handle to any device which the driver creates
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = BoosterCreateClose;					    // Required to close a handle to any device which the driver creates
	DriverObject->MajorFunction[IRP_MJ_WRITE] = BoosterWrite;							// Make the driver respond to WriteFile() 
	//DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = BoosterDeviceControl;

	// Create a device object for the client to talk to
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_OBJECT device_obj = NULL;
	UNICODE_STRING symlink_name = RTL_CONSTANT_STRING(L"\\??\\Booster");
	UNICODE_STRING device_name = RTL_CONSTANT_STRING(L"\\Device\\Booster");

	// Create a device object for the client to talk to
	status = IoCreateDevice(
		DriverObject,						// Pointer to the driver object for the caller
		0,									// No device extension
		&device_name,						// Name of the device name
		FILE_DEVICE_UNKNOWN,				// Specify that this is a software device
		0,									// No additional information for the device
		FALSE,								// Device is not exclusive 
		&device_obj);						// Receive the Device object
	if (!NT_SUCCESS(status)) {
		KdPrint((DRIVER_PREFIX "Error creating device (0x%X)\n", status));
		return status;
	}
//	device_obj->Flags |= DO_BUFFERED_IO;

	// Create symbolic link
	status = IoCreateSymbolicLink(&symlink_name, &device_name);
	if (!NT_SUCCESS(status)) {
		KdPrint((DRIVER_PREFIX "Error creating symbolic link (0x%X)\n", status));
		IoDeleteDevice(device_obj);
		return status;
	}

	return status;
}

// Dispatch routine for Create/Close
NTSTATUS BoosterCreateClose(PDEVICE_OBJECT _DriverObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(_DriverObject);

	HANDLE h_curr_pid = PsGetCurrentProcessId();
	ULONG u_curr_pid = HandleToULong(h_curr_pid); 

	if (IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_CREATE) {
		KdPrint((DRIVER_PREFIX "Create called from process %u\n", u_curr_pid));
	}
	else {
		KdPrint((DRIVER_PREFIX "Close called from process %u\n", u_curr_pid));
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);
	return STATUS_SUCCESS;
}

// Dispatch routine for Write 
NTSTATUS BoosterWrite(PDEVICE_OBJECT _DriverObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(_DriverObject);

	ULONG info = 0;
	PETHREAD thread = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION irp_sp = IoGetCurrentIrpStackLocation(Irp);
	if (irp_sp->Parameters.Write.Length != sizeof(ThreadData)) {
		status = STATUS_BUFFER_TOO_SMALL;
		goto io;
	}

	ThreadData * p_data = (ThreadData*)(Irp->UserBuffer);
	if (p_data == NULL || p_data->TargetPriority < 1 || p_data->TargetPriority > 31) {
		status = STATUS_INVALID_PARAMETER;
		goto io;
	}

	HANDLE h_tid = ULongToHandle(p_data->ThreadId);
	status = PsLookupThreadByThreadId(h_tid, &thread);
	if (!NT_SUCCESS(status)) goto io;

	KPRIORITY _old_priority = KeSetPriorityThread(thread, p_data->TargetPriority);
	KdPrint((DRIVER_PREFIX "Changed priority from %ld to %d", _old_priority, p_data->TargetPriority));
	ObDereferenceObject(thread);
	info = sizeof(ThreadData);

io:
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, 0);
	return status;
}

// Device Unload routine for driver
void BoosterUnload(PDRIVER_OBJECT DriverObject) {
	KdPrint((DRIVER_PREFIX "Unloading Routine - Driver Object: 0x%p\n", DriverObject));
	UNICODE_STRING symlink_name = RTL_CONSTANT_STRING(L"\\??\\Booster");
	IoDeleteSymbolicLink(&symlink_name);
	if (DriverObject->DeviceObject) {
		IoDeleteDevice(DriverObject->DeviceObject);
	}
}