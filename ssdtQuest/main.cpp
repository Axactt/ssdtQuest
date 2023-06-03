//?? For only inlcuding printf otherwise weird error ofsnprintf glocbals scope 
//! Do not include <cstdio>

#include<stdio.h> 

#include<ntifs.h>

#include"main.h"



VOID ssdtQuestUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath);



//!Names the code section where the specified function definitions are placed.
//!  pragma must occur between a function declarator and the function definition for the named functions.

#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, ssdtQuestUnload)

//! DriverEntry like a main function of c which returns NTSTATUS
extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
	
	UNREFERENCED_PARAMETER(pRegistryPath);

	//! assign the Irp_Major_function dispatch routines function pointers  of driver

	pDriverObject->DriverUnload = ssdtQuestUnload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = ssdtQuestCreateClose;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = ssdtQuestCreateClose;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ssdtQuestDispatchDeviceControl;
	

//! initialize the device-name and symbolic-link UNICODE string
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_OBJECT pDeviceObject = nullptr;
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\ssdtQuest"); //! Initialize device name unicode
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\ssdtQuest"); //! Initialize symLink unicode
	bool symLinkCreated = false;

	//!ROBUST ERROR HANDLING:
	//! The trick we’ll use is a do / while(false) block, which is not really a loop, 
	//! but it allows getting out of the block with a simple break statement 
	//! in case something goes wrong
	do
	{
		status = IoCreateDevice(pDriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDeviceObject); //! To create the device object with assigned driver internal name
		if (!NT_SUCCESS(status))
		{
			printf("Failed to create device object.Error:0x%08x\n", status);
			break;
		}

		

		status = IoCreateSymbolicLink(&symLink, &devName); //! accepts the symbolic link and the target of the lnk

		if (!NT_SUCCESS(status))
		{
			printf("Symbolic link creation failed. Delete the device object.\n");
			
			break;
		}
		symLinkCreated = true;

	} while (false);

	//! Outside loop check status and undo any creation if status is not success
	if (!NT_SUCCESS(status))
	{
		if (symLinkCreated)
			IoDeleteSymbolicLink(&symLink);
		if(pDeviceObject)
			IoDeleteDevice(pDeviceObject); //! Delete the device object if symlink creation fails
	}

	//! If all ok then set up device object flags
	pDeviceObject->Flags &= (~DO_DEVICE_INITIALIZING);
	printf("DriverEntry Exit status = %d\n", status);
	return status;
}

// Implementations

//! a helper function that simplifies completing an IRP with a given staus and Information
//! wraps up IoCompleteRequest to be send at the end of a dispatch routine
//! Returns the status received in Irp back on request completion

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0)
{

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;

}

VOID ssdtQuestUnload(PDRIVER_OBJECT pDriverObject)
{
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\ssdtQuest");
	
	//! Delete the symbolic link and device object in unload routine
	
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(pDriverObject->DeviceObject);
}