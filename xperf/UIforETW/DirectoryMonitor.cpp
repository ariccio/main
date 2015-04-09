/*
Copyright 2015 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "stdafx.h"
#include "DirectoryMonitor.h"
#include <assert.h>

DirectoryMonitor::DirectoryMonitor(CWnd* pMainWindow)
	: mainWindow_(pMainWindow)
{
}

// This function monitors the traceDir_ directory and sends a message to the main thread
// whenever anything changes. That's it. All UI work is done in the main thread.
DWORD WINAPI DirectoryMonitor::DirectoryMonitorThreadStatic(LPVOID pVoidThis)
{
	DirectoryMonitor* pThis = static_cast<DirectoryMonitor*>(pVoidThis);
	return pThis->DirectoryMonitorThread();
}

//For warning numbers in the range 4700-4999, which are the ones associated with code generation, the state of the warning in effect when the compiler encounters the open curly brace of a function will be in effect for the rest of the function.
//Using the warning pragma in the function to change the state of a warning that has a number larger than 4699 will only take effect after the end of the function.
//https://msdn.microsoft.com/en-us/library/2c8f766e(v=vs.120).aspx
//^
//| That's SO intuitive!
#pragma warning(push, 3)
#pragma warning(disable:4702)//C4702: unreachable code

DWORD DirectoryMonitor::DirectoryMonitorThread()
{
	HANDLE hChangeHandle = FindFirstChangeNotification(traceDir_->c_str(), FALSE,
				FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);

	if (hChangeHandle == INVALID_HANDLE_VALUE)
	{
		assert(0);
		return 0;
	}

	HANDLE handles[] = { hChangeHandle, hShutdownRequest_ };
	
	//See: return statement after loop?
	for (;;)
	{
		const DWORD dwWaitStatus = WaitForMultipleObjects(ARRAYSIZE(handles), &handles[0], FALSE, INFINITE);

		switch (dwWaitStatus)
		{
		case WAIT_OBJECT_0:
			mainWindow_->PostMessage(WM_UPDATETRACELIST, 0, 0);
			if (FindNextChangeNotification(hChangeHandle) == FALSE)
			{
				assert(0);
				return 0;
			}
			break;
		case WAIT_OBJECT_0 + 1:
			// Shutdown requested.
			return 0;

		case WAIT_TIMEOUT:
			//We asked for INFINITE timeout, Unexpected!
			assert(0);
			return 0;

		case WAIT_FAILED:
			//[WaitForMultipleObjects] has failed. To get extended error information, call GetLastError.
			assert(0);
			return 0;


		default:
			assert(0);
			return 0;
		}
	}

	//TODO: why is loop with no condition? All paths return on first iteration?
	//See: #pragma at top of function!
	assert(0);
	return 0;
#pragma warning(pop)
}

void DirectoryMonitor::StartThread(const std::wstring* traceDir)
{
	assert(hThread_ == 0);
	assert(hShutdownRequest_ == 0);
	traceDir_ = traceDir;
	// No error checking -- what could go wrong?
	hThread_ = CreateThread(nullptr, 0, DirectoryMonitorThreadStatic, this, 0, 0);
	hShutdownRequest_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);;
}

DirectoryMonitor::~DirectoryMonitor()
{
	if (hThread_)
	{
		SetEvent(hShutdownRequest_);
		WaitForSingleObject(hThread_, INFINITE);
		CloseHandle(hThread_);
		CloseHandle(hShutdownRequest_);
	}
}
