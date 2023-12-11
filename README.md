# Booster

A PoC and code walkthrough to demonstrate how to facilitate communication between userland applications and Windows kernel driver. This is a follow-up to [my last Windows Kernel development repository](https://github.com/whokilleddb/HelloWorldDriver) where I document my journey into Windows Kernel land - while giving extensive code walkthroughs. 

In this repository, we write a Client and a Driver which work together to boost a thread's Base Priority. 

## Usage

To send a signal to the Driver to increase the base priority of a thread, use the following command:

```
BoosterClient.exe <Thread ID> <Target Priority>
```

## Walkthrough

This part of the guide walks you through the Driver and Client code to explain the underlying concepts. First, we take a look into the driver itself which explores concepts like Handling Dispatch routines, Major Functions, etc, while the Client covers topics like how to use `CreateFile()` and `WriteFile()` to communicate with a driver. 

Also, we briefly touch upon IRQs but more upon that in future articles. 

## References

This article is directly influenced by [@zodicon's Windows Internal training](https://training.trainsec.net/view/courses/windows-kernel-programming-1) and I recommend everyone to check it out.  

### The Driver

We break down this 

#### DriverEntry

Looking at the `DriverEntry()` function of the Driver, it has the following code:
```c
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING _RegistryPath) {
  // Set major functions to indicate supported functions
  DriverObject->DriverUnload = BoosterUnload;
  DriverObject->MajorFunction[IRP_MJ_WRITE] = BoosterWrite;
  DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = BoosterCreateClose;

  // Create a device object for the client to talk to
  PDEVICE_OBJECT device_obj = NULL;
  UNICODE_STRING device_name = RTL_CONSTANT_STRING(L"\\Device\\Booster");
  NTSTATUS status = IoCreateDevice(DriverObject, 0, &device_name,	FILE_DEVICE_UNKNOWN, 0,	FALSE, &device_obj);
  if (!NT_STATUS(status))	return status;
  device_obj->Flags |= DO_BUFFERED_IO;

  // Create symbolic link
  UNICODE_STRING symlink_name = RTL_CONSTANT_STRING(L"\\??\\Booster");
  status = IoCreateSymbolicLink(&symlink_name, &device_name);
  if (!NT_SUCCESS(status)) {
    IoDeleteDevice(device_obj);
    return status;
  }

  return status;
}
```

There are three major parts to the function - Setting the major functions to indicate the functions our driver supports, creating a device object for the client to interact with, and finally create a symlink for the client to call `CreateFile()` on. 

The first part of the code sets the necessary function pointers:
 - First we point set the `DriverUnload` member of the `DriverObject` which points to the unload routine.
 - Then we set the `MajorFunction` members. The `MajorFunction` array contains a list of function pointers which serve as entrypoints for the Driver's dispathc routines. These indicate the functionalities supported by the driver. In our case, we support three routines:
	- `IRP_MJ_CREATE`: A routine to deal with requests sent by the client when it tries to open a handle to the Device object 
	- `IRP_MJ_CLOSE`: A routine to deal with requests sent by the client when it tries to close the handle to the Device object 
	- `IRP_MJ_WRITE`: A routine to deal with requests sent by the client when it tries to transfer data to the driver using operations like `WriteFile()` or `NtWriteFile()`

For the sake of simplicity, we will point the major functions indicated by `IRP_MJ_CREATE` and `IRP_MJ_WRITE` to the same dispatch routine. 

Next up, we create a Device for the Client to interact with. We use the `RTL_CONSTANT_STRING` macro to initialize a `UNICODE_STRING` with the full device name. 

### The Client

With the driver done, we can take a look at the client which we would use to send messages to the driver itself. The pseudo-code for the client looks as such:

```c
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

#include "..\BoosterDriver\BoosterCommon.h"

int main(int argc, char* argv[]) {
	if (argc != 3) {
		usage();
		return -1;
	}

	ULONG thread_id = (ULONG)atoi(argv[1]);
	int t_priority = atoi(argv[2]);

	// Check for thread id
    ...

	// Check for thread priority
    ...

	// Try to print current base thread priority
	...

	// Make a call to driver
	ThreadData t_data = {0};
	t_data.ThreadId = thread_id;
	t_data.TargetPriority = t_priority;

	HANDLE hDevice = CreateFile(L"\\\\.\\BoosterDriver", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	BOOL bRes = WriteFile(hDevice, &t_data, sizeof(t_data), &ret, NULL); 
	
	printf("[i] Priority changed from %d -> %d", t_data.CurrentPriority, t_data.TargetPriority);
	CloseHandle(hDevice);

	return 0;
}
```

Before we begin diving into `main()`, notice we include the `BoosterCommon.h` header which we had previously used in the Driver. We include this header file so we can have access to the `ThreadData` structure to have a uniform definition throughout. 

Looking into `main()`, we take in the target thread id and the priority we want to set it to from the command line. Then, after performing some checks to make sure the inputs are reasonable, we try to print the current thread's base priority.

Now comes the interesting part of the code. We initialize a `ThreadData` with the provided thread id and the target priority. This is the structure we will use to communicate with the Driver. 

Next, we open a handle to the `BoosterDriver` Device with:
```
HANDLE hDevice = CreateFile(L"\\\\.\\BoosterDriver", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
```

The parameters to `CreateFile()` are important here:
- `lpFileName`: This indicates the [device naming co](https://learn.microsoft.com/en-us/windows/win32/fileio/defining-an-ms-dos-device-name)