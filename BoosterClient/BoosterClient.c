#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

#include "..\BoosterDriver\BoosterCommon.h"

void usage() {
	printf("\nUsage:\n\tClient.exe <Thread ID> <Target Priority>\n");
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		usage();
		return -1;
	}

	ULONG thread_id = (ULONG)atoi(argv[1]);
	int t_priority = atoi(argv[2]);

	// Check for thread id
	if (thread_id == 0 || thread_id % 4 != 0) {
		printf("\n[i] Invalid Thread ID:\t\t%s\n", argv[1]);
		return -1;
	}

	// Check for thread priority
	if (t_priority == 0 || t_priority < 1 || t_priority > 31) {
		printf("\n[i] Invalid Thread Priority:\t\t%s\n", argv[2]);
		return -1;
	}

	printf("\n[i] Booster Client\n");

	// Try to print current thread priority
	HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, thread_id);
	if (hThread) {
		LONG _ct_priority = 0;
		BOOL bresult = GetThreadInformation(hThread, ThreadAbsoluteCpuPriority, &_ct_priority, sizeof(LONG));
		if (bresult) {
			printf("[i] Current Thread Base Priority:\t%ld\n", _ct_priority);
		}
		CloseHandle(hThread);
	}

	// Make a call to driver
	ThreadData t_data = {0};
	t_data.ThreadId = thread_id;
	t_data.TargetPriority = t_priority;

	HANDLE hDevice = CreateFile(L"\\\\.\\Booster", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hDevice == INVALID_HANDLE_VALUE) {
		printf("[!] Failed to open handle to Device (0x%x)\n", GetLastError());
		return -1;
	}
	
	DWORD ret;
	BOOL bRes = WriteFile(hDevice, &t_data, sizeof(t_data), &ret, NULL); 
	if (!bRes) {
		printf("[!] WriteFile() failed\n");
		CloseHandle(hDevice);
		return -1;
	}
	printf("[i] Priority changed from %d -> %d", t_data.CurrentPriority, t_data.TargetPriority);
	CloseHandle(hDevice);
	return 0;
}
