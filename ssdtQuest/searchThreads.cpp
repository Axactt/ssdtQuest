#include<ntifs.h>
#include"main.h"
#include"SysInfoStructs.h"
#include <intrin.h>


#pragma warning(disable:4055)

#ifdef _WIN64

#pragma alloc_text(PAGE,GetW32pServiceTable64)

//? ULONG_PTR is unsigned _int64
NTSTATUS GetW32pServiceTable64(ULONG_PTR* ServiceDescriptorTableShadow, ULONG_PTR* ntpServiceTable, ULONG_PTR* w32pServiceTable)
{
	//!Read IA32_LSTAR Msr IA-32e Mode System Call Target Address(R / W) Target RIP 
	//! for the called procedure when SYSCALL is executed in 64 - bit mod 
	//! nt!kiSystemCall64 start address of routine in IA32_LSTAR
	PUCHAR pStartAddress = (PUCHAR)__readmsr(0xc0000082);

	//!Expecting nt!KiSystemServiceRepeat to be within 1024 bytes 
	//! of the syscall dispatcher nt!kiSystemCall64 (IA32_LSTAR address)
	//! kiSystemservicerepeat contains the address for KiServiceDescriptorTable and KiserviceDescriptorTable shadow in LEA opcode as a 
	PUCHAR pEndAddress = (PUCHAR)((ULONG_PTR)pStartAddress + 0x400);

	ULONG pattern1 = 0x000043f7;
	ULONG pattern2 = 0x00008d4c;

	DbgPrint("0x%llX", pStartAddress);
	DbgPrint("0x%llX", pEndAddress);

	while (++pStartAddress < pEndAddress) // Search for the pattern of address
	{
		if (((*(PULONG)pStartAddress & 0xffff) == pattern1) && (((*(PULONG)pStartAddress - 7) & 0xffff) == pattern2))
		{
			*ServiceDescriptorTableShadow = (ULONG_PTR)pStartAddress + (ULONG) * (PULONG)(pStartAddress - 4);
			*ntpServiceTable = (ULONG_PTR) * (ULONGLONG*)(*ServiceDescriptorTableShadow);
			*w32pServiceTable = (ULONG_PTR) * (ULONGLONG*)(*ServiceDescriptorTableShadow + 0x20);
			break;

		}




	}
	
	return STATUS_SUCCESS;
}



#else

typedef struct tagSYSTEM_SERVICE_TABLE
{
	PULONG ServiceTable;
	PULONG_PTR  CounterTable;
	ULONG_PTR ServiceLimit;
	PUCHAR ArgumentTable;
} SYSTEM_SERVICE_TABLE, * PSYSTEM_SERVICE_TABLE, ** PPSYSTEM_SERVICE_TABLE;

extern   NTSTATUS PsLookupThreadByThreadId(
	HANDLE HANDLE,
	PETHREAD* Thread
);
typedef NTSTATUS(NTAPI* ZWQUERYSYSTEMINFORMATION)(
	IN _SYSTEM_INFORMATION_CLASS,
	OUT PVOID SystemInformation,
	IN ULONG SystemInformationLength,
	OUT PULONG ReturnLength OPTIONAL
	);
ZWQUERYSYSTEMINFORMATION ZwQuerySystemInformation;


static const UINT32 gPoolTag = 'prvt';
UCHAR* buffer;

#pragma alloc_text(PAGE, GetW32pServiceTable32)

NTSTATUS GetW32pServiceTable32(HANDLE ProcessId, ULONG* ServiceDescriptorTableShadow, ULONG* ntpServiceTable, ULONG* w32pServiceTable)
{
	NTSTATUS status = 0;
	ULONG   length;
	UNICODE_STRING funcName;
	_SYSTEM_PROCESS_INFORMATION* procInfo = NULL;
	ULONG NumberOfThreads = 0;
	PETHREAD ethread = NULL;
	ULONG oWin32Thread = 0;
	ULONG oServiceTable = 0;

	if (RtlIsNtDdiVersionAvailable(0x0A000004)) // Only tested until Windows 10 Red Stone 2 (Version 1703)
		return STATUS_UNSUCCESSFUL;
	/*
	else if (RtlIsNtDdiVersionAvailable(NTDDI_WIN10)) {
		oWin32Thread = 0x124;
		oServiceTable = 0x3c;
	}

	else if (RtlIsNtDdiVersionAvailable(NTDDI_WINBLUE)) {
		oWin32Thread = 0x124;
		oServiceTable = 0x3c;
	}
	*/
	else if (RtlIsNtDdiVersionAvailable(NTDDI_WIN8)) // Windows 8.x 32-bit and Windows 10. 
	{
		oWin32Thread = 0x124;
		oServiceTable = 0x3c;
	}
	else if (RtlIsNtDdiVersionAvailable(NTDDI_WIN7))
	{
		oWin32Thread = 0x18c;
		oServiceTable = 0x0bc;
	}
	else
		return STATUS_UNSUCCESSFUL; // Previous Windows releases not in scope

	RtlInitUnicodeString(&funcName, L"ZwQuerySystemInformation");
	ZwQuerySystemInformation = (ZWQUERYSYSTEMINFORMATION)MmGetSystemRoutineAddress(&funcName);
	if (!ZwQuerySystemInformation)
	{
		DbgPrint("ZwQuerySystemInformation not available");
		return STATUS_UNSUCCESSFUL;
	}

	do
	{
		status = ZwQuerySystemInformation(SystemProcessInformation, NULL, 0, &length);
		if (status != STATUS_INFO_LENGTH_MISMATCH)
		{
			return status;
		}
		if (length == 0)
		{
			return STATUS_UNSUCCESSFUL;
		}
		buffer = ExAllocatePoolWithTag(PagedPool, length, gPoolTag);
		if (buffer == NULL)
		{
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		status = ZwQuerySystemInformation(SystemProcessInformation, buffer, length, NULL);
		if (!NT_SUCCESS(status))
		{
			ExFreePool(buffer);
			buffer = NULL;
			if (status != STATUS_INFO_LENGTH_MISMATCH)
			{
				return status;
			}
		}

	} while (buffer == NULL);
	DbgPrint("buffer: %x buffer length: %x", buffer, length);

	procInfo = (_SYSTEM_PROCESS_INFORMATION*)buffer;
	while (procInfo)
	{

		if (procInfo->UniqueProcessId == ProcessId)
		{
			DbgPrint("Found Process");
			NumberOfThreads = procInfo->NumberOfThreads;
			DbgPrint("Number of Threads: %d", NumberOfThreads);

			for (ULONG i = 0; i < NumberOfThreads; i++)
			{
				__try
				{
					status = PsLookupThreadByThreadId(procInfo->Threads[i].ClientId.UniqueThread, &ethread);
					if (!NT_SUCCESS(status))
					{
						DbgPrint("PsLookupThreadByThreadId Error %X", status);
						ExFreePool(buffer);
						return status;
					}
					if (*(ULONG*)(((BYTE*)ethread) + oWin32Thread)) // Only GUI Threads.
					{
						*ServiceDescriptorTableShadow = (ULONG) * (ULONG*)(((BYTE*)ethread) + oServiceTable);
						*ntpServiceTable = (ULONG) * (ULONG*)*ServiceDescriptorTableShadow;
						*w32pServiceTable = (ULONG) * (ULONG*)(*ServiceDescriptorTableShadow + 0x10);
						ObDereferenceObject((PVOID)ethread);
						break;
					}
					else
						DbgPrint("Not a Gui thread");

					ObDereferenceObject((PVOID)ethread);
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					DbgPrint("Exception while cycling threads");
				}
			}
			break;
		}
		if (!procInfo->NextEntryOffset)
			break;
		procInfo = (_SYSTEM_PROCESS_INFORMATION*)(((BYTE*)procInfo) + procInfo->NextEntryOffset);
	}

	ExFreePool(buffer);

	return STATUS_SUCCESS;
}
#endif

