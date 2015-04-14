/*
	windd64 (c) Luis González Fernández 2015
	luisgf@luisgf.es
	https://www.luisgf.es/
*/

#include "windd64.h"

LPVOID xmalloc(size_t size) {
	LPVOID ptr = malloc(size);
	if (!ptr) {
		wprintf(L"xmalloc(%d): Memory allocation error\n", size);
		exit(-1);
	}
	return ptr;
}

VOID Usage(LPCTSTR ProgName) {
	wprintf(L"Usage: %s source destination BufferSize\n\n", ProgName);
	wprintf(L"Source:\t\tA block device or file. Like \\\\.\\PhysicalDrive0\n");
	wprintf(L"Destination:\tA block device or file. Like D:\\image.windd\n");
	wprintf(L"Buffer:\t\tAmount of memory in MB to use as buffer. Like: 100\n");
	wprintf(L"\n\n");
	exit(0);
}

VOID Disclaimer() {
	wprintf(L">>> windd v%d.%d - By Luis Gonzalez Fernandez\n", VERSION_MAJOR, VERSION_MINOR);
	wprintf(L"WARNING: This program commes without warranty and can damage your computer\n");
	wprintf(L"if used incorrectly. Use with caution.\n\n");
}

DWORD WINAPI ReadSect(LPVOID lpParam)
{
	DWORD dwRead;			// bytes readed
	BOOL rs = FALSE;
	PTPARAMS param = (PTPARAMS)lpParam;			// Thread parameters
	LONGLONG cursect = param->StartSector;
	DWORD dwWaitResult;
	PDATA data = NULL;
	SIZE_T TotalReaded = 0;
	LONGLONG NumSectorsToRead = 0;

	while (cursect != param->DiskSectors) {
		if (param->DiskSectors - cursect < param->SectorsAtTime) {
			// estamos al final del dispositivo y no hay tantos bloques que leer.			
			NumSectorsToRead = param->DiskSectors - cursect;
		}
		else {			
			NumSectorsToRead = param->SectorsAtTime;
		}

		if (param->cola->size > param->MemBuff) {
			// Queue Full. Sleep...
			Sleep(100);
		}
		else {
			data = (PDATA)xmalloc(sizeof(DATA));
			data->sector_num = NumSectorsToRead;
			data->size = param->SectorSize * (DWORD)data->sector_num;
			data->ptr = (LPVOID)xmalloc(data->size);
			rs = ReadFile(param->hDev, data->ptr, data->size, &dwRead, 0);  // read sector	

			if (dwRead != data->size) {
				wprintf(L"Error reading sector: %lu\n", param->StartSector);
				exit(-1);
			}

			dwWaitResult = WaitForSingleObject(
				param->Mutex,    // handle to mutex
				INFINITE);  // no time-out interval

			switch (dwWaitResult)
			{
				// The thread got ownership of the mutex
			case WAIT_OBJECT_0:
				__try {
					param->cola->ptr->push(data);
					param->cola->size += data->size;
					TotalReaded += data->size;
					cursect += data->sector_num;
				}
				__finally {
					ReleaseMutex(param->Mutex);
				}
				break;
			}
			wprintf(L"\rCompleted: %.2f %%", (float)((float)(cursect * 100) / (float)param->DiskSectors));
		}
	}
	wprintf(L"\rData Readed: %lu bytes\n", TotalReaded);
	return rs;
}

DWORD WINAPI WriteSect(LPVOID lpParam)
{
	PTPARAMS param = (PTPARAMS)lpParam;
	LONGLONG sectors_writed = { 0 };
	DWORD dwWaitResult;
	PDATA data = NULL;
	BOOL rs = FALSE;
	SIZE_T TotalWrited = 0;

	while (sectors_writed != param->DiskSectors) {
		dwWaitResult = WaitForSingleObject(
			param->Mutex,    // handle to mutex
			INFINITE);  // no time-out interval

		switch (dwWaitResult)
		{
			// The thread got ownership of the mutex
		case WAIT_OBJECT_0:
			__try {
				if (param->cola->size == 0) {
					//wprintf(L"\rQueue Depleted. Waiting...\n");
					Sleep(100);
				}
				else {
					data = (PDATA)param->cola->ptr->front();

					DWORD written;					
					rs = WriteFile(param->hDev, data->ptr, data->size, &written, NULL);

					if (written > 0 && written < data->size) {
						wprintf(L"\nError, Out of space at destination.\n");
						exit(-1);
					}
					
					if (!rs) {
						wprintf(L"\nError Writing to destination. %lu\n", GetLastError());
						exit(-1);
					}					
					
					param->cola->ptr->pop();
					param->cola->size -= data->size;
					TotalWrited += data->size;
					sectors_writed += data->sector_num;

					free(data->ptr);
					free(data);
					data = NULL;
				}
			}
			__finally {				
				ReleaseMutex(param->Mutex);
			}
			break;
		}
	}
	wprintf(L"Data Writed: %lu bytes\n", TotalWrited);
	return rs;
}

/*
** Get the disk geometry info
*/

BOOL GetDriveGeometry(HANDLE hDisk, PDWORD SectorSize, PLONGLONG DiskSectors, PLONGLONG DiskSize) {
	BOOL bResult = FALSE;                 // results flag
	DWORD junk = 0;                     // discard results
	DISK_GEOMETRY_EX pdg = { 0 };
	STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR pAlignmentDescriptor = { 0 };	
	STORAGE_PROPERTY_QUERY Query;

	ZeroMemory(&Query, sizeof(STORAGE_PROPERTY_QUERY));
	Query.PropertyId = StorageAccessAlignmentProperty;

	/*
	* First  try using STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR, if not supported we revert
	* to DISK_GEOMETRY_EX
	*/

	bResult = DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY, 
		&Query, sizeof(STORAGE_PROPERTY_QUERY),
		&pAlignmentDescriptor, sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR),
		&junk, NULL);
	
	if (bResult) {
		*SectorSize = pAlignmentDescriptor.BytesPerPhysicalSector;
	} 

	bResult = DeviceIoControl(hDisk,                       // device to be queried
			IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, // operation to perform
			NULL, 0,                       // no input buffer
			&pdg, sizeof(pdg),            // output buffer
			&junk,                         // # bytes returned
			(LPOVERLAPPED)NULL);          // synchronous I/O	

	if (bResult) {		
		if (!*SectorSize)
			*SectorSize = pdg.Geometry.BytesPerSector;

		*DiskSize = pdg.DiskSize.QuadPart;
		*DiskSectors = pdg.DiskSize.QuadPart / *SectorSize;
	}

	return (bResult);
}

/*
* Open the source and destination descriptors. Fail otherwhise.
*/
BOOL OpenDescriptors(LPTSTR InDev, LPTSTR OutDev, PHANDLE hInDev, PHANDLE hOutDev)
{
	DWORD desired_mask = (DWORD)(GENERIC_READ | GENERIC_WRITE);
	DWORD acces_mask = (DWORD)(FILE_SHARE_READ | FILE_SHARE_WRITE);

	*hInDev = CreateFileW(InDev, desired_mask, acces_mask, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, 0);
	if (*hInDev == INVALID_HANDLE_VALUE)
	{
		std::cout << "Open source descriptor error: " << GetLastError();
		CloseHandle(*hInDev);
		return FALSE;
	}
	
	SetFilePointer(*hInDev, 0, 0, FILE_BEGIN); // empezamos en el sector 0

	*hOutDev = CreateFile(OutDev, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, 0);
	if (*hOutDev == INVALID_HANDLE_VALUE)
	{
		std::cout << "Out destination descriptor error: " << GetLastError();
		CloseHandle(*hOutDev);
		return FALSE;
	}	
	SetFilePointer(*hOutDev, 0, 0, FILE_BEGIN); // empezamos en el sector 0

	return TRUE;
}

int _tmain(int argc, _TCHAR* argv[])
{
	LPTSTR ifdev;
	LPTSTR ofdev;
	 
	HANDLE hInDev = NULL;
	HANDLE hOutDev = NULL;

	// Disk Geometry
	LONGLONG DiskSize = { 0 };			// disk size in bytes
	LONGLONG DiskSectors = { 0 };		// num of sectors in disk
	DWORD SectorSize = 4096;				// Physical sector size, def=4096

	std::queue <LPVOID> cola;
	BQUEUE data = { &cola, 0};					// data queue		 

	// Buffering
	DWORD SectorsAtTime;
	DWORD MemBuffer;

	// Thread synchronization
	HANDLE hMutex;
	HANDLE hThread[2] = { 0 };
	DWORD ThreadID[2] = { 0 };
	
#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
	/* A program using VSS must run in elevated mode */
	HANDLE hToken;
	OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &hToken);
	DWORD infoLen;

	TOKEN_ELEVATION elevation;
	GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &infoLen);
	if (!elevation.TokenIsElevated)
	{
		wprintf(L"This program must run in elevated mode\n");
		return -1;
	}


#else
#error you are using an old version of sdk or not supported operating system
#endif	
	if (argc != 4) {
		Usage(argv[0]);		
	}
	else {
		ifdev = argv[1];
		ofdev = argv[2];
		MemBuffer = MB_TO_BYTES(wcstol(argv[3], NULL, 10));				
	}

	if (!OpenDescriptors(ifdev, ofdev, &hInDev, &hOutDev)) {		
		return -1;
	}

	if (GetDriveGeometry(hInDev, &SectorSize, &DiskSectors, &DiskSize))
	{
		if (MemBuffer < SectorSize) {
			wprintf(L"Memory buffer is not enough\n");
			return -1;
		}		
	} else {
		// No Drive geometry. This is a file HANDLE.
		LARGE_INTEGER FileSize;
		GetFileSizeEx(hInDev, &FileSize);

		DiskSize = FileSize.QuadPart;
		DiskSectors = DiskSize / SectorSize;
	}
	SectorsAtTime = MB_TO_BYTES(20) / SectorSize;

	/*
	** Creamos el Mutex
	*/

	hMutex = CreateMutex(NULL, FALSE, NULL);

	if (hMutex == NULL)
	{
		wprintf(L"CreateMutex() error: %d\n", GetLastError());
		return -1;
	}

	/*
		Mostramos algo de informacion
	*/

	Disclaimer();
	wprintf(L"%s => %s\n", ifdev, ofdev);
	wprintf(L"Disk Size: %I64d Sector Size: %lu Sectors: %I64d\n", DiskSize, SectorSize, DiskSectors);

	/*
	* Creamos los threads del lector y escritor
	*/
	TPARAMS ReaderParams = { 0 };
	ReaderParams.hDev = hInDev;
	ReaderParams.cola = &data;
	ReaderParams.StartSector = 0;
	ReaderParams.SectorSize = SectorSize;
	ReaderParams.DiskSectors = DiskSectors;
	ReaderParams.MemBuff = MemBuffer;
	ReaderParams.Mutex = hMutex;
	ReaderParams.SectorsAtTime = SectorsAtTime;

	hThread[0] = CreateThread(NULL, 0, ReadSect, &ReaderParams, 0, &ThreadID[0]);

	TPARAMS WriterParams = { 0 };
	WriterParams.hDev = hOutDev;
	WriterParams.cola = &data;
	WriterParams.StartSector = 0;
	WriterParams.SectorSize = SectorSize;
	WriterParams.DiskSectors = DiskSectors;
	WriterParams.Mutex = hMutex;
	WriterParams.SectorsAtTime = SectorsAtTime;
	
	hThread[1] = CreateThread(NULL, 0, WriteSect, &WriterParams, 0, &ThreadID[1]);

	WaitForMultipleObjects(2, hThread, TRUE, INFINITE);

	wprintf(L"Done!\n");
	CloseHandle(hInDev);
	CloseHandle(hOutDev);

	return 0;
}
