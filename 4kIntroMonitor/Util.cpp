#include <windows.h>

#ifndef _DEBUG
extern "C"
{
	int _fltused = 0x9875;
	int _purecall()
	{
		ExitProcess(static_cast< UINT >(-1));
	}
}
#endif
//--------------------------------------------------------------------------------
extern "C"
{
	void zeroMem(void* dst, unsigned int siz)
	{
		for (unsigned int s = 0; s < siz; s++)
		{
			((unsigned char*)dst)[s] = 0;
		}
	}
	//! @brief memset()‚Ì‘ã‘ÖŽÀ‘•
	//! @return pDst‚Æ“™‰¿
	void* opt_memset(void* pDst, unsigned char value, size_t op_size)
	{
		__asm{
			mov al, BYTE PTR value;			/* const unsigned char fill = value; */
			xor ebx, ebx;								/* int i=0;	*/
			mov ecx, DWORD PTR op_size;	/* const size_t size = op_size; */
			mov edi, pDst;							/* unsigned char* pFill = static_cast< unsigned char* >(pDst); */
	MEMFILL:												/* while(i!=size){ */
			mov BYTE PTR[edi+ebx], al;	/*  pFill[i] = fill; */
			inc ebx;										/*  ++i; */
			loop MEMFILL;								/* } */
		}

		return pDst;
	}

	//! @brief memcpy()‚Ì‘ã‘ÖŽÀ‘•
	//! @return pDst‚Æ“™‰¿
	void* opt_memcpy(void* pDst, const void* pSrc, size_t op_size)
	{
		__asm{
			mov  ecx, DWORD PTR op_size;
			mov  esi, pSrc;
			mov  eax, ecx;
			mov  edi, pDst;
			shr  ecx, 2;
			rep movsd;
			mov  ecx, eax;
			and  ecx, 3;
			rep movsb;
		}

		return pDst;
	}
}

void* tmalloc(unsigned int size)
{
	//return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	return GlobalAlloc(GPTR, size);
}

void tfree(void* pt)
{
	//HeapFree(GetProcessHeap(), 0, pt);
	GlobalFree(pt);
}

HANDLE OpenFile(const char* filename)
{
	return CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}
void CloseFile(HANDLE handle)
{
	CloseHandle(handle);
}

bool WriteToFile(const char* filename, const char* buf)
{
	HANDLE h = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h != INVALID_HANDLE_VALUE)
	{
		unsigned long w;
		WriteFile(h, buf, lstrlen(buf), &w, NULL);
		CloseHandle(h);
		return true;
	}
	return false;
}

void ReadFileToBuffer(HANDLE handle, char*& buffer, unsigned long& readsize)
{
	if (buffer != NULL)
	{
		tfree(buffer);
		buffer = NULL;
	}
	buffer = (char*)tmalloc(GetFileSize(handle , NULL) + 1);
	ReadFile(handle, buffer, GetFileSize(handle, NULL), &readsize, NULL);
	buffer[readsize] = '\0';
}

bool NeedUpdateFileTimeStamp(HANDLE handle, FILETIME& oldtime)
{
	FILETIME ftFileTime;
	GetFileTime(handle, NULL, NULL, &ftFileTime);
	bool r = false;
	if ((oldtime.dwHighDateTime < ftFileTime.dwHighDateTime)
	||  (oldtime.dwLowDateTime < ftFileTime.dwLowDateTime))
	{
		r = true;
		oldtime = ftFileTime;
	}
	return r;
}


void GetNowTimeString(char* buf)
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	wsprintf(buf, "%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
}

//
//time_t GetFileStampTime(const char* filename)
//{
//	struct stat stat_buf;
//	if(stat(filename,&stat_buf)==0) //get file-time-stamp
//		return stat_buf.st_mtime;
//	else
//		return 0;
//}
//
//bool ChechUpdate(const char* filename, time_t& beftime)
//{
//	time_t ftm = GetFileStampTime(filename);
//	if(beftime < ftm) //file modified
//	{
//		beftime = ftm;
//		return true;
//	}
//	return false;
//}
//
//
//unsigned long GetPerfFreq()
//{
//	LARGE_INTEGER nFreq;
//	QueryPerformanceFrequency(&nFreq);
//	return static_cast<unsigned long>(nFreq.QuadPart);
//}
//__int64 GetPerfCount()
//{
//	LARGE_INTEGER nCount;
//	QueryPerformanceCounter(&nCount);	
//	return nCount.QuadPart;
//}