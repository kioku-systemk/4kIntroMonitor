/*
	4K Intro Monitor v0.1
	coded by  kioku 2011
*/

#include <windows.h>

#include "CSound.h"
#include "Util.h"
#include "Logger.h"
#include "ShaderCompactor.h"

#include "Driver.h"

//#include <sys/stat.h>
//#include <time.h>


#define WIN_WIDTH   1920*0.5
#define WIN_HEIGHT  1080*0.5
//--------------------------------------------------------------------

#define TITLEBAR_TEXT		"4K Intro Studio (System K @ 2011)"

//--------------------------------------------------------------------------------
//#include "4kGfxMon_driver.h"

static driverPure* gDriver = NULL;//!< アクティブなドライバインスタンス

//! @brief アクティブなドライバがサポートするシェーダーのコンパクション
//! @param char** new_shader : コンパクション後のシェーダーコード。driver::SHADER_NUM数の要素が必要。呼び出し側がtfree()する。
//! @param unsigned long* shadershze : コンパクション後のコードサイズ。driver::SHADER_NUM数の要素が必要。
void wrapCompactionSrc(char** new_shader, unsigned long* shadersize);

//--------------------------------------------------------------------------------

int screenwidth = 1920;
int screenheight = 1080;

float g_tm = 0.0f;
bool g_animMode = false;

float g_starttm = 0;
HWND g_hWnd = 0;
FILETIME g_shaderfiletime = {0};

CSound* g_sound = 0;

enum AspectMode{
	ASPECTMODE_4_3,
	ASPECTMODE_16_9,
	ASPECTMODE_16_10,
	ASPECTMODE_NUM
};
struct {
	unsigned int menuID;
	char* menuString;
	float ratio;
} aspectStruct[ASPECTMODE_NUM] = 
{
//	100, "Always On top"
	{101, "4:3"  , 4.0f / 3.0f},
	{102, "16:9" , 16.0f/ 9.0f},
	{103, "16:10", 16.0f/10.0f}
};
static AspectMode aspectMode = ASPECTMODE_16_9;



void Export2Src()
{
	bool r = false;
	// TODO:
	/*
	// compaction
	char *new_shader[driver::SHADER_NUM];
	unsigned long shadersize[driver::SHADER_NUM];
	wrapCompactionSrc(&new_shader[0], &shadersize[0]);

	// ソースファイルに出力
	char templateName[1024];
	wsprintf(templateName, "template_%s.txt", driver::getDriverTypeName(driver::getCurrentDriverType()));
	bool r = output_src((const char**)&new_shader[0], templateName);
	
	for(int i=0; i<driver::SHADER_NUM; ++i){
		tfree(new_shader[i]);
	}
	*/
	Logger::GetInstance().Clear();
	if (r)
		Logger::GetInstance().OutputString("Export...OK!\n\n>> template.cpp\n\n");
	else
		Logger::GetInstance().OutputString("Export Error!!\n\nCheck template.txt or so...\n\n");
	
}


// シェーダーファイルのビルド
bool updateShader(bool force = false)
{
	char* shaderSource = 0;
	bool updated = false;
	// 更新日付の評価とシェーダーコードの読み込み
	HANDLE fileShader = OpenFile( gDriver->getShaderName() );
	if (fileShader == INVALID_HANDLE_VALUE)
	{
		Logger::GetInstance().OutputString("Can't open Shader file : %s\n", gDriver->getShaderName());
		return false;
	}
	
	if (NeedUpdateFileTimeStamp(fileShader, g_shaderfiletime) || force)
	{
		updated = true;
		unsigned long fsize = 0;
		ReadFileToBuffer(fileShader, shaderSource, fsize);
	}
	CloseFile(fileShader);
	
	// ログのクリア・シェーダーコンパイル＆リンク
	if (updated)
	{
		shaderPure* shader = 0;
		Logger::GetInstance().Clear();
		if( shaderSource )
		{
			Logger::GetInstance().OutputString("Shader file updated: %s\n", gDriver->getShaderName());
			shader = gDriver->compileShader(shaderSource);
			tfree(shaderSource);
		}
		if (!shader->valid())
		{
			Logger::GetInstance().OutputString("Failed shader compile.");
			return false;	
		}

		gDriver->setShader(shader);

		Logger::GetInstance().OutputString("UpdateTime = ");
		char buf[512];
		GetNowTimeString(buf);
		Logger::GetInstance().OutputString(buf);
		Logger::GetInstance().OutputString("\n\n");
	}

	return true;
}

bool LoadShaders()
{
	bool r = updateShader();
	//gDriver==NULL ? false : gDriver->updateShader();
	//r |= gDriver==NULL ? false : gDriver->updateExtShader();
	return r;
}

void Draw(HWND hWnd)
{
	if( !gDriver ){
		return ;
	}

	char buf[512];
	wsprintf(buf, TITLEBAR_TEXT" : [Time = %dms]", kftol(g_tm*1000));
	SetWindowText(hWnd, buf);
	gDriver->setTime(g_tm);
	gDriver->render();
	//gDriver->ExtDrive();
	gDriver->swapbuffer();
}

void startAnim()
{
	g_animMode = true;
	g_starttm = timeGetTime() / static_cast<float>(1000.0f) - g_tm;
	if (g_sound)
	{
		g_sound->Play(static_cast<unsigned int>(g_tm*1000));
	}
}

void endAnim()
{
	g_animMode = false;
	if (g_sound)
		g_sound->Stop();
}

void idle()
{
	g_tm = timeGetTime() / static_cast<float>(1000.0f) - g_starttm;
	Draw(g_hWnd);
}
//---------------------------------------------------------------
//static DEVMODE dmScreenSettings = {
//	0,0,0,sizeof(DEVMODE),0,DM_PELSWIDTH|DM_PELSHEIGHT,
//	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1024,768,0,0,0,0,0,0,0,0,0,0
//};

void CheckSize(WPARAM wParam,LPARAM lParam)
{
	RECT    *rc;
	int     width,height;
	//int     minimumwidth;
	//float RX = 4;
	//float RY = 3;
	float RXRY = aspectStruct[aspectMode].ratio;
	float RYRX = 1.0f / aspectStruct[aspectMode].ratio;
	
	//int widthtop, widthbottom;
	//minimumwidth = GetSystemMetrics(SM_CXMINTRACK);
	rc = (RECT *)lParam;
	width=rc->right-rc->left;
	height=rc->bottom-rc->top;
	switch(wParam){
		case    WMSZ_TOP:
		case    WMSZ_BOTTOM:
			rc->right=rc->left+kftol(height*(RXRY));
		break;
		case    WMSZ_LEFT:
		case    WMSZ_RIGHT:
			rc->bottom=rc->top+kftol(width*(RYRX));
		break;
		case    WMSZ_TOPLEFT:
		if(((double)width/(double)height)<(RXRY)){
			rc->left=rc->right-kftol(height*(RXRY));
		}else{
			rc->top=rc->bottom-kftol(width*(RYRX));
		}
		break;
		case    WMSZ_TOPRIGHT:
		if(((double)width/(double)height)<(RXRY)){
			rc->right=rc->left+kftol(height*(RXRY));
		}else{
			rc->top=rc->bottom-kftol(width*(RYRX));
		}
		break;
		case    WMSZ_BOTTOMLEFT:
		if(((double)width/(double)height)<(RXRY)){
			rc->left=rc->right-kftol(height*(RXRY));
		}else{
			rc->bottom=rc->top+kftol(width*(RYRX));
		}
		break;
		case    WMSZ_BOTTOMRIGHT:
		if(((double)width/(double)height)<(RXRY)){
			rc->right=rc->left+kftol(height*(RXRY));
		}else{
			rc->bottom=rc->top+kftol(width*(RYRX));
		}
		break;
	}
}

void Benchmark(HWND hWnd)
{
	// Performance Analysis
	unsigned long stm = timeGetTime();
	for (int i = 0; i < 10; i++)
		Draw(hWnd);
	unsigned long dtm = timeGetTime() - stm;
	char buf[1024];
	wsprintf(buf, "%s - RenderTime = %ld[ms]", TITLEBAR_TEXT, dtm/10);
	SetWindowText(hWnd, buf);
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	unsigned char VKey;
	HMENU menu;
	unsigned int flag = 0;
	static bool m_lbutton = false;
	bool isTopmost = false;
	long exstyle = GetWindowLong(hWnd, GWL_EXSTYLE);
	if (exstyle & WS_EX_TOPMOST)
		isTopmost = true;
			
	switch (message)
	{
		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			Draw(hWnd);
		break;
	
		case WM_TIMER:
			if (LoadShaders())
				Draw(hWnd);
			//KillTimer(hWnd, 500);
			
		break;
		case WM_KEYUP:
			VKey = (unsigned char)wParam;
			if (VKey == VK_ESCAPE)
				ExitProcess(0);
			else if (VKey == VK_F5)
			{
				Benchmark(hWnd);
			}
			else if (VKey == VK_RETURN)
			{
				Export2Src();
				Draw(hWnd);
			}
			else if (VKey == VK_SPACE)
			{
				if (g_animMode)
					endAnim();
				else
					startAnim();
			}
			else if (VKey == 'A')
			{
				g_tm = 0;
				Draw(hWnd);
			}
		
		break;
	
		case WM_DESTROY:
			ExitProcess(0);
		break;
	
		case WM_SIZE:
			gDriver->resize(LOWORD(lParam), HIWORD(lParam));
			screenwidth = LOWORD(lParam);
			screenheight = HIWORD(lParam);
			Draw(hWnd);
		break;
		case WM_SIZING:
			CheckSize(wParam,lParam);
			return TRUE;
		break;
	
		case WM_LBUTTONDOWN:
			m_lbutton = true;
		case WM_MOUSEMOVE:
			if (m_lbutton)
			{
				POINT point;
				point.x = LOWORD(lParam);
				point.y = HIWORD(lParam);
				
				float allrate = 1.0;
				if (g_sound)
				{
					allrate = g_sound->GetDuration() / 1000.0f;
				}
				g_tm = allrate * point.x / static_cast<float>(screenwidth);
	//			gDriver->SetTime(g_tm);
				Draw(hWnd);
			}
		break;
		case WM_LBUTTONUP:
			m_lbutton = false;
		break;

		case WM_RBUTTONUP:
			menu = CreatePopupMenu();
			flag = MF_STRING|MF_ENABLED;
			if (isTopmost)
				flag |= MF_CHECKED;
			AppendMenu(menu, flag, 100, "Always On Top");
			AppendMenu(menu, MF_SEPARATOR, 0, 0);
			for (int i = 0; i < ASPECTMODE_NUM; i++)
			{
				flag = MF_STRING|MF_ENABLED;
				if (aspectMode == i)
					flag |= MF_CHECKED;
				AppendMenu(menu, flag, aspectStruct[i].menuID, aspectStruct[i].menuString);
			}
			AppendMenu(menu, MF_SEPARATOR , 0, 0);
			AppendMenu(menu, MF_ENABLED, 1100, "calc RenderTime...\t[F5]");
			AppendMenu(menu, MF_SEPARATOR , 0, 0);
			AppendMenu(menu, MF_DISABLED, 1510, "Use Shader Minifier");// TODO
			AppendMenu(menu, MF_ENABLED, 1000, "Export...\t[SPACE]");
			AppendMenu(menu, MF_SEPARATOR , 0, 0);
			{
				HMENU driverMenu = CreateMenu();
				AppendMenu(menu, MF_POPUP, reinterpret_cast< UINT >(driverMenu), "Change driver...");

				AppendMenu(driverMenu, gDriver->getDriverType()==driverPure::TYPE_OGL ? MF_ENABLED|MF_CHECKED : MF_ENABLED, 1501, "OpenGL");
				AppendMenu(driverMenu, gDriver->getDriverType()==driverPure::TYPE_D3D9 ? MF_ENABLED|MF_CHECKED : MF_ENABLED, 1502, "Direct3D 9");
				AppendMenu(driverMenu, gDriver->getDriverType()==driverPure::TYPE_D3D11 ? MF_ENABLED|MF_CHECKED : MF_ENABLED, 1504, "Direct3D 11");	
			}
			
			
			POINT point;
			point.x = LOWORD(lParam);
			point.y = HIWORD(lParam);
			ClientToScreen(hWnd, &point);
			TrackPopupMenu(menu,
				TPM_LEFTALIGN  |	//クリック時のX座標をメニューの左辺にする
				TPM_RIGHTBUTTON,	//右クリックでメニュー選択可能とする
				point.x, point.y,	//メニューの表示位置
				0,
				hWnd,            	//このメニューを所有するウィンドウ
				NULL
			);
			DestroyMenu(menu);
		break;
		
		case WM_COMMAND:
		{
			// Always on top
			if (LOWORD(wParam) == 100)
			{
				if (isTopmost)
					SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0,0, SWP_NOSIZE | SWP_NOMOVE);
				else
					SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0,0, SWP_NOSIZE | SWP_NOMOVE);
				break;
			}
			
			// Export
			if (LOWORD(wParam) == 1000)
			{
				Export2Src();
				Draw(hWnd);
				break;
			}
			
			// Bench
			if (LOWORD(wParam) == 1100)
			{
				Benchmark(hWnd);
				break;
			}
			
			for (int i = 0; i < ASPECTMODE_NUM; i++)
			{
				if (LOWORD(wParam) == aspectStruct[i].menuID)
				{
					aspectMode = (AspectMode)i;
					break;
				}
			}

			// Change driver
			driverPure* newDriver = 0;
			switch( LOWORD(wParam) ){
			case 1501:// OpenGL
				newDriver = static_cast<driverPure*>(new driverOGL());
				break;
			case 1502:// Direct3D 9
				newDriver = static_cast<driverPure*>(new driverD3D9());	
				break;
			case 1504:// Direct3D 11
				newDriver = static_cast<driverPure*>(new driverD3D11());
				break;
			}

			if (newDriver)
			{
				if (gDriver)
				{
					gDriver->uninstall();
					delete gDriver;
				}
				gDriver = newDriver;
				gDriver->install(static_cast<void*>(hWnd));
				gDriver->resize(static_cast<int>(screenwidth), static_cast<int>(screenheight));
				
				Logger::GetInstance().OutputString("Ready New Driver:%s\n", gDriver->getDriverName());
				Draw(hWnd);
				updateShader(true);
			}

        }
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return FALSE;
}

void entrypoint()
{
	//slic::initialize();
	
//#ifndef _DEBUG
//	ChangeDisplaySettings(&dmScreenSettings,CDS_FULLSCREEN);
//	const HDC hDC = GetDC(CreateWindow("edit",0,WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0,
//	0, 0, 0, 0, 0, 0, 0));
//#else
	static const char* wndclassname = "4KINTROCLASS";
	WNDCLASSEX wcex;
	if(GetClassInfo((HINSTANCE)GetModuleHandle(NULL),wndclassname,(WNDCLASS*)&wcex) == FALSE)
	{
		wcex.cbSize			= sizeof(WNDCLASSEX); 
		wcex.style			= CS_OWNDC;
		wcex.lpfnWndProc	= (WNDPROC)WndProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= (HINSTANCE)GetModuleHandle(NULL);
		wcex.hIcon			= NULL;
		wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
		wcex.lpszMenuName	= NULL;
		wcex.lpszClassName	= wndclassname;
		wcex.hIconSm		= NULL;
		RegisterClassEx(&wcex);
	}
	
	// SetCurrentDir
	char filename[1024];
	GetModuleFileName((HINSTANCE)GetModuleHandle(NULL), filename, 1024);
	unsigned long p = lstrlen(filename);
	while(filename[p] != '\\')
		p--;
	filename[p] = '\0';
	SetCurrentDirectory(filename);
	
	HWND hWnd = CreateWindow(wndclassname, TITLEBAR_TEXT,WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX,
					100, 100,
					static_cast<int>(WIN_WIDTH), static_cast<int>(WIN_HEIGHT), 0, 0, 0, 0);
	g_hWnd = hWnd;
	
	RECT rect;
	GetClientRect(hWnd, &rect);
	screenwidth  = rect.right;
	screenheight = rect.bottom;

	const HDC hDC = GetDC(hWnd);
//#endif

	g_sound = new CSound();
	g_sound->Initialize(hWnd);
	int r = g_sound->Load("sound.wav");
	if (r != 1)
	{
		delete g_sound;
		g_sound = 0;
	}

	gDriver = static_cast<driverPure*>(new driverOGL());
	gDriver->install(hWnd);
	gDriver->resize(screenwidth, screenheight);
	
	SetTimer(hWnd,1, 500, NULL); 
	
	while(1)
	{
		MSG msg;
		if (!g_animMode || PeekMessage (&msg,NULL,0,0,PM_NOREMOVE))
		{
			if (GetMessage(&msg, NULL, 0, 0))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			idle();
		}
	}
	
	
	/*while(true)
	{
		
		if(PeekMessage(&msg,NULL,0,0,PM_REMOVE))
		{
			if(msg.message==WM_QUIT)
				break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}*/

	//slic::finalize();
}

//#ifdef _DEBUG

int WINAPI WinMain(HINSTANCE , HINSTANCE, char* , int)
{
	entrypoint();
}

//#endif
