
#ifndef INCLUDE_UTIL_H
#define INCLUDE_UTIL_H

#include <windows.h>

//---------------------------------------------------------------------
#ifndef _DEBUG
extern "C" int _fltused;
//__forceinline void * __cdecl operator new(unsigned size) {return HeapAlloc(GetProcessHeap(), 0, size);}
__forceinline void * __cdecl operator new(unsigned int size) {return GlobalAlloc(GPTR, size);}
__forceinline void * __cdecl operator new[](unsigned int size) {return operator new(size);}
//__forceinline void __cdecl operator delete(void *ptr) {HeapFree(GetProcessHeap(), 0, ptr);}
__forceinline void __cdecl operator delete(void *ptr) {GlobalFree(ptr);}
__forceinline void __cdecl operator delete[](void *ptr) {operator delete(ptr);}
//--------------------------------------------------------------------------------
extern "C"
{
	int _purecall();
}
#endif

//--------------------------------------------------------------------------------
extern "C"
{
	void zeroMem(void* dst, unsigned int siz);
	//! @brief memset()‚Ì‘ã‘ÖŽÀ‘•
	//! @return pDst‚Æ“™‰¿
	void* opt_memset(void* pDst, unsigned char value, size_t op_size);

	//! @brief memcpy()‚Ì‘ã‘ÖŽÀ‘•
	//! @return pDst‚Æ“™‰¿
	void* opt_memcpy(void* pDst, const void* pSrc, size_t op_size);
}

//--------------------------------------------------------------------------------
__forceinline long kftol(const float fSrc){// Default Ftol
	long fAns;
	_asm{
		fld fSrc
		fistp fAns
	}
	return fAns;
}

void* tmalloc(unsigned int size);
void tfree(void* pt);
HANDLE OpenFile(const char* filename);
void CloseFile(HANDLE handle);
bool WriteToFile(const char* filename, const char* buf);
void ReadFileToBuffer(HANDLE handle, char*& buffer, unsigned long& readsize);
bool NeedUpdateFileTimeStamp(HANDLE handle, FILETIME& oldtime);
void GetNowTimeString(char* buf);

//time_t GetFileStampTime(const char* filename);
//bool ChechUpdate(const char* filename, time_t& beftime);
//unsigned long GetPerfFreq();
//__int64 GetPerfCount();

#endif // INCLUDE_UTIL_H
