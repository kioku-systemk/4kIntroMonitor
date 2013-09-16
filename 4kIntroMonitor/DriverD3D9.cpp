#include "DriverD3D9.h"

#include "Util.h"
#include "ShaderCompactor.h"

#include "Logger.h"
#include "lcfont_bw112x66.h"

#include <d3d9.h>
#include <d3dx9.h>

#define SAFE_RELEASE(A) {if(A){A->Release();A=0;}}

namespace {

class BitmapFontD3D9
{
	private:
		//unsigned int texName;
		BitmapFontD3D9(){};
		IDirect3DTexture9* pFont;
		IDirect3DDevice9* m_pDev;
	public:
		static BitmapFontD3D9& GetInstance()
		{
			static BitmapFontD3D9* singleton = NULL;
			if (singleton == NULL)
				singleton = new BitmapFontD3D9();
			return *singleton;
		}
		void CreateTexture(IDirect3DDevice9* pDev)
		{
			m_pDev = pDev;
			D3DXCreateTexture(pDev, 112, 66, 0, 0, D3DFMT_L8, D3DPOOL_MANAGED, &pFont);
			if( !pFont ){
				return ;
			}

			D3DLOCKED_RECT rect;
			if( SUCCEEDED(pFont->LockRect(0, &rect, NULL, 0)) ){
				const unsigned char* ps = lcfont_bw112x66;
				unsigned char* pd = static_cast< unsigned char* >(rect.pBits);
				const DWORD pitch = rect.Pitch;
				for(int y=0; y<66; ++y){
					for(int x=0; x<112; ++x){
						pd[x] = ps[x];
					}
					pd += pitch;
					ps += 112;
				}

				pFont->UnlockRect(0);
			}
		}
		void DeleteTexture()
		{
			SAFE_RELEASE(pFont);
		}

		void DrawString(const char* strbuf, float aspect, float width)
		{
			m_pDev->SetTexture(0, pFont);

			DWORD dwMagFilter, dwMinFilter, dwMipFilter;
			m_pDev->GetSamplerState(0, D3DSAMP_MAGFILTER, &dwMagFilter);
			m_pDev->GetSamplerState(0, D3DSAMP_MINFILTER, &dwMinFilter);
			m_pDev->GetSamplerState(0, D3DSAMP_MIPFILTER, &dwMipFilter);

			m_pDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
			m_pDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			//mpDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

			DWORD dwBlendOp, dwSrcBlend, dwDstBlend, dwZEnable, dwAlphaBlendEnable;
			m_pDev->GetRenderState(D3DRS_BLENDOP, &dwBlendOp);
			m_pDev->GetRenderState(D3DRS_SRCBLEND, &dwSrcBlend);
			m_pDev->GetRenderState(D3DRS_DESTBLEND, &dwDstBlend);
			m_pDev->GetRenderState(D3DRS_ZENABLE, &dwZEnable);
			m_pDev->GetRenderState(D3DRS_ALPHABLENDENABLE, &dwAlphaBlendEnable);

			m_pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			m_pDev->SetRenderState(D3DRS_ZENABLE, FALSE);
			m_pDev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			m_pDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_INVDESTCOLOR);
			m_pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);

			float fontWidth = 7;
			float fontHeight = 11;
			float ddx = 0;
			float ddy = 0;

			unsigned int strend = lstrlen(strbuf);
			for(unsigned int strptr = 0; strptr < strend; strptr++)
			{
				if (strbuf[strptr] >= 0x20 && strbuf[strptr] < 0x80)
				{
					unsigned int lowbit = (strbuf[strptr] - 0x20) & 0x0F;
					unsigned int highbit = ((strbuf[strptr] - 0x20) >> 4) & 0x0F;
					float ptx = lowbit / 16.0f;
					float pty = 1.0f - highbit / 6.0f;
					float dtx = 1.0f / 16.0f;
					float dty = 1.0f /  6.0f;
			
					struct VERTEX{
						float x,y,z;
						D3DCOLOR color;
						float u,v;
					};

					const VERTEX vertex[] = {
						{ ddx, ddy, 0.f,  0xffffffff, ptx, /*1.f -*/ pty },
						{ ddx+fontWidth, ddy, 0.f, 0xffffffff, ptx + dtx, /*1.f -*/ pty },
						{ ddx+fontWidth, ddy - fontHeight, 0.f, 0xffffffff, ptx + dtx, /*1.f -*/ (pty - dty) },

						{ ddx, ddy, 0.f,  0xffffffff, ptx, /*1.f -*/ pty },
						{ ddx+fontWidth, ddy - fontHeight, 0.f, 0xffffffff, ptx + dtx, /*1.f -*/ (pty - dty) },
						{ ddx, ddy - fontHeight, 0.f,  0xffffffff, ptx, /*1.f - */(pty - dty) },

						// works fine. stretched
						{ -1.f, 1.f, 0.f,  0xffffffff, ptx, /*1.f -*/ pty },
						{ 1.f, 1.f, 0.f, 0xffffffff, ptx + dtx, /*1.f -*/ pty },
						{ 1.f, -1.f, 0.f, 0xffffffff, ptx + dtx, /*1.f -*/ (pty - dty) },

						{ -1.f, 1.f, 0.f,  0xffffff00, ptx, /*1.f - */pty },
						{ 1.f, -1.f, 0.f, 0xffffff00, ptx + dtx, /*1.f - */(pty - dty) },
						{ -1.f, -1.f, 0.f,  0xffffff00, ptx, /*1.f - */(pty - dty) },
					};

					m_pDev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
					m_pDev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertex, sizeof(VERTEX));
				}
				
				ddx += fontWidth;
				if (strbuf[strptr] == 0xA || strbuf[strptr] == 0xC)
				{
					ddx = 0;
					ddy -= fontHeight;
				}
			}

			m_pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, dwAlphaBlendEnable);
			m_pDev->SetRenderState(D3DRS_ZENABLE, dwZEnable);
			m_pDev->SetRenderState(D3DRS_BLENDOP, dwBlendOp);
			m_pDev->SetRenderState(D3DRS_SRCBLEND, dwSrcBlend);
			m_pDev->SetRenderState(D3DRS_DESTBLEND, dwDstBlend);

			m_pDev->SetSamplerState(0, D3DSAMP_MAGFILTER, dwMagFilter);
			m_pDev->SetSamplerState(0, D3DSAMP_MINFILTER, dwMinFilter);
			m_pDev->SetSamplerState(0, D3DSAMP_MIPFILTER, dwMipFilter);

			m_pDev->SetTexture(0, NULL);
		}
};


} // namespace

shaderD3D9::shaderD3D9(IDirect3DDevice9* dev, const char* shaderbuffer)
{
	m_vs = 0;
	m_ps = 0;
	m_pDev = dev;
	LPD3DXBUFFER pVSBuffer = NULL;
	LPD3DXBUFFER pPSBuffer = NULL;
	LPD3DXBUFFER pError = NULL;
	if( SUCCEEDED(D3DXCompileShader(shaderbuffer, lstrlen(shaderbuffer), 0, 0, "vs", "vs_3_0", 0, &pVSBuffer, &pError, 0))){
		m_pDev->CreateVertexShader(static_cast< const DWORD* >(pVSBuffer->GetBufferPointer()), &m_vs);
	}
	if( pError ){
		Logger::GetInstance().OutputString(static_cast< const char* >(pError->GetBufferPointer()));
		Logger::GetInstance().OutputString("Compile error in vertex shader.\n\n\n");
	}

	if( SUCCEEDED(D3DXCompileShader(shaderbuffer, lstrlen(shaderbuffer), 0, 0, "ps", "ps_3_0", 0, &pPSBuffer, &pError, 0))){
		m_pDev->CreatePixelShader(static_cast< const DWORD* >(pPSBuffer->GetBufferPointer()), &m_ps);
	}
	if( pError ){
		Logger::GetInstance().OutputString(static_cast< const char* >(pError->GetBufferPointer()));
		Logger::GetInstance().OutputString("Compile error in pixel shader.\n\n\n");
	}	
}
shaderD3D9::~shaderD3D9()
{
	SAFE_RELEASE(m_vs);
	SAFE_RELEASE(m_ps);
}

bool shaderD3D9::valid()
{
	if (m_vs && m_ps)
		return true;
	else
		return false;
}

void shaderD3D9::bind()
{
	m_pDev->SetPixelShader(static_cast< LPDIRECT3DPIXELSHADER9 >(m_ps));
	m_pDev->SetVertexShader(static_cast< LPDIRECT3DVERTEXSHADER9 >(m_vs));
}

void shaderD3D9::unbind()
{
	m_pDev->SetPixelShader(static_cast< LPDIRECT3DPIXELSHADER9 >(0));
	m_pDev->SetVertexShader(static_cast< LPDIRECT3DVERTEXSHADER9 >(0));
}

void shaderD3D9::setUniform(const char* name, float v)
{
	m_pDev->SetPixelShaderConstantF(0, &v, 1);
}


driverD3D9::driverD3D9()
{
	m_time = 0;
	m_shader = 0;
}

driverD3D9::~driverD3D9()
{
	BitmapFontD3D9::GetInstance().DeleteTexture();
	SAFE_RELEASE(m_pDev);
	SAFE_RELEASE(m_pd3d9);
}

bool driverD3D9::install(void* windowHandle)
{
	const HWND hWnd = static_cast<HWND>(windowHandle);
	const HDC hDC = ::GetDC(hWnd);
	m_hWnd = hWnd;
	
	RECT rect;
	GetWindowRect(hWnd, &rect);

	D3DDISPLAYMODE d3ddm;

	m_pd3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	if( !m_pd3d9 ){
		return false;
	}
	if( FAILED(m_pd3d9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm)) ){
		return false;
	}

	D3DPRESENT_PARAMETERS mDPP;
	opt_memset(&mDPP, 0, sizeof(D3DPRESENT_PARAMETERS));
	mDPP.BackBufferCount       = 1;
	mDPP.Windowed              = TRUE;
	mDPP.BackBufferFormat      = d3ddm.Format;
	mDPP.SwapEffect            = D3DSWAPEFFECT_DISCARD;
	mDPP.EnableAutoDepthStencil= TRUE;
	mDPP.AutoDepthStencilFormat= D3DFMT_D16;

	if( FAILED(m_pd3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &mDPP, &m_pDev)) ){
		if( FAILED(m_pd3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &mDPP, &m_pDev)) ){
			if( FAILED(m_pd3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &mDPP, &m_pDev)) ) {
				return false;
			}
		}
	}

	BitmapFontD3D9::GetInstance().CreateTexture(m_pDev);

	return true;
}

bool driverD3D9::uninstall()
{
	if (m_shader)
	{
		delete m_shader;
	}

	return true;
}

shaderPure* driverD3D9::compileShader(const char* shaderbuffer) const
{
	return new shaderD3D9(m_pDev, shaderbuffer);
}
	
const char* driverD3D9::getShaderName() const
{
	return "d3d9.fx";
}

void driverD3D9::setShader(shaderPure* shader)
{
	m_shader = static_cast<shaderD3D9*>(shader);
}

void driverD3D9::setTime(float t)
{
	m_time = t;
}

void driverD3D9::render()
{
	if (!m_pDev)
		return;

	m_pDev->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE);
	m_pDev->SetRenderState( D3DRS_ZENABLE, TRUE);
	m_pDev->SetRenderState( D3DRS_LIGHTING, FALSE);

	m_pDev->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0,0,0), 1.0f, 0);

	if( !m_shader || !m_shader->valid() )
		return;

	HRESULT hr = m_pDev->BeginScene();
	if (FAILED(hr))
		return;
	
	// シェーダーの描画
	struct VERTEX{
		D3DXVECTOR3 pos;
		D3DCOLOR color;
	};

	VERTEX v[] = {
		{ D3DXVECTOR3(-1.0f, +1.f, +0.f), 0xffffffff },
		{ D3DXVECTOR3(+1.0f, +1.f, +0.f), 0xffffffff },
		{ D3DXVECTOR3(+1.0f, -1.f, +0.f), 0xffffffff },

		{ D3DXVECTOR3(-1.0f, +1.f, +0.f), 0xffffffff },
		{ D3DXVECTOR3(+1.0f, -1.f, +0.f), 0xffffffff },
		{ D3DXVECTOR3(-1.0f, -1.f, +0.f), 0xffffffff },
	};
	
	m_shader->setUniform("t", m_time);
	m_shader->bind();
	m_pDev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
	m_pDev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, v, sizeof(VERTEX));
	m_shader->unbind();

	// フォント描画
	const float aspect = m_width/static_cast<float>(m_height);
	const float width = 1920/2;
	D3DXMATRIX matWorld;
	D3DXMATRIX matScaling;
	D3DXMATRIX matTranslation;

	const float xxx = 0.25f * 8.0f / width;
	const float yyy = xxx*aspect;
	D3DXMatrixScaling(&matScaling, xxx, yyy, 1.f);
	D3DXMatrixTranslation(&matTranslation, -0.98f, 0.98f, 0.f);
	matWorld = matScaling * matTranslation;

	m_pDev->SetTransform(D3DTS_WORLD, &matWorld);

	BitmapFontD3D9::GetInstance().DrawString(Logger::GetInstance().GetBuffer(), aspect, width);
	
	m_pDev->EndScene();

	
}

void driverD3D9::resize(int w, int h)
{
	m_width  = w;
	m_height = h;
	
	// デバイス復帰
	if( m_pDev->TestCooperativeLevel()==D3DERR_DEVICELOST ){
		return ;
	}
	D3DPRESENT_PARAMETERS mDPP;
	zeroMem(&mDPP, sizeof(D3DPRESENT_PARAMETERS));
	
	D3DDISPLAYMODE d3ddm;
	if( FAILED(m_pd3d9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm)) ){
		return ;
	}
	mDPP.BackBufferWidth = w;
	mDPP.BackBufferHeight = h;
	mDPP.BackBufferCount       = 1;
	mDPP.Windowed              = TRUE;
	mDPP.BackBufferFormat      = d3ddm.Format;
	mDPP.SwapEffect            = D3DSWAPEFFECT_DISCARD;
	mDPP.EnableAutoDepthStencil= TRUE;
	mDPP.AutoDepthStencilFormat= D3DFMT_D16;

	if( FAILED(m_pDev->Reset(&mDPP)) ){//FIXME D3DERR_DRIVERINTERNALERRORが返されたらExitProcess()しないとだめらしい
		return ;
	}
}

void driverD3D9::swapbuffer()
{
	if(m_pDev)
		m_pDev->Present(NULL, NULL, NULL, NULL);
}

