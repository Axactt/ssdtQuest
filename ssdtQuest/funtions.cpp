#include <ntddk.h>
#include <Ntstrsafe.h>
#include "main.h"

NTSTATUS HandleIoctl_DirectOutIo(PIRP Irp, PIO_STACK_LOCATION pIoStackIrp, size_t* pdwDataWritten);

#pragma alloc_text(PAGE, ssdtQuestCreateClose)
#pragma alloc_text(PAGE, ssdtQuestDispatchDeviceControl)
#pragma alloc_text(PAGE,HandleIoctl_DirectOutIo)
#pragma alloc_text(PAGE,CompleteIrp)

NTSTATUS HandleIoctl_DirectOutIo(PIRP Irp, PIO_STACK_LOCATION pIoStackIrp, size_t* pdwDataWritten)
{
	NTSTATUS NtStatus = STATUS_UNSUCCESSFUL;
	void* pInputBuffer;   // unsigned long* in original code
	void* pOutputBuffer; // ? PWCHAR IN original code
	WCHAR ReturnData[256] = { 0 };
	size_t dwDataSize = 0;
	ULONG_PTR W32pServiceTable = 0;
	ULONG_PTR ntServiceTable = 0;
	ULONG_PTR ServiceDescriptorTableShadow = 0;

	DbgPrint("HandleIoctl_DirectOutIo Called");

	pInputBuffer = Irp->AssociatedIrp.SystemBuffer;

	pOutputBuffer = NULL;

	if (Irp->MdlAddress)
	{

		pOutputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	}

	if (pInputBuffer && pOutputBuffer)
	{

#ifdef _WIN64
		if (!NT_SUCCESS(GetW32pServiceTable64(&ServiceDescriptorTableShadow, &ntServiceTable, &W32pServiceTable)))
			goto CleanUp;
		//! String copy function in kernel mode copies the data to ReturnData variable
		NtStatus = RtlStringCbPrintfW(ReturnData, 512, L"nt!ServiceDescriptorTableShadow: 0x%llX\nService table (nt!KiServiceTable): 0x%llX\nService table (win32k!W32pServiceTable): 0x%llX", ServiceDescriptorTableShadow, ntServiceTable, W32pServiceTable);

#else
		DbgPrint("Application PID = '%d'", *pInputBuffer);
		if (!NT_SUCCESS(GetW32pServiceTable32((HANDLE)*pInputBuffer, &ServiceDescriptorTableShadow, &ntServiceTable, &W32pServiceTable)))
			goto CleanUp;
		NtStatus = RtlStringCbPrintfW(ReturnData, 512, L"nt!ServiceDescriptorTableShadow: 0x%X\nService table (nt!KiServiceTable): 0x%X\nService table (win32k!W32pServiceTable): 0x%X", ServiceDescriptorTableShadow, ntServiceTable, W32pServiceTable);
		
#endif
		if (!NT_SUCCESS(NtStatus))
			goto CleanUp;
		//! Function saves the ReturnData string length in dwDataSzie variable
		NtStatus = RtlStringCbLengthW (ReturnData, 512, &dwDataSize);
		if (!NT_SUCCESS(NtStatus))
			goto CleanUp;
		DbgPrint("%i>=%i", pIoStackIrp->Parameters.DeviceIoControl.OutputBufferLength, dwDataSize);
		if (pIoStackIrp->Parameters.DeviceIoControl.OutputBufferLength >= dwDataSize)
		{
			RtlCopyMemory(pOutputBuffer, &ReturnData, dwDataSize);
			*pdwDataWritten = dwDataSize;
			NtStatus = STATUS_SUCCESS;
		}
		else
		{
			*pdwDataWritten = dwDataSize;
			NtStatus = STATUS_BUFFER_TOO_SMALL;

		}

	}
CleanUp: return NtStatus;

}

NTSTATUS ssdtQuestDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{

	UNREFERENCED_PARAMETER(DeviceObject);
	NTSTATUS NtStaus = STATUS_INVALID_DEVICE_REQUEST;

	PIO_STACK_LOCATION pIoStackIrp = NULL;

	size_t dwDataWritten = 0;

	DbgPrint("ssdtQuestDispatchDeviceControl Called \r\n");

	pIoStackIrp = IoGetCurrentIrpStackLocation(Irp);

	if (pIoStackIrp)
	{
		auto& dic = pIoStackIrp->Parameters.DeviceIoControl; //! To set a variable to DeviceIoControl member of Parameters field
		switch (dic.IoControlCode)
		{
		case IOCTL_GET_SSDT_DIRECT_OUT_READ_IO:

			NtStaus = HandleIoctl_DirectOutIo(Irp, pIoStackIrp,  &dwDataWritten);
			break;
		}

	}

	
	return CompleteIrp(Irp, NtStaus, dwDataWritten);


}

NTSTATUS ssdtQuestCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	return CompleteIrp(Irp);
}