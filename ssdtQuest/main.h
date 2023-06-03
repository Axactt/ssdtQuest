#pragma once
#include<ntifs.h>
typedef unsigned int UINT;
typedef char* PCHAR;
typedef unsigned char BYTE;

//! Driver dispatch 
 NTSTATUS ssdtQuestCreateClose(PDEVICE_OBJECT DeviceObject, PIRP irp);
NTSTATUS ssdtQuestDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
//x NTSTATUS ssdtQuestUnsupportedFunc(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0);
#ifdef _WIN64

NTSTATUS GetW32pServiceTable64(ULONG_PTR* ServiceDescriptorTableShadow, ULONG_PTR* ntpServiceTable, ULONG_PTR* w32pServiceTable);

#else
NTSTATUS GetW32pServiceTable32(HANDLE processId, ULONG* ServiceDescriptorTableShadow, ULONG* ntpServiceTable, ULONG* w32pServiceTable);
#endif

#define IOCTL_GET_SSDT_DIRECT_OUT_READ_IO  CTL_CODE(FILE_DEVICE_UNKNOWN,0X1326,METHOD_DIRECT_FROM_HARDWARE,FILE_ANY_ACCESS)