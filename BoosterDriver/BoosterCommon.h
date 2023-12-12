#pragma once
typedef struct _ThreadData {
	_In_	ULONG ThreadId;
	_In_	int TargetPriority;
} ThreadData, *PThreadData;