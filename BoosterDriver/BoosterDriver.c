#include <ntifs.h>
#define DRIVER_PREFIX "BoosterDriver: "

void BoosterUnload(PDRIVER_OBJECT);
NTSTATUS BoosterCreateClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS BoosterWrite(PDEVICE_OBJECT, PIRP Irp);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING _RegistryPath) {
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
	if (!NT_STATUS(status)) {
		KdPrint((DRIVER_PREFIX "Error creating device (0x%X)\n", status));
		return status;
	}
	device_obj->Flags |= DO_BUFFERED_IO;

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
	KdPrint((DRIVER_PREFIX "CreateClose\n"));

	if (IoGetCurrentIrpStackLocation(Irp)->MajorFunction == IRP_MJ_CREATE) {
		KdPrint((DRIVER_PREFIX "Create called from process %u\n",
			HandleToULong(PsGetCurrentProcessId())));
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);
	return STATUS_SUCCESS;
}

// Dispatch routine for Write 
NTSTATUS BoosterWrite(PDEVICE_OBJECT _DriverObject, PIRP Irp) {
	PIO_STACK_LOCATION irp_sp = IoGetCurrentIrpStackLocation(Irp);
	return STATUS_SUCCESS;
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