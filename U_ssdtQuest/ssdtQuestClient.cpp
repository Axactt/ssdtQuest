#include<windows.h>
#include<TlHelp32.h>
#include<iostream>
#include"ErrorLogger.h"


//! #define IOCTL_DispatchDeviceControl_Function CTL_CODE(DeviceType,Function,Method,Access)
// Define some easier to comprehend aliases:
//   METHOD_DIRECT_TO_HARDWARE (writes, aka METHOD_IN_DIRECT)
//   METHOD_DIRECT_FROM_HARDWARE (reads, aka METHOD_OUT_DIRECT)


#define IOCTL_GET_SSDT_DIRECT_OUT_READ_IO  CTL_CODE(FILE_DEVICE_UNKNOWN,0X1326,METHOD_DIRECT_FROM_HARDWARE,FILE_ANY_ACCESS)

#ifndef _WIN64

unsigned int getProcessIdbyName(LPCWSTR name)
{

	DWORD pid{};
	PROCESSENTRY32 pe32;
	HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapShot == INVALID_HANDLE_VALUE)
	{
		logger::logA("ToolHelp32Snapshot failed: %u\n", GetLastError());
		return 13;
	}
	ZeroMemory(&pe32, sizeof(PROCESSENTRY32));

	BOOL bRet = Process32First(hSnapShot, &pe32);
	if (!bRet)
	{
		logger::logA("Process32First failed: %u\n", GetLastError());
		CloseHandle(hSnapShot);
		return 13 ;

	}

		do
		{
			if (_wcsicmp(pe32.szExeFile, name) == 0)
			{
				pid = pe32.th32ProcessID;
				CloseHandle(hSnapShot);
				break;
			}
			bRet = Process32Next(hSnapShot, &pe32);

		} while (bRet);

		return pid;
}


#endif

int main()
{

	SC_HANDLE hSCManager;
	SC_HANDLE hService;
	SERVICE_STATUS sStatus; //! Struct to pass info to api setServivceStatus
	HANDLE hDevice;

	DWORD dwReturn; //!lpBytesReturned cannot be null:no of bytes returned actually by DeviceIoControl inlpOutBuffer
	WCHAR dPath[MAX_PATH];
	WCHAR fFile[MAX_PATH];
	DWORD pid = 0;
	WCHAR szTemp[256] = { 0 }; // output buffer to the string by DeviceIoControl api

#ifdef _WIN64
	//todo understand use of ExpandEnvironmentStrings() api here
	//! Expands  environment-variable strings and replace them with the values defined for the current user
	ExpandEnvironmentStrings(L"%SYSTEMROOT%\\System32\\Drivers", dPath, MAX_PATH);

	//! Writes formatted data to the specified buffer. Any arguments are converted and copied to the output
	//!  buffer according to the corresponding format specification in the format string. The function 
	//! appends a terminating null character to the characters it writes, but the return value does not include
	//!  the terminating null character in its character count. 
	// int WINAPIV wsprintfA([out] LPSTR  unnamedParam1,[in]  LPCSTR unnamedParam2,...);
	wsprintf(fFile, TEXT("%s\\%s"), dPath, L"ssdtQuest64.sys");

	//! CopyFile() to copy an existing file to a new file 
	BOOL bCopied = CopyFile(L"ssdtQuest64.sys", fFile, 0);

	if (!bCopied)
	{

		logger::logW(L"Failed to copy to %s (make sure Administrator)\n", fFile);
		return 1;
	}

#else

	GetCurrentDirectory(MAX_PATH, dPath);
	wsprintf(fFile, TEXT("%s\\%s"), dPath, L"ssdtQuest32.sys"); //! writes the full path with file-name

#endif

	logger::logA("Loading driver\n");

	// getchar();
	//!Establishes a connection to the service control manager on the specified computer and
	//!  opens the specified service control manager database.
	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

	if (!hSCManager)
	{
		logger::logA("Opening handle to Service_control_manger create service failed:%u\n", GetLastError());
		return 13;

	}

	logger::logA("Creating Service\n");

	//! Creates a service object and adds it to the specified service control manager database.
	
	hService = CreateService(hSCManager, L"ssdtQuest", L"ssdtQuest Driver", SERVICE_START | DELETE | SERVICE_STOP, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, fFile, NULL, NULL, NULL, NULL, NULL);

	if (!hService)
	{
		// should not happen  because service is deleted on exit
		logger::logA("Error Creating service.Error: %d \r\n", GetLastError());
		CloseServiceHandle(hSCManager);
		return 13;
		
	}

	logger::logA("Opening service\n");
	//! OPENS AN existing service
	hService = OpenService(hSCManager, L"ssdtQuest", SERVICE_START | DELETE | SERVICE_STOP);
	if (!hService)
	{
		logger::logA("Error In Opening service.Error: %d \r\n", GetLastError());
		return 13;
	}

	logger::logA("Starting  Service\n");

	BOOL bRet = StartService(hService, 0, NULL); //! driver service do not receive addl arguments than handle
	if (!bRet)
	{
		logger::logA("Error Starting service.Error: %d \r\n", GetLastError());
		return 13;

	}
#ifndef _WIN64

	//!  csrss.exe always runs and has gui threads
	//! So will be definitely containing KeServiceDescritorTableShadow second ssdt
	pid = getProcessIdbyName(L"csrss.exe"); 
	if (!pid)
	{
		//! something weird happened
		
		logger::logA(" Could not get Process Id by name of csrss.exe.\n");
		return 13;

	}

#endif

	logger::logA("Retrieving nt!KeServiceDescriptorTableShadow, nt!KiServiceTable and win32k!W32pServiceTable\n\n");

	//! Createfile() api by user_client to open a handle to driver_device by liking the symbolic_name 
	//! Corresponds to IRP_MJ_CREate Major function dispatch routine in driver code to open handle
	//! createFile fucntion accepts a symbolic link to that leads to device object
	
	 hDevice = CreateFile(L"\\\\.\\ssdtQuest", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		logger::logA("Error opening a handle to driver device_object by createFile.Error:%d\n", GetLastError());
		return 13;
	}
	//!Zero_out the string buffer which is the output buffer 
	RtlZeroMemory(szTemp, sizeof(szTemp));

	//! note we have not used PID for win64, it is initialzed as Zero(0)
	
	//! DeviceIoControl function called to assign DeviceDispatchControl{IRP_MJ_DEviceIoControl} Major_function 
	//! to driver part to execute the functionality as per IOCTL_code

	DeviceIoControl(hDevice, IOCTL_GET_SSDT_DIRECT_OUT_READ_IO, &pid, sizeof(DWORD), szTemp, sizeof(szTemp), &dwReturn, NULL);

	if (dwReturn)
		logger::logW(L"%s\n\n", szTemp);
	else
	{
		logger::logA("ZERO bytes returned in lpBytesReturned parameter of DeviceIoControl.");
	
	}
	
	if (!ControlService(hService, SERVICE_CONTROL_STOP, &sStatus)) //! For stopping the service
		printf("Error control service to stop. LastError %d\n", GetLastError());

	if (!DeleteService(hService)) // This shall remove the registry entries as well
		printf("Errror deleteService. LastError %d\n", GetLastError());
	CloseHandle(hDevice); //! Close the handle to kernel device_object opened by client
	CloseServiceHandle(hSCManager); //! Close service handle finally;

#ifdef  _WIN64

	DeleteFile(fFile); //! delete the driver .sys file from system driver's folder

#else
	Exit:

#endif //  

	logger::logA(" press <ENTER> TO exit.");
	
	getchar();
	
	return 0;

}