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
#include "Utility.h"
#include <fstream>

std::vector<std::wstring> split(const std::wstring& s, const char c)
{
	std::wstring::size_type i = 0;
	std::wstring::size_type j = s.find(c);

	std::vector<std::wstring> result;
	while (j != std::wstring::npos)
	{
		result.push_back(s.substr(i, j - i));
		i = ++j;
		j = s.find(c, j);
	}

	if (!s.empty())
		result.push_back(s.substr(i, s.length()));

	return result;
}

std::vector<std::wstring> GetFileList(const std::wstring& pattern, const bool fullPaths)
{
	std::wstring directory;
	if (fullPaths)
		directory = GetDirPart(pattern);
	WIN32_FIND_DATA findData;
	HANDLE hFindFile = FindFirstFileEx(pattern.c_str(), FindExInfoStandard,
				&findData, FindExSearchNameMatch, NULL, 0);

	std::vector<std::wstring> result;
	if (hFindFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			result.push_back(directory + findData.cFileName);
		} while (FindNextFile(hFindFile, &findData));

		VERIFY( FindClose(hFindFile) );
	}

	return result;
}

// Load a file and convert to a string. If the file contains
// an embedded NUL then the resulting string will be truncated.
std::wstring LoadFileAsText(const std::wstring& fileName)
{
	std::ifstream f;
	f.open(fileName, std::ios_base::binary);
	if (!f)
		return L"";

	// Find the file length.
	f.seekg(0, std::ios_base::end);
	size_t length = static_cast<size_t>( f.tellg() );
	f.seekg(0, std::ios_base::beg);

	// Allocate a buffer and read the file.
	std::vector<char> data(length + 2);
	f.read(&data[0], length);
	if (!f)
		return L"";

	// Add a multi-byte null terminator.
	data[length] = 0;
	data[length+1] = 0;

	const wchar_t bom = 0xFEFF;
	if (memcmp(&bom, &data[0], sizeof(bom)) == 0)
	{
		// Assume UTF-16, strip bom, and return.
		return reinterpret_cast<const wchar_t*>(&data[sizeof(bom)]);
	}

	// If not-UTF-16 then convert from ANSI to wstring and return
	return AnsiToUnicode(&data[0]);
}


void WriteTextAsFile(const std::wstring& fileName, const std::wstring& text)
{
	std::ofstream outFile;
	outFile.open(fileName, std::ios_base::binary);
	if (!outFile)
		return;

	const wchar_t bom = 0xFEFF; // Always write a byte order mark
	outFile.write(reinterpret_cast<const char*>(&bom), sizeof(bom));
	outFile.write(reinterpret_cast<const char*>(text.c_str()), text.size() * sizeof(text[0]));
}

void SetRegistryDWORD(HKEY root, const std::wstring& subkey, const std::wstring& valueName, DWORD value)
{
	HKEY key;
	const LONG openRootKeyResult = RegOpenKeyEx(root, subkey.c_str(), 0, KEY_ALL_ACCESS, &key);
	ASSERT( openRootKeyResult == ERROR_SUCCESS );

	//we should properly report error instead of silently failing?
	if (openRootKeyResult == ERROR_SUCCESS)
	{
		//If [RegSetValueEx] succeeds, the return value is ERROR_SUCCESS.
		//If [RegSetValueEx] fails, the return value is a nonzero error code defined in Winerror.h.
		const LONG setResult = RegSetValueEx(key, valueName.c_str(), 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));

		ASSERT( setResult == ERROR_SUCCESS );


		const LONG keyCloseResult = RegCloseKey(key);
		ASSERT( keyCloseResult == ERROR_SUCCESS );
	}
}

void CreateRegistryKey(HKEY root, const std::wstring& subkey, const std::wstring& newKey)
{
	HKEY key;
	const LONG openRootKeyResult = RegOpenKeyEx(root, subkey.c_str(), 0, KEY_ALL_ACCESS, &key);
	if (openRootKeyResult == ERROR_SUCCESS)
	{
		HKEY resultKey;
		const LONG createKeyResult = RegCreateKey(key, newKey.c_str(), &resultKey);
		if (createKeyResult == ERROR_SUCCESS)
		{
			const LONG keyCloseResult = RegCloseKey(resultKey);
			ASSERT( keyCloseResult == ERROR_SUCCESS );
		}
		const LONG keyCloseResult = RegCloseKey(key);
		ASSERT( keyCloseResult == ERROR_SUCCESS );
	}
}

std::wstring GetEditControlText(HWND hEdit)
{
	std::wstring result;//unused??

	//If the window has no text, the return value [of GetWindowTextLength] is zero. To get extended error information, call GetLastError.
	//If the function succeeds, the return value [of GetWindowTextLength] is the length, in characters, of the text.
	//The return value, [can be used] to guide buffer allocation.
	const int length = GetWindowTextLength(hEdit);

	if ( length == 0 )
	{
		//Is this a failure condition?
		return L"";
	}


	std::vector<wchar_t> buffer(length + 1);

	//If the function succeeds, the return value [of GetWindowText] is the length, in characters, of the copied string, not including the terminating null character. 
	//If the window has no title bar or text, if the title bar is empty, or if the window or control handle is invalid, the return value [of GetWindowText] is zero.
	const int windowTextResult = GetWindowText(hEdit, &buffer[0], buffer.size());

	//it's kinda silly to "Double-verify that the buffer is null-terminated" if we haven't even checked that GetWindowText successfully wrote to `buffer`.
	//Hell, all characters up to ( buffer.size( ) - 1 ) could be nonzero garbage, and null-terminating that means that we're still returning uninitialized memory!
	if ( windowTextResult == 0 )
	{
		//TODO: pending discussion, return empty string on failure.
		return L"";
	}



	// Double-verify that the buffer is null-terminated.
	buffer.at( buffer.size() - 1 ) = 0;
	return &buffer[0];
}

std::wstring AnsiToUnicode(const std::string& text)
{
	// Determine number of wide characters to be allocated for the
	// Unicode string.
	const int cCharacters = static_cast<int>( text.size() ) + 1;

	std::vector<wchar_t> buffer(cCharacters);

	// Convert to Unicode.
	std::wstring result;
	if (MultiByteToWideChar(CP_ACP, 0, text.c_str(), cCharacters, &buffer[0], cCharacters))
	{
		// Double-verify that the buffer is null-terminated.
		buffer[buffer.size() - 1] = 0;
		result = &buffer[0];
		return result;
	}

	return result;
}

// Get the next/previous dialog item (next/prev in window order and tab order) allowing
// for disabled controls, invisible controls, and wrapping at the end of the tab order.

static bool ControlOK(HWND win)
{
	if (!win)
		return false;
	if (!IsWindowEnabled(win))
		return false;
	if (!(GetWindowLong(win, GWL_STYLE) & WS_TABSTOP))
		return false;
	// You have to check for visibility of the parent window because during dialog
	// creation the parent window is invisible, which renders the child windows
	// all invisible - not good.
	if (!IsWindowVisible(win) && IsWindowVisible(GetParent(win)))
		return false;
	return true;
}

static HWND GetNextDlgItem(HWND win, bool Wrap)
{
	HWND next = GetWindow(win, GW_HWNDNEXT);
	while (next != win && !ControlOK(next))
	{
		if (next)
			next = GetWindow(next, GW_HWNDNEXT);
		else
		{
			if (Wrap)
				next = GetWindow(win, GW_HWNDFIRST);
			else
				return 0;
		}
	}
	assert(!Wrap || next);
	return next;
}

void SmartEnableWindow(HWND Win, BOOL Enable)
{
	assert(Win);
	if (!Enable)
	{
		HWND hasfocus = GetFocus();
		bool FocusProblem = false;
		HWND focuscopy;
		for (focuscopy = hasfocus; focuscopy; focuscopy = (GetParent)(focuscopy))
			if (focuscopy == Win)
				FocusProblem = true;
		if (FocusProblem)
		{
			HWND nextctrl = GetNextDlgItem(Win, true);
			if (nextctrl)
				SetFocus(nextctrl);
		}
	}
	::EnableWindow(Win, Enable);
}

std::wstring GetFilePart(const std::wstring& path)
{
	PCWSTR const pLastSlash = wcsrchr(path.c_str(), '\\');
	if (pLastSlash)
		return pLastSlash + 1;
	// If there's no slash then the file part is the entire string.
	return path;
}

std::wstring GetFileExt(const std::wstring& path)
{
	std::wstring filePart = GetFilePart(path);
	PCWSTR const pLastPeriod = wcsrchr(filePart.c_str(), '.');
	if (pLastPeriod)
		return pLastPeriod;
	return L"";
}

std::wstring GetDirPart(const std::wstring& path)
{
	PCWSTR const pLastSlash = wcsrchr(path.c_str(), '\\');
	if (pLastSlash)
		return path.substr(0, pLastSlash + 1 - path.c_str());
	// If there's no slash then there is no directory.
	return L"";
}

std::wstring CrackFilePart(const std::wstring& path)
{
	std::wstring filePart = GetFilePart(path);
	const std::wstring extension = GetFileExt(filePart);
	if (!extension.empty())
	{
		filePart = filePart.substr(0, filePart.size() - extension.size());
	}

	return filePart;
}

int DeleteOneFile(HWND hwnd, const std::wstring& path)
{
	std::vector<std::wstring> paths;
	paths.push_back(path);
	return DeleteFiles(hwnd, paths);
}

int DeleteFiles(HWND hwnd, const std::vector<std::wstring>& paths)
{
	_NullNull_terminated_ std::vector<wchar_t> fileNames;
	for (auto& path : paths)
	{
		// Push the file name and its NULL terminator onto the vector.
		fileNames.insert(fileNames.end(), path.c_str(), path.c_str() + path.size());
		fileNames.push_back(0);
	}

	// Double null-terminate.
	fileNames.push_back(0);

	SHFILEOPSTRUCT fileOp =
	{
		hwnd,
		FO_DELETE,
		&fileNames[0],
		NULL,
		FOF_ALLOWUNDO | FOF_FILESONLY | FOF_NOCONFIRMATION,
	};
	// Delete using the recycle bin.
	int result = SHFileOperation(&fileOp);

	return result;
}

void SetClipboardText(const std::wstring& text)
{
	BOOL cb = OpenClipboard(GetDesktopWindow());
	if (!cb)
		return;

	VERIFY( EmptyClipboard( ) );

	const rsize_t length = (text.size() + 1) * sizeof(wchar_t);
	HANDLE hmem = GlobalAlloc(GMEM_MOVEABLE, length);
	if (hmem)
	{
		void* ptr = GlobalLock(hmem);
		if (ptr != NULL)
		{
			memcpy(ptr, text.c_str(), length);
			
			//If the memory object is still locked after decrementing the lock count, the return value [of GlobalUnlock] is a nonzero value.
			//If the memory object is unlocked after decrementing the lock count, [GlobalUnlock] returns zero and GetLastError returns NO_ERROR.
			//If [GlobalUnlock] fails, the return value is zero and GetLastError returns a value other than NO_ERROR.
			const BOOL unlockResult = GlobalUnlock(hmem);
			if ( unlockResult == 0 )
			{
				const DWORD lastErr = GetLastError( );
				ASSERT( lastErr == NO_ERROR );
			}

			//If [SetClipboardData] fails, the return value is NULL
			VERIFY( SetClipboardData(CF_UNICODETEXT, hmem) );
		}
	}

	VERIFY( CloseClipboard() );
}

int64_t GetFileSize(const std::wstring& path)
{
	LARGE_INTEGER result;
	HANDLE hFile = CreateFile(path.c_str(), GENERIC_READ,
		( FILE_SHARE_READ | FILE_SHARE_WRITE ), NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return 0;

	if (GetFileSizeEx(hFile, &result))
	{

		//If [CloseHandle] succeeds, the return value is nonzero.
		VERIFY( CloseHandle(hFile) );
		return result.QuadPart;
	}
	VERIFY( CloseHandle(hFile) );
	return 0;
}

bool Is64BitWindows()
{
	// http://blogs.msdn.com/b/oldnewthing/archive/2005/02/01/364563.aspx
	BOOL f64 = FALSE;
	bool bIsWin64 = IsWow64Process(GetCurrentProcess(), &f64) && f64;
	return bIsWin64;
}

WindowsVersion GetWindowsVersion()
{
	OSVERSIONINFO verInfo = { sizeof(OSVERSIONINFO) };
	// warning C4996: 'GetVersionExA': was declared deprecated
	// warning C28159: Consider using 'IsWindows*' instead of 'GetVersionExW'.Reason : Deprecated.Use VerifyVersionInfo* or IsWindows* macros from VersionHelpers.
#pragma warning(suppress : 4996)
#pragma warning(suppress : 28159)
	GetVersionEx(&verInfo);

	// Windows 10 preview has major version 10, if you have a compatibility
	// manifest for that OS, which this program should have.
	if (verInfo.dwMajorVersion > 6)
		return kWindowsVersion10;	// Or higher, I guess.

	// Windows 8.1 will only be returned if there is an appropriate
	// compatibility manifest.
	if (verInfo.dwMajorVersion == 6 && verInfo.dwMinorVersion >= 3)
		return kWindowsVersion8_1;

	if (verInfo.dwMajorVersion == 6 && verInfo.dwMinorVersion >= 2)
		return kWindowsVersion8;

	if (verInfo.dwMajorVersion == 6 && verInfo.dwMinorVersion >= 1)
		return kWindowsVersion7;

	if (verInfo.dwMajorVersion == 6)
		return kWindowsVersionVista;

	return kWindowsVersionXP;
}

bool IsWindowsServer()
{
	OSVERSIONINFOEXW osvi = { sizeof(osvi), 0, 0, 0, 0, { 0 }, 0, 0, 0, VER_NT_WORKSTATION };
	DWORDLONG        const dwlConditionMask = VerSetConditionMask(0, VER_PRODUCT_TYPE, VER_EQUAL);

	bool result = !VerifyVersionInfoW(&osvi, VER_PRODUCT_TYPE, dwlConditionMask);
	return result;
}



std::wstring FindPython()
{
#pragma warning(suppress:4996)
	const wchar_t* path = _wgetenv(L"path");
	if (path)
	{
		std::vector<std::wstring> pathParts = split(path, ';');
		for (auto part : pathParts)
		{
			std::wstring pythonPath = part + L"\\python.exe";
			if (PathFileExists(pythonPath.c_str()))
			{
				return pythonPath;
			}
		}
	}
	// No python found.
	return L"";
}
