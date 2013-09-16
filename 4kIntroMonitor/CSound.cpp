//v090
//#include "stdafx.h"
#include "CSound.h"

//extern "C" const GUID CLSID_DirectSound      = { 0x47D4D946, 0x62E8, 0x11CF, 0x93, 0xBC, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 };
//extern "C" const GUID IID_IDirectSound       = { 0x279AFA83, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60 };
//extern "C" const GUID IID_IDirectSoundNotify = { 0xB0210783, 0x89CD, 0x11D0, 0xAF, 0x08, 0x00, 0xA0, 0xC9, 0x25, 0xCD, 0x16 };

#include "Util.h"
//extern void zeroMem(void* dst, unsigned int siz);
///extern "C" void* opt_memset(void* pDst, unsigned char value, size_t op_size);
//extern "C" void* opt_memcpy(void* pDst, const void* pSrc, size_t op_size);

#define copyMem(Destination,Source,Length) opt_memcpy((Destination),(Source),(Length))

#define ENABLE_SOUND_POLLING (1)

#include <assert.h>
#define CORE_ASSERT(a,b) assert(a)

//static
#if DIRECTSOUND_VERSION >= 0x0800
IDirectSound8* CSound::m_pSoundObject = NULL;
#else
IDirectSound* CSound::m_pSoundObject = NULL;
#endif
IDirectSoundBuffer* CSound::m_pPrimaryBuffer = NULL;
int CSound::m_nRef = 0;
HMODULE CSound::m_hDLL = NULL;
/*
void OutputDebugStringFormatted(const char* format, ...)
{
	char str[1024];
	va_list ap;
	va_start(ap,format);
	_vsnprintf(str, sizeof(str), format, ap);
	va_end(ap);

	::OutputDebugString(str);
}
*/

void CSound::QueryInitialize(){
	m_pSecondaryBuffer = NULL;
	
	//バッファの複製用
	m_nDuplicateLimit = 0;
	m_ppDuplicatedBuffer = NULL;

	m_Loader = NULL;

	m_isLoop		= FALSE;

	m_dwOneSplittedBufferSize = 0;
	m_pNotifyHandle = NULL;
	m_pSoundNotify = NULL;
	m_dwBufferLengthSec = 3;
	m_dwNotificationNum = 12;
	m_hThread = NULL;
	m_pDsbnotify = NULL;
	m_isStreamFile = FALSE;
	//m_dwPlayProgress = 0;

	m_isAllowRapidAccess = true;

	//InitializeCriticalSection(&m_csWriteBuffer);
	m_hThreadMessageDispatchEvent = NULL;

	m_nRef++;
}

CSound::CSound(){
	this->QueryInitialize();
	//m_nRef++;
	//this->QueryInitialize();
}

CSound::CSound(HWND hWnd, DWORD dwCoopLevel){
	//m_nRef++;
	this->QueryInitialize();
	//m_nRef++;
	//this->QueryInitialize();
	this->Initialize(hWnd, dwCoopLevel);
}

CSound::~CSound(){
	this->UnInitialize();
}

void CSound::MessageBox(const char* format, ...)
{
	//char* str = new char[1024];
	char str[4096];
	va_list ap;
	va_start(ap,format);
	wvsprintf(str,format,ap);
	va_end(ap);
	::MessageBox(NULL,str,"Message",MB_OK);
	//delete str;
}

int CSound::Initialize(HWND hWnd, DWORD dwCoopLevel)
{
	if(m_pSoundObject){
		return CS_E_NOCANDO;
	}

	if( !m_nRef ){
		this->QueryInitialize();
	}

	m_hDLL = LoadLibrary(TEXT("dsound.dll"));
#if DIRECTSOUND_VERSION >= 0x0800
	typedef HRESULT (WINAPI *pDirectSoundCreate)(LPCGUID, LPDIRECTSOUND8*, LPUNKNOWN);
	pDirectSoundCreate FDirectSoundCreate = (pDirectSoundCreate)GetProcAddress(m_hDLL, TEXT("DirectSoundCreate8"));
#else
	typedef HRESULT (WINAPI *pDirectSoundCreate)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
	pDirectSoundCreate FDirectSoundCreate = (pDirectSoundCreate)GetProcAddress(m_hDLL, TEXT("DirectSoundCreate"));
#endif

	//DirectSoundオブジェクトの作成
	if(FAILED(FDirectSoundCreate(NULL, &m_pSoundObject, NULL)))
	{
		_ASSERT(0);
		return CS_E_NULL_OBJECT;
	}

	if(FAILED(m_pSoundObject->SetCooperativeLevel(hWnd, dwCoopLevel)))	//協調レベルを設定
	{
		_ASSERT(0);
		return CS_E_NOCANDO;
	}

	if(CS_E_OK!=this->SetPrimaryBufferWaveFormat(2, 44100, 16)){
		_ASSERT(0);
		return CS_E_NOCANDO;
	}
	return CS_E_OK;
}

void CSound::CloseStreamThread(){
	//this->Stop();	//2回目以降の呼び出しのために停止
	if(m_hThread){
		PostThreadMessage(m_dwThreadId, WM_QUIT, 0, 0);
		WaitForSingleObject(m_hThread, INFINITE);

		CloseHandle(m_hThread);
		m_hThread = NULL;
	}
}

//サウンドオブジェクトを破棄
int CSound::UnInitialize()
{
	this->CloseStreamThread();
	SAFE_GLOBALFREE(m_pDsbnotify);
	SAFE_CLOSEHANDLE(m_pNotifyHandle);
	SAFE_RELEASE(m_pSoundNotify);

	//セカンダリバッファとその複製を破棄
	SAFE_RELEASE(m_pSecondaryBuffer);
	for(int i=0; i<m_nDuplicateLimit; i++)
	{
		SAFE_RELEASE(m_ppDuplicatedBuffer[i]);
	}
	SAFE_GLOBALFREE(m_ppDuplicatedBuffer);
	SAFE_RELEASE(m_pPrimaryBuffer);

	SAFE_DELETE(m_Loader);

	//DeleteCriticalSection(&m_csWriteBuffer);

	SAFE_CLOSEHANDLE(m_hThreadMessageDispatchEvent);

	if(!--m_nRef){
		SAFE_RELEASE(m_pSoundObject);
		FreeLibrary(m_hDLL);
	}
	return CS_E_OK;
}

int CSound::GetPrimaryBuffer(IDirectSoundBuffer** buffer, DWORD dwFlags)
{
	if(!m_pSoundObject)	return CS_E_NULL_OBJECT;
	//プライマリバッファを取得してくる
	DSBUFFERDESC dsbdesc;
	zeroMem(&dsbdesc, sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = dwFlags;
	dsbdesc.dwBufferBytes = 0;
	dsbdesc.lpwfxFormat = NULL;

	if(FAILED(m_pSoundObject->CreateSoundBuffer(&dsbdesc, buffer, NULL)))
	{
		_ASSERT(0);
		return CS_E_NULL_PRIMARY;
	}
	return CS_E_OK;
}

bool CSound::CreateBuffer(IDirectSoundBuffer** buffer, DSBUFFERDESC* dsbdesc, WAVEFORMATEX* wfx)
{
	//SAFE_RELEASE((*buffer));

	if(wfx != NULL && dsbdesc->lpwfxFormat == NULL)
		dsbdesc->lpwfxFormat = wfx;

	if(FAILED(m_pSoundObject->CreateSoundBuffer(dsbdesc, buffer, NULL)))
		return false;
	
	return true;
}

void CSound::SetPan(LONG nPan){
	if(!m_pSecondaryBuffer) return;
	m_pSecondaryBuffer->SetPan(nPan);
}

void CSound::SetVolume(LONG nVolume){
	if(!m_pSecondaryBuffer) return;
	m_pSecondaryBuffer->SetVolume(nVolume);
}

void CSound::SetFrequency(DWORD nFrequency){
	if(!m_pSecondaryBuffer) return;
	m_pSecondaryBuffer->SetFrequency(nFrequency);
}

LONG CSound::GetVolume(){
	if(!m_pSecondaryBuffer) return 0;
	LONG nVol;
	m_pSecondaryBuffer->GetVolume(&nVol);
	return CS_VOLUME_FIX_GET(nVol);
}

LONG CSound::GetPan(){
	LONG nPan;
	m_pSecondaryBuffer->GetPan(&nPan);
	return nPan;
}

void CSound::SetMasterPanAndVolume(LONG nVol, LONG nPan)
{
	if(CS_E_OK==GetPrimaryBuffer(&m_pPrimaryBuffer, DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_PRIMARYBUFFER)){
		m_pPrimaryBuffer->SetVolume(nVol);
		m_pPrimaryBuffer->SetPan(nPan);
	}
	SAFE_RELEASE(m_pPrimaryBuffer);
}

int CSound::SetPrimaryBufferWaveFormat(WORD Channels, DWORD SamplesPerSec, WORD BitsPerSample)
{
	if(CS_E_OK!=GetPrimaryBuffer(&m_pPrimaryBuffer, DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_PRIMARYBUFFER)) return CS_E_NULL_PRIMARY;

	WAVEFORMATEX wfx;
	zeroMem( &wfx, sizeof(WAVEFORMATEX) ); 
	wfx.wFormatTag      = WAVE_FORMAT_PCM; 
	wfx.nChannels       = Channels;
	wfx.nSamplesPerSec  = SamplesPerSec;
	wfx.wBitsPerSample  = BitsPerSample;
	wfx.nBlockAlign     = wfx.wBitsPerSample / 8 * wfx.nChannels;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	if(FAILED(m_pPrimaryBuffer->SetFormat(&wfx))) return CS_E_NOCANDO;

	SAFE_RELEASE(m_pPrimaryBuffer);
	return CS_E_OK;
}

int CSound::GetLoaderInterface(CSoundLoader** ppLoader, const char* szFileName, void* pMemData, DWORD dwMembufferSize){
	//*ppLoader = NULL;
	/* DO NOT FREE PPLOADER AT HERE! */
	//if( *ppLoader!=NULL ){
	//	SAFE_DELETE(*ppLoader);
	//}

	CSoundLoader* tmpLoader;
	tmpLoader = new WaveLoader();
	if(tmpLoader->IsLoadable(szFileName, pMemData, dwMembufferSize)){
		*ppLoader = tmpLoader;
		return CS_E_OK;
	}

#ifndef NOT_USE_OGG
	SAFE_DELETE(tmpLoader);
	tmpLoader = new OggVorbisLoader();
	if(tmpLoader->IsLoadable(szFileName, pMemData, dwMembufferSize)){
		*ppLoader = tmpLoader;
		return CS_E_OK;
	}
#endif

	SAFE_DELETE(tmpLoader);
	*ppLoader = NULL;
	return CS_E_UNEXP;
}

int CSound::Load(const char* szFileName, DSBUFFERDESC* pDsbdesc){
	return this->LoadInternal(szFileName, pDsbdesc);
}

////NEED TO REVIEW
//int CSound::LoadWaveFromMemory(void* pdata, WAVEFORMATEX* wfx, DWORD dwLength_byte, DSBUFFERDESC* pDsbdesc)
//{
//	if(!m_pPrimaryBuffer){		//プライマリバッファを取得する
//		if(CS_E_OK!=GetPrimaryBuffer(&m_pPrimaryBuffer, DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_PRIMARYBUFFER)) return CS_E_NULL_PRIMARY;
//	}
//	SAFE_RELEASE(m_pSecondaryBuffer);	//前のゴミを消しておく
//
//	if(NULL == pdata || NULL == wfx || 0 == dwLength_byte)	return CS_E_NULL;
//
//	DSBUFFERDESC dsbdesc;
//	ZeroMemory(&dsbdesc, sizeof(DSBUFFERDESC));
//	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
//	dsbdesc.dwFlags =	DSBCAPS_CTRLPAN|DSBCAPS_CTRLVOLUME|
//						DSBCAPS_CTRLFREQUENCY|
//						DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_CTRLPOSITIONNOTIFY|
//						DSBCAPS_GLOBALFOCUS;
//	dsbdesc.dwBufferBytes = dwLength_byte;
//	dsbdesc.lpwfxFormat = wfx;
//
//	CopyMemory(&m_wfx, wfx, sizeof(WAVEFORMATEX));
//
//	if(!CreateBuffer(&m_pSecondaryBuffer, &dsbdesc, NULL))	return CS_E_NOCANDO;	//セカンダリバッファの作成
//	if(!WriteDataToBuffer(&m_pSecondaryBuffer, pdata, 0, dwLength_byte)) return CS_E_NOCANDO;	//データをバッファに書き込む
//	SAFE_RELEASE(m_pPrimaryBuffer);
//	return CS_E_OK;
//}

int CSound::LoadInternal(const char* szFileName, DSBUFFERDESC* pDsbdesc, void* pData, unsigned long dwDataSize){

	CSoundLoader* pLoader;
	char errmsg[512];
	if(CS_E_OK!=this->GetLoaderInterface(&pLoader, szFileName, pData, dwDataSize)){
		wsprintf(errmsg, "Sound::%sの読み取りインターフェイス取得に失敗.\nファイルが存在するかもしくは対応形式か確認して下さい.", szFileName);
		::MessageBox(NULL, errmsg, "", MB_ICONEXCLAMATION|MB_OK|MB_TOPMOST);
		//FatalAppExit(0, errmsg);
		return CS_E_NOTFOUND;
	}
	if(CSL_E_OK != pLoader->QueryLoadFile(szFileName, pData, dwDataSize)){
		wsprintf(errmsg, "Sound::%sの読み取りに失敗.", szFileName);
		::MessageBox(NULL, errmsg, "", MB_ICONEXCLAMATION|MB_OK|MB_TOPMOST);
		//FatalAppExit(0, errmsg);
		return CS_E_UNEXP;
	}

	//初期化
	this->AddRef();
	this->UnInitialize();

	m_Loader = pLoader;

	if(!m_pPrimaryBuffer){		//プライマリバッファを取得する
		if(CS_E_OK!=GetPrimaryBuffer(&m_pPrimaryBuffer, DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_PRIMARYBUFFER)) return CS_E_NULL_PRIMARY;
	}

	DSBUFFERDESC dsbdesc;
	zeroMem(&dsbdesc, sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);

	//全体の長さとWFXの取得
	DWORD dwDataLength = m_Loader->GetDecodedLength();
	m_Loader->GetWaveFormatEx(&m_wfx);
	if(dwDataLength >= CS_LIMITLOADONMEMORY){//展開したときのサイズが1MB以上だったらストリーミング再生]
		//スレッド処理
		this->CloseStreamThread();
		m_hThreadMessageDispatchEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		m_hThread = CreateThread(NULL, 0, this->StreamThread, (void*)this, CREATE_SUSPENDED, &m_dwThreadId);		//スレッド生成
		// スレッド優先を変更
		SetThreadPriority( m_hThread, THREAD_PRIORITY_NORMAL ); 
		// スレッド開始
		ResumeThread( m_hThread );
		WaitForSingleObject(m_hThreadMessageDispatchEvent, INFINITE);// スレッドメッセージキューが作成されるのを待つ

		m_isStreamFile = TRUE;

		//セカンダリバッファ
		{
			SAFE_RELEASE(m_pSecondaryBuffer);
			dsbdesc.dwFlags =	DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_CTRLPOSITIONNOTIFY|
								DSBCAPS_GLOBALFOCUS|DSBCAPS_CTRLPAN|DSBCAPS_CTRLVOLUME|
								DSBCAPS_CTRLFREQUENCY|DSBCAPS_LOCSOFTWARE;

			if(pDsbdesc){
				dsbdesc.dwFlags = pDsbdesc->dwFlags;
				dsbdesc.guid3DAlgorithm = dsbdesc.guid3DAlgorithm;
			}
			dsbdesc.lpwfxFormat = &m_wfx;
			DWORD dwSize = m_wfx.nAvgBytesPerSec * m_dwBufferLengthSec / m_dwNotificationNum;
			dwSize -= dwSize % m_wfx.nBlockAlign;
			dsbdesc.dwBufferBytes = dwSize * m_dwNotificationNum;
			if(!CreateBuffer(&m_pSecondaryBuffer, &dsbdesc, NULL))	return CS_E_NOCANDO;

			m_dwOneSplittedBufferSize = dwSize;//区切られたバッファの１つのサイズ（バッファ全体はこれ*m_dwNotificationNum
		}

		//通知インターフェイス
#if !ENABLE_SOUND_POLLING
		{
			SAFE_RELEASE(m_pSoundNotify);
			if(FAILED(m_pSecondaryBuffer->QueryInterface(IID_IDirectSoundNotify, (void**)&m_pSoundNotify))){
				return CS_E_NOCANDO;
			}

			SAFE_GLOBALFREE(m_pDsbnotify);
			if(!(m_pDsbnotify = (DSBPOSITIONNOTIFY*)GlobalAlloc(GPTR, m_dwNotificationNum * sizeof(DSBPOSITIONNOTIFY)))){
				return CS_E_UNEXP;
			}

			m_pNotifyHandle = CreateEvent(NULL, FALSE, FALSE, NULL);	//通知ハンドルの作成
			for(DWORD i=0; i<m_dwNotificationNum; i++){
				//OutputDebugStringFormatted("[%2lu]:%lu\n", i, (m_dwOneSplittedBufferSize*i) + 1);
				m_pDsbnotify[i].dwOffset     = (m_dwOneSplittedBufferSize*i) + 1;// バッファを分割する。通知ポイントは、バッファの区切れ目から1バイト先。こうすることで、スペックの低いマシンでも249ms以内に次のバッファ区間を埋めればよいことになる。
				m_pDsbnotify[i].hEventNotify = m_pNotifyHandle;
			}
			if(FAILED(m_pSoundNotify->SetNotificationPositions(m_dwNotificationNum, m_pDsbnotify))){
				SAFE_GLOBALFREE(m_pDsbnotify);
				SAFE_RELEASE(m_pSoundNotify);
				SAFE_CLOSEHANDLE(m_pNotifyHandle);
				return CS_E_NOCANDO;
			}
		}
#endif
	}else{
		m_isStreamFile = FALSE;

		void* pdata = NULL;
		if(CSL_E_OK != m_Loader->GetDecodedData(&pdata, 0, 0, FALSE)){
		}

		dsbdesc.dwFlags =	DSBCAPS_CTRLPAN|DSBCAPS_CTRLVOLUME|
							DSBCAPS_CTRLFREQUENCY|
							DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_CTRLPOSITIONNOTIFY|
							DSBCAPS_GLOBALFOCUS;
		if(pDsbdesc){
			dsbdesc.dwFlags = pDsbdesc->dwFlags;
			dsbdesc.guid3DAlgorithm = dsbdesc.guid3DAlgorithm;
		}
		dsbdesc.dwBufferBytes = dwDataLength;
		dsbdesc.lpwfxFormat = &m_wfx;

		SAFE_RELEASE(m_pSecondaryBuffer);
		if(!CreateBuffer(&m_pSecondaryBuffer, &dsbdesc, NULL)) return CS_E_NOCANDO;		//セカンダリバッファの作成
		if(!WriteDataToBuffer(&m_pSecondaryBuffer, pdata, 0, dwDataLength))	return CS_E_NOCANDO;		//データをバッファに書き込む
		
		SAFE_GLOBALFREE(pdata);
	}
	SAFE_RELEASE(m_pPrimaryBuffer);
	return CS_E_OK;
}

int CSound::LoadMemoryImage(void* pData, unsigned long dwDataSize){
	if(NULL == pData || 0 == dwDataSize)	return CS_E_NULL;
	return this->LoadInternal(CSL_LOAD_MEMORYIMAGE, NULL, pData, dwDataSize);
}

//waveデータをサウンドバッファにコピー
bool CSound::WriteDataToBuffer(IDirectSoundBuffer** buffer, void* data, DWORD dwOffset, DWORD dwWriteSize)
{
	//EnterCriticalSection(&m_csWriteBuffer);
		void* pmem[2];
		DWORD alloc_size[2];
		
		if(NULL == data || m_pSecondaryBuffer == NULL)	return false;

		if(DSERR_BUFFERLOST == (*buffer)->Lock(dwOffset, dwWriteSize, &pmem[0], &alloc_size[0], &pmem[1], &alloc_size[1], 0)){//サウンドバッファをロック
			(*buffer)->Restore();
			if(DS_OK != (*buffer)->Lock(dwOffset, dwWriteSize, &pmem[0], &alloc_size[0], &pmem[1], &alloc_size[1], 0))	return false;
		}


		//読み込んだサイズ分コピー
		if(pmem[0]){
			CORE_ASSERT(dwWriteSize==alloc_size[0], "");
			copyMem(pmem[0], data, alloc_size[0]);
		}else if(pmem[1]){
			CORE_ASSERT(dwWriteSize==alloc_size[1], "");
			copyMem(pmem[1], data, alloc_size[1]);
		}else{
			CORE_ASSERT(0, "sound memory allocation error");
		}

		(*buffer)->Unlock(pmem[0], alloc_size[0], pmem[1], alloc_size[1]);	//バッファをアンロック
	//LeaveCriticalSection(&m_csWriteBuffer);
	return true;
}

//同一サウンド多重再生用コピー
void CSound::DuplicateBuffer(DWORD nDuplicate)
{
	if(!m_pSecondaryBuffer)	return;
	if(m_isStreamFile) return;

	if(m_ppDuplicatedBuffer!=NULL){	//後片付けを先にする
		for(int i=0; i<m_nDuplicateLimit; i++){
			SAFE_RELEASE(m_ppDuplicatedBuffer[i]);
		}
		SAFE_GLOBALFREE(m_ppDuplicatedBuffer);
	}

	if(nDuplicate > 0){
		m_ppDuplicatedBuffer = (IDirectSoundBuffer**)GlobalAlloc(GPTR, sizeof(IDirectSoundBuffer*) * nDuplicate);
		for(DWORD i=0; i<nDuplicate; i++){
			if(FAILED(m_pSoundObject->DuplicateSoundBuffer(m_pSecondaryBuffer, &m_ppDuplicatedBuffer[i]))){
				m_ppDuplicatedBuffer[i] = NULL;
			}
		}
	}else{
		if(m_ppDuplicatedBuffer!=NULL){
			for(int i=0; i<m_nDuplicateLimit; i++){
				SAFE_RELEASE(m_ppDuplicatedBuffer[i]);
			}
			SAFE_GLOBALFREE(m_ppDuplicatedBuffer);
		}
	}

	m_nDuplicateLimit = nDuplicate;	//デュプリケート数を保存
}

DWORD CSound::GetCurrentPosition()
{
	if(!m_pSecondaryBuffer)	return 0;
	if(m_isStreamFile){
		DWORD dwRet;
		PostThreadMessage(m_dwThreadId, CSL_MSG_GETCURPOS_BYTE, 0, (LPARAM)&dwRet);
		WaitForSingleObject(m_hThreadMessageDispatchEvent, INFINITE);
		//DWORD dwTmp = m_dwPlayProgress - (m_dwPlayProgress/(double)m_Loader->GetDecodedLength() * m_dwPlayProgress);
		return (DWORD)(dwRet / ((double)m_wfx.nAvgBytesPerSec/1000.0));
	}
	DWORD  now, now2;
	m_pSecondaryBuffer->GetCurrentPosition( &now, &now2 );
	return (DWORD)((double)now / (double)(m_wfx.nAvgBytesPerSec/1000.0));
}

DWORD CSound::GetFrequency()
{
	if(!m_pSecondaryBuffer)	return 0;
	DWORD  dwFreq;
	m_pSecondaryBuffer->GetFrequency(&dwFreq);
	return dwFreq;
}

int CSound::Play(DWORD position_ms, bool isLoop)
{
	if(!m_pSecondaryBuffer) return CS_E_NULL_SECONDARY;
	m_isLoop = isLoop;

	if(m_isStreamFile){
		//OpenStreamThread();
		//if(IsPlaying()){
		//PostThreadMessage(m_dwThreadId, CSL_MSG_SEEK_AND_PLAY, (DWORD)(position_ms * ((double)m_wfx.nAvgBytesPerSec/1000.0)), 0);
    PostThreadMessage(m_dwThreadId, CSL_MSG_SEEK_AND_PLAY, position_ms, 0);
		WaitForSingleObject(m_hThreadMessageDispatchEvent, INFINITE);
		//this->SetStreamCurosr((DWORD)(position_ms * ((double)m_wfx.nAvgBytesPerSec/1000.0)));
		//}
		//m_pSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
	}else{
		IDirectSoundBuffer* pBuffer = NULL;
		if(m_nDuplicateLimit > 0){	//セカンダリバッファのコピーを使う
			int id = GetInactiveBufferNo();//使用中でないバッファのインデックスを取得
			if(id == -1) return CS_E_NOCANDO;
			pBuffer = m_ppDuplicatedBuffer[id];
		}else{
			if( !m_isAllowRapidAccess ){
				DWORD dwStatus;
				m_pSecondaryBuffer->GetStatus( &dwStatus );
				bool isSecondaryPlaying = (dwStatus & DSBSTATUS_PLAYING);
				if( isSecondaryPlaying ){
					/* nop */
				}else{
					pBuffer = m_pSecondaryBuffer;
				}
			}else{
				pBuffer = m_pSecondaryBuffer;
			}
		}
		if( !pBuffer ){
			/* nop */
		}else{
			pBuffer->SetCurrentPosition( (DWORD)((double)position_ms * (double)(m_wfx.nAvgBytesPerSec/1000.0)) );
			pBuffer->Play(0, 0, isLoop ? DSBPLAY_LOOPING : 0);
			//m_pSecondaryBuffer->Play(0, 0, m_isStreamFile ? DSBPLAY_LOOPING : isLoop ? DSBPLAY_LOOPING : 0);
		}
	}
	return CS_E_OK;
}

//使用中でない複製バッファのIDを探す
int CSound::GetInactiveBufferNo()
{
	if(!m_ppDuplicatedBuffer) return -1;

	DWORD dwStatus;
	int max = m_nDuplicateLimit;
	for(int i=0; i<max; i++){
		if(m_ppDuplicatedBuffer[i]){
			m_ppDuplicatedBuffer[i]->GetStatus( &dwStatus );
			if(!(dwStatus & DSBSTATUS_PLAYING)){
				return i;
			}
		}
	}
	return -1;
}

int CSound::Stop()
{
	if(!m_pSecondaryBuffer) return CS_E_NULL_SECONDARY;
	if( m_isStreamFile ){
		PostThreadMessage(m_dwThreadId, CSL_MSG_STOP, 0, 0);
		WaitForSingleObject(m_hThreadMessageDispatchEvent, INFINITE);
	}else{
		m_pSecondaryBuffer->Stop();
		this->StopDuplicatedBuffer();	//まさか複製バッファの音だけ残しておきたいとか無いでしょ・・・
	}
	return CS_E_OK;
}

int CSound::StopDuplicatedBuffer(){
	if(!m_ppDuplicatedBuffer) return CS_E_NOCANDO;
	for(int i=0; i<m_nDuplicateLimit; i++){
		if(m_ppDuplicatedBuffer[i]){
			m_ppDuplicatedBuffer[i]->Stop();
			m_ppDuplicatedBuffer[i]->SetCurrentPosition(0);
		}
	}
	return CS_E_OK;
}

bool CSound::IsPlaying()
{
	if(!m_pSecondaryBuffer)	return 0;
	DWORD dwStatus;
	m_pSecondaryBuffer->GetStatus( &dwStatus );
	bool isSecondaryPlaying = (dwStatus & DSBSTATUS_PLAYING);
	return (isSecondaryPlaying || IsDuplicatedBufferPlaying());
}

bool CSound::IsDuplicatedBufferPlaying(){
	if(!m_ppDuplicatedBuffer) return false;

	DWORD dwStatus;
	for(int i=0; i<m_nDuplicateLimit; i++){
		if(m_ppDuplicatedBuffer[i]){
			m_ppDuplicatedBuffer[i]->GetStatus( &dwStatus );
			if(!(dwStatus & DSBSTATUS_PLAYING))	return true;
		}
	}
	return false;
}

int CSound::Reset(){
	if(!m_pSecondaryBuffer) return CS_E_NULL_SECONDARY;

	//if(m_isStreamFile){
	//	this->SetStreamCurosr(0);
	//}

	//m_pSecondaryBuffer->Stop();
	//m_pSecondaryBuffer->SetCurrentPosition(0);
	return CS_E_OK;
}

//int CSound::SetStreamCurosr(DWORD dwResumePosMs){
//	if(!m_pSecondaryBuffer) return CS_E_NULL_SECONDARY;
//	//m_dwPlayProgress = dwResumePosMs;
//	PostThreadMessage(m_dwThreadId, CSL_MSG_SEEK_AND_PLAY, dwResumePosMs, 0);
//
//	//m_pSecondaryBuffer->Stop();
//	//m_pSecondaryBuffer->SetCurrentPosition(0);
//
//	//m_Loader->SetWavePointerPos(dwResumePosMs, SEEK_SET);
//	//void* pdata = NULL;
//	//	m_Loader->GetDecodedData(&pdata, 0, m_dwOneSplittedBufferSize*m_dwNotificationNum, m_isLoop);
//	//	if(!WriteDataToBuffer(&m_pSecondaryBuffer, pdata, 0, m_dwOneSplittedBufferSize * m_dwNotificationNum))	return CS_E_NOMEM;
//	//SAFE_GLOBALFREE(pdata);
//
//	//this->CloseThread();
//	return CS_E_OK;
//}

//
//Stream
//
#if ENABLE_SOUND_POLLING
//void CSound::refillBuffer()
//{
//
//}
#endif
DWORD WINAPI CSound::StreamThread(LPVOID CSSPtr)
{
	CSound* pss = (CSound*)CSSPtr;

  //! PostThreadMessageが失敗しないようにメッセージキューを作成する必要がある see MSDN::PostThreadMessage
	{
		ResetEvent(pss->m_hThreadMessageDispatchEvent);
			MSG createQueueMsg;
			PeekMessage(&createQueueMsg, NULL, 0, 0, PM_NOREMOVE);
		SetEvent(pss->m_hThreadMessageDispatchEvent);//メッセージキュー作成完了
	}

#if !ENABLE_SOUND_POLLING
	DWORD dwNextWriteOffset = 0;
	DWORD dwBeginOffset       = 0;
	DWORD dwTotalWriteOffset  = 0;
	long dwLastNotificationPos = -1;

	while(TRUE){
		DWORD dwSignalPos = MsgWaitForMultipleObjects(1, &(HANDLE)(pss->m_pNotifyHandle), FALSE, INFINITE, QS_ALLEVENTS);
		if(dwSignalPos == WAIT_OBJECT_0 + 0){		//シグナルになった位置を算出
			DWORD  dwCurrentPlayPos;
			pss->m_pSecondaryBuffer->GetCurrentPosition(&dwCurrentPlayPos, NULL);

			void* pdata = NULL;
				DWORD dwCurrentDecodePos = pss->m_Loader->GetCurrentDecodedPos();

				int retdec = pss->m_Loader->GetDecodedData(&pdata, dwCurrentDecodePos, pss->m_dwOneSplittedBufferSize, pss->m_isLoop);
				if(retdec == CSL_N_FIN){
					OutputDebugStringFormatted("%s:found end of stream.\n", __FUNCTION__);
				}else if(retdec == CSL_E_UNEXP){
					continue;
				}
				if(!(pss->WriteDataToBuffer(&(pss->m_pSecondaryBuffer), pdata, dwNextWriteOffset, pss->m_dwOneSplittedBufferSize))){
					SAFE_GLOBALFREE(pdata);
					continue;
				}
			SAFE_GLOBALFREE(pdata);

			dwNextWriteOffset += pss->m_dwOneSplittedBufferSize;
			dwNextWriteOffset %= (pss->m_dwOneSplittedBufferSize * pss->m_dwNotificationNum);

			dwTotalWriteOffset+= pss->m_dwOneSplittedBufferSize;

			dwLastNotificationPos = (dwLastNotificationPos+1)%pss->m_dwNotificationNum;
		}else if(WAIT_OBJECT_0 + 1){
			MSG msg;
			while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
        switch( msg.message ){
					case WM_QUIT:
					{
						pss->m_pSecondaryBuffer->Stop();
						return 0;
					}break;
					case CSL_MSG_SEEK_AND_PLAY:
					case CSL_MSG_SEEK:
					{
						ResetEvent(pss->m_hThreadMessageDispatchEvent);

						pss->m_pSecondaryBuffer->Stop();
						pss->m_pSecondaryBuffer->SetCurrentPosition(0);
						dwNextWriteOffset		= 0;
						dwBeginOffset				= 0;
						dwTotalWriteOffset  = 0;
						dwLastNotificationPos= -1;

						//アライメントをチェックする
						DWORD dwSizeToRead = (DWORD)(msg.wParam/1000.0 * (double)pss->m_wfx.nAvgBytesPerSec);

						if(dwSizeToRead % pss->m_wfx.nBlockAlign){
							if((dwSizeToRead % pss->m_wfx.nBlockAlign) <= (DWORD)(pss->m_wfx.nBlockAlign / 2)){
								dwSizeToRead-=(dwSizeToRead % pss->m_wfx.nBlockAlign);
							}else{
								dwSizeToRead+=(pss->m_wfx.nBlockAlign - (dwSizeToRead % pss->m_wfx.nBlockAlign));
							}
						}

						pss->m_Loader->SetWavePointerPos(dwSizeToRead);

						void* pdata = NULL;
							int retdec = pss->m_Loader->GetDecodedData(&pdata, CSL_CONTINUE_CURSOR, pss->m_dwOneSplittedBufferSize, pss->m_isLoop);
							if(retdec == CSL_E_UNEXP){
								/**/
							}else{
								if(!pss->WriteDataToBuffer(&pss->m_pSecondaryBuffer, pdata, 0, pss->m_dwOneSplittedBufferSize)){
									SAFE_GLOBALFREE(pdata);
								}
								dwNextWriteOffset += pss->m_dwOneSplittedBufferSize;
								dwTotalWriteOffset+= pss->m_dwOneSplittedBufferSize;
								dwBeginOffset			 = dwSizeToRead;
								//dwLastNotificationPos=0;	//ここは通知イベント処理部分ではないため、更新しない
							}
						SAFE_GLOBALFREE(pdata);

						if( msg.message==CSL_MSG_SEEK_AND_PLAY ){
							pss->m_pSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
						}

						SetEvent(pss->m_hThreadMessageDispatchEvent);
					}break;
					case CSL_MSG_GETCURPOS_BYTE:
					{
						ResetEvent(pss->m_hThreadMessageDispatchEvent);
						DWORD dwCursorByte;
						pss->m_pSecondaryBuffer->GetCurrentPosition( &dwCursorByte, NULL );

						// まだバッファに書き込み終わっていないのに位置を取得されたら
						if( dwLastNotificationPos==-1 ){
							*((DWORD*)msg.lParam) = dwBeginOffset/* + dwTotalWriteOffset*/;// ステップ実行時にバグ
						}else{
							DWORD dwCursorInCursor;
							DWORD dwPlayingTotalOffset;
							if( dwLastNotificationPos==0 && dwCursorByte>=pss->m_pDsbnotify[pss->m_dwNotificationNum-1].dwOffset-1 ){// バッファがループしたが、再生カーソルがまだ最初の通知位置まで届いてない(最後の区間を再生中)
								dwCursorInCursor = dwCursorByte-pss->m_pDsbnotify[pss->m_dwNotificationNum-1].dwOffset;
								dwPlayingTotalOffset = (dwTotalWriteOffset-pss->m_dwOneSplittedBufferSize*2);
							}else if( dwCursorByte>=pss->m_pDsbnotify[dwLastNotificationPos].dwOffset-1 ){// 通常
								dwCursorInCursor = dwCursorByte-pss->m_pDsbnotify[dwLastNotificationPos].dwOffset;
								dwPlayingTotalOffset = (dwTotalWriteOffset-pss->m_dwOneSplittedBufferSize);
							}else{ // まだ再生カーソルが直前の通知バッファ位置に届いていない
								dwCursorInCursor = dwCursorByte-pss->m_pDsbnotify[dwLastNotificationPos-1].dwOffset;
								dwPlayingTotalOffset = (dwTotalWriteOffset-pss->m_dwOneSplittedBufferSize*2);
							}
							*((DWORD*)msg.lParam) = dwBeginOffset + dwPlayingTotalOffset + dwCursorInCursor;
							//OutputDebugStringFormatted("3:%lu : %lu : %lu : %lu : %ld\n", (DWORD)(*((DWORD*)msg.lParam) / ((double)pss->m_wfx.nAvgBytesPerSec/1000.0)), dwCursorByte, dwPlayingTotalOffset, dwCursorInCursor, dwLastNotificationPos );
						}
						SetEvent(pss->m_hThreadMessageDispatchEvent);
					}break;
					case CSL_MSG_PAUSE:
					case CSL_MSG_STOP:
					{
						ResetEvent(pss->m_hThreadMessageDispatchEvent);
						pss->m_pSecondaryBuffer->Stop();

						if( CSL_MSG_STOP ){
							pss->m_pSecondaryBuffer->SetCurrentPosition(0);

							dwNextWriteOffset		= 0;
							dwBeginOffset				= 0;
							dwTotalWriteOffset  = 0;
							dwLastNotificationPos= -1;
						}

						SetEvent(pss->m_hThreadMessageDispatchEvent);
					}break;
				}
			}
		}
	}
#else
	DWORD dwNextWriteOffset = 0;
	bool isPlaying = false;
	DWORD dwNopFrame        = 0;

	while(TRUE){
		MSG msg;
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
      switch( msg.message ){
				case WM_QUIT:
				{
					pss->m_pSecondaryBuffer->Stop();
					return 0;
				}break;
				case CSL_MSG_SEEK_AND_PLAY:
				case CSL_MSG_SEEK:
				{
					ResetEvent(pss->m_hThreadMessageDispatchEvent);

					pss->m_pSecondaryBuffer->Stop();
					pss->m_pSecondaryBuffer->SetCurrentPosition(0);
					dwNextWriteOffset		= 0;

					//アライメントをチェックする
					DWORD dwSizeToRead = (DWORD)(msg.wParam/1000.0 * (double)pss->m_wfx.nAvgBytesPerSec);

					if(dwSizeToRead % pss->m_wfx.nBlockAlign){
						if((dwSizeToRead % pss->m_wfx.nBlockAlign) <= (DWORD)(pss->m_wfx.nBlockAlign / 2)){
							dwSizeToRead-=(dwSizeToRead % pss->m_wfx.nBlockAlign);
						}else{
							dwSizeToRead+=(pss->m_wfx.nBlockAlign - (dwSizeToRead % pss->m_wfx.nBlockAlign));
						}
					}

					if( CSL_E_OK==pss->m_Loader->SetWavePointerPos(dwSizeToRead) ){
						void* pdata = NULL;
							int retdec = pss->m_Loader->GetDecodedData(&pdata, CSL_CONTINUE_CURSOR, pss->m_dwOneSplittedBufferSize, pss->m_isLoop);
							if(retdec == CSL_E_UNEXP){
								/**/
							}else{
								if(!pss->WriteDataToBuffer(&pss->m_pSecondaryBuffer, pdata, 0, pss->m_dwOneSplittedBufferSize)){
									SAFE_GLOBALFREE(pdata);
								}
								dwNextWriteOffset += pss->m_dwOneSplittedBufferSize;
							}
						SAFE_GLOBALFREE(pdata);
					}else{
						void* pdata = NULL;
							pdata = GlobalAlloc(GPTR, pss->m_dwOneSplittedBufferSize);
							opt_memset(pdata, 0x00, pss->m_dwOneSplittedBufferSize);
								if(!pss->WriteDataToBuffer(&pss->m_pSecondaryBuffer, pdata, 0, pss->m_dwOneSplittedBufferSize)){
									SAFE_GLOBALFREE(pdata);
								}
								dwNextWriteOffset += pss->m_dwOneSplittedBufferSize;
						SAFE_GLOBALFREE(pdata);
					}

					if( msg.message==CSL_MSG_SEEK_AND_PLAY ){
						pss->m_pSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
						isPlaying = true;
					}

					SetEvent(pss->m_hThreadMessageDispatchEvent);
				}break;
				case CSL_MSG_GETCURPOS_BYTE:
				{
					ResetEvent(pss->m_hThreadMessageDispatchEvent);
					CORE_ASSERT(0, "should not reach here.");
					SetEvent(pss->m_hThreadMessageDispatchEvent);
				}break;
				case CSL_MSG_PAUSE:
				case CSL_MSG_STOP:
				{
					ResetEvent(pss->m_hThreadMessageDispatchEvent);
					pss->m_pSecondaryBuffer->Stop();

					if( CSL_MSG_STOP ){
						pss->m_pSecondaryBuffer->SetCurrentPosition(0);

						dwNextWriteOffset		= 0;
						isPlaying = false;
					}

					SetEvent(pss->m_hThreadMessageDispatchEvent);
				}break;
			}
		}else{
			// 現在の再生カーソル+パディングが次のブロックに入っているなら、そのブロックに書き込みをする
			DWORD dwPlayCursor;
			if( isPlaying ){
				pss->m_pSecondaryBuffer->GetCurrentPosition(&dwPlayCursor, NULL);

				DWORD dwSecondaryBufferSize = pss->m_dwOneSplittedBufferSize*pss->m_dwNotificationNum;
				DWORD dwWriteMargin = pss->m_dwOneSplittedBufferSize/2;//適当に決めた値
				DWORD dwNextWillBe = (dwPlayCursor+dwWriteMargin) % dwSecondaryBufferSize;
				if( dwNextWillBe>=dwNextWriteOffset ){// 次のブロックにさしかかるよ
					if( dwNextWriteOffset==0 && dwNextWillBe>=dwSecondaryBufferSize-(pss->m_dwOneSplittedBufferSize*(pss->m_dwNotificationNum/2)) ){// 半分より後ろにいるなら、まだ再生バッファは周回していないと仮定
						/* NOP */
					}else{
						//OutputDebugStringFormatted("%lu:%lu:%lu\r\n", dwNextWillBe, dwNextWriteOffset, dwNopFrame);
						dwNopFrame = 0;

						// データのデコード
						void* pdata = NULL;
							int retdec = pss->m_Loader->GetDecodedData(&pdata, /*pss->m_Loader->GetCurrentDecodedPos()*/CSL_CONTINUE_CURSOR, pss->m_dwOneSplittedBufferSize, pss->m_isLoop);
							if( retdec!=CSL_N_FIN ){
								// データの書き込み
								if(!pss->WriteDataToBuffer(&pss->m_pSecondaryBuffer, pdata, dwNextWriteOffset, pss->m_dwOneSplittedBufferSize)){
									SAFE_GLOBALFREE(pdata);
								}
								// ポインタ更新
								dwNextWriteOffset += pss->m_dwOneSplittedBufferSize;
								dwNextWriteOffset %= dwSecondaryBufferSize;
							}else{
								pss->m_pSecondaryBuffer->Stop();
								pss->m_pSecondaryBuffer->SetCurrentPosition(0);
								dwNextWriteOffset		= 0;
								isPlaying = false;
							}
						SAFE_GLOBALFREE(pdata);
					}
				}else{
					++dwNopFrame;
				}
			}
			Sleep(5);// このスレッドは適当に休ませる
		}
	}
#endif
	return 0;
}

void CSound::AllowRapidAccess(bool isAllow){
	m_isAllowRapidAccess = isAllow;
}

DWORD CSound::GetDuration(){
	if( m_Loader ){
		return m_Loader->GetDecodedLengthMs();
	}
	return 0;
}