#include "DriverD3D11.h"

#include "Util.h"
#include "ShaderCompactor.h"

#include "Logger.h"
#include "lcfont_bw112x66.h"

#include <d3d11.h>
#include <d3dx11.h>

#include <vector>

#define SAFE_RELEASE(A) {if(A){A->Release();A=0;}}

namespace {

enum eShaderType
{
	D3D11_VS,
	D3D11_PS,
	D3D11_GS
};


void compileShaderSrc(ID3D11Device* pDevice, eShaderType type, const char* csh, void** ppShaderBuffer, const D3D11_INPUT_ELEMENT_DESC* inputelement_desc = 0, int ie_num = 0, ID3D11InputLayout** pLayout = 0, bool effect = false);


class BitmapFontD3D11
{
	private:
		//unsigned int texName;
		BitmapFontD3D11(){};
		ID3D11Texture2D* pFont;
		ID3D11ShaderResourceView* pFontShaderResource;
		ID3D11Device* m_pDev;
		ID3D11DeviceContext* m_pCon;

		ID3D11InputLayout* m_inputlayout;
		ID3D11VertexShader* m_pVS;
		ID3D11PixelShader* m_pPS;
		ID3D11Buffer* m_pVB;
		int m_maxlen;

		ID3D11RasterizerState* m_rstate;
		ID3D11DepthStencilState* m_pDS;
		ID3D11BlendState* m_pBS;
public:
		static BitmapFontD3D11& GetInstance()
		{
			static BitmapFontD3D11* singleton = NULL;
			if (singleton == NULL)
				singleton = new BitmapFontD3D11();
			return *singleton;
		}
		void CreateTexture(ID3D11Device* pDev, ID3D11DeviceContext* pCon)
		{
			m_pDev = pDev;
			m_pCon = pCon;
			const D3D11_TEXTURE2D_DESC texdesc = {
				112, 66, 1, 1, DXGI_FORMAT_R8_UNORM,1,0,
				D3D11_USAGE_DEFAULT,
				D3D11_BIND_SHADER_RESOURCE, 0, 0};
			D3D11_SUBRESOURCE_DATA texdata = {lcfont_bw112x66, 112, 0};
			m_pDev->CreateTexture2D(&texdesc, &texdata, &pFont);
			if (!pFont)
				return ;
			
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			ZeroMemory( &srvDesc, sizeof(srvDesc) );
			srvDesc.Format = texdesc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = texdesc.MipLevels;
			m_pDev->CreateShaderResourceView(pFont, &srvDesc, &pFontShaderResource );

			//font shader
			m_inputlayout = 0;
			m_pVS = 0;
			m_pPS = 0;
			const D3D11_INPUT_ELEMENT_DESC	layout[] =
			{
				{ "IN_POS" , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "IN_TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
		
			static const char* font_sh = "\
			Texture2D fonttex : register( t0 );\
			SamplerState Samp : register( s0 );\
			struct VS_OUTPUT\
			{\
				float4 pos : SV_POSITION;\
				float2 tex : TEXCOORD0;\
			};\
			\
			float4 ps( VS_OUTPUT In ) : SV_TARGET\
			{\
				return fonttex.Sample(Samp, In.tex).rrrr;\
			}\
			VS_OUTPUT vs(float3 pos : IN_POS0, float2 tex : IN_TEXCOORD0)\
			{\
				VS_OUTPUT Out;\
				Out.pos = float4(pos,1);\
				Out.tex = tex;\
				return Out;\
			}";
			compileShaderSrc(m_pDev, D3D11_VS, font_sh, (void**)&m_pVS, layout, sizeof(layout)/sizeof(D3D11_INPUT_ELEMENT_DESC), &m_inputlayout); 
			compileShaderSrc(m_pDev, D3D11_PS, font_sh, (void**)&m_pPS);
			
			m_pVB = 0;
			m_maxlen = 0;

			D3D11_RASTERIZER_DESC rsDesc;
			zeroMem( &rsDesc, sizeof( D3D11_RASTERIZER_DESC ) );
			rsDesc.CullMode = D3D11_CULL_BACK;
			rsDesc.FillMode = D3D11_FILL_SOLID; 
			rsDesc.DepthClipEnable = FALSE;
			m_pDev->CreateRasterizerState(&rsDesc, &m_rstate);

			D3D11_DEPTH_STENCIL_DESC dsDesc;
			zeroMem( &dsDesc, sizeof( D3D11_DEPTH_STENCIL_DESC ) );
			dsDesc.DepthEnable	  = FALSE;
			dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			dsDesc.DepthFunc      = D3D11_COMPARISON_LESS;
			dsDesc.StencilEnable  = FALSE;
			m_pDev->CreateDepthStencilState( &dsDesc, &m_pDS);

			D3D11_BLEND_DESC BlendDesc;
			zeroMem( &BlendDesc, sizeof( BlendDesc ) );
			BlendDesc.AlphaToCoverageEnable = FALSE;
			BlendDesc.IndependentBlendEnable = FALSE;
			BlendDesc.RenderTarget[0].BlendEnable = TRUE;
			BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_INV_DEST_COLOR;//D3D11_BLEND_SRC_ALPHA;
			BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;//D3D11_BLEND_INV_SRC_ALPHA;
			BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			m_pDev->CreateBlendState( &BlendDesc, &m_pBS );
		}
		void DeleteTexture()
		{
			SAFE_RELEASE(m_inputlayout);
			SAFE_RELEASE(pFontShaderResource);
			SAFE_RELEASE(pFont);
			SAFE_RELEASE(m_rstate);
			SAFE_RELEASE(m_pDS);
			SAFE_RELEASE(m_pBS);
		}

		void DrawString(const char* strbuf, float aspect, float width)
		{
			int strnum = lstrlen(strbuf);
			bool recreate = false;
			if (m_pVB == 0)
			{
				recreate = true;
			}
			if (m_maxlen < strnum)
			{
				m_maxlen = strnum;
				SAFE_RELEASE(m_pVB);
				recreate = true;
			}
			
			if (recreate)
			{
				D3D11_BUFFER_DESC desc;
				desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				desc.Usage= D3D11_USAGE_DYNAMIC;
				desc.CPUAccessFlags= D3D11_CPU_ACCESS_WRITE;
				desc.MiscFlags= 0;
				desc.ByteWidth= sizeof(float) * 5 * 6 * m_maxlen;
				desc.StructureByteStride= sizeof(float) * 5 * 6;
				m_pDev->CreateBuffer(&desc, 0, &m_pVB);
			}

			ID3D11RasterizerState* oldrs;
			ID3D11DepthStencilState* oldds;
			unsigned int olddsref;
			m_pCon->RSGetState(&oldrs);
			m_pCon->OMGetDepthStencilState(&oldds, &olddsref);
			m_pCon->RSSetState(m_rstate);
			m_pCon->OMSetDepthStencilState(m_pDS, 0);
			/*
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
			*/
			float fontWidth = 7.0f * 0.0022f;
			float fontHeight = 11.0f * 0.0022f * aspect;
			const float ddx_init = -0.98f;
			float ddx = ddx_init;
			float ddy = 0.98f;

			struct VERTEX{
				float x,y,z;
				float u,v;
			};
			std::vector<VERTEX> vbmem;
			int strcnt = 0;
			unsigned int strend = strnum;//lstrlen(strbuf);
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
			

					const VERTEX vertex[] = {
						{ ddx, ddy, -0.1,  ptx,  pty },
						{ ddx+fontWidth, ddy, -0.1, ptx + dtx, pty },
						{ ddx+fontWidth, ddy - fontHeight, -0.1, ptx + dtx, (pty - dty) },

						{ ddx, ddy, -0.1,  ptx,  pty },
						{ ddx, ddy - fontHeight, -0.1,  ptx, (pty - dty) },
						{ ddx+fontWidth, ddy - fontHeight, -0.1, ptx + dtx, (pty - dty) },
						
/*
						// works fine. stretched
						{ -1.f,  1.f, 0, ptx,  pty },
						{  1.f,  1.f, 0, ptx + dtx, pty },
						{  1.f, -1.f, 0, ptx + dtx, (pty - dty) },

						{ -1.f,  1.f, 0, ptx, pty },
						{  1.f, -1.f, 0, ptx + dtx, (pty - dty) },
						{ -1.f, -1.f, 0, ptx, (pty - dty) },*/
					};
					strcnt++;
					for (int i = 0; i < sizeof(vertex)/sizeof(VERTEX); i++)
						vbmem.push_back(vertex[i]);

					//m_pDev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
					//m_pDev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertex, sizeof(VERTEX));
				}
				
				ddx += fontWidth;
				if (strbuf[strptr] == 0xA || strbuf[strptr] == 0xC)
				{
					ddx = ddx_init;
					ddy -= fontHeight;
				}
			}
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			m_pCon->Map(m_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
			memcpy(MappedResource.pData, &vbmem[0], sizeof(float)*5*6*strcnt);
			m_pCon->Unmap(m_pVB, 0);
/*
			m_pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, dwAlphaBlendEnable);
			m_pDev->SetRenderState(D3DRS_ZENABLE, dwZEnable);
			m_pDev->SetRenderState(D3DRS_BLENDOP, dwBlendOp);
			m_pDev->SetRenderState(D3DRS_SRCBLEND, dwSrcBlend);
			m_pDev->SetRenderState(D3DRS_DESTBLEND, dwDstBlend);

			m_pDev->SetSamplerState(0, D3DSAMP_MAGFILTER, dwMagFilter);
			m_pDev->SetSamplerState(0, D3DSAMP_MINFILTER, dwMinFilter);
			m_pDev->SetSamplerState(0, D3DSAMP_MIPFILTER, dwMipFilter);

			m_pDev->SetTexture(0, NULL);*/

			// DRAW
			m_pCon->IASetInputLayout(m_inputlayout);
			m_pCon->VSSetShader(m_pVS, 0, 0);
			m_pCon->PSSetShader(m_pPS, 0, 0);
			m_pCon->PSSetShaderResources(0, 1, &pFontShaderResource);
			ID3D11BlendState* oldbs = 0;
			unsigned int oldmask = 0;
			m_pCon->OMGetBlendState(&oldbs, 0, &oldmask);
			m_pCon->OMSetBlendState(m_pBS, 0, 0xffffffff );
			unsigned int stride = sizeof(float)*5;
			unsigned int offset = 0;
			m_pCon->IASetVertexBuffers( 0, 1, &m_pVB, &stride, &offset);
			m_pCon->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
			m_pCon->Draw(6 * strcnt, 0);
			m_pCon->OMSetBlendState(oldbs, 0, oldmask );
			SAFE_RELEASE(oldbs);
			m_pCon->VSSetShader(0, 0, 0);
			m_pCon->PSSetShader(0, 0, 0);
			m_pCon->RSSetState(oldrs);
			m_pCon->OMSetDepthStencilState(oldds, olddsref);
			SAFE_RELEASE(oldrs);
			SAFE_RELEASE(oldds);
		}
		
};



void compileShaderSrc(ID3D11Device* pDevice, eShaderType type, const char* csh, void** ppShaderBuffer, const D3D11_INPUT_ELEMENT_DESC* inputelement_desc, int ie_num, ID3D11InputLayout** pLayout, bool effect)
{
	DWORD flags = D3D10_SHADER_ENABLE_STRICTNESS;
	switch( type ){
	case D3D11_VS:
		{
			ID3DBlob*	pError = NULL;
			char*		vsProfile = "vs_4_0";
			ID3DBlob*	pVSBuffer = NULL;
			HRESULT hr;
			hr = D3DX11CompileFromMemory(csh, lstrlen(csh), NULL, NULL, NULL, "vs", vsProfile, flags, 0, NULL, &pVSBuffer, &pError, NULL );
			if( FAILED(hr) )
			{
				OutputDebugStringA( (char*)pError->GetBufferPointer() );
				Logger::GetInstance().OutputString(static_cast< const char* >(pError->GetBufferPointer()));
				Logger::GetInstance().OutputString("Compile error in vertex shader.\n\n\n");
			}
			else
			{
				hr = pDevice->CreateVertexShader( pVSBuffer->GetBufferPointer(), pVSBuffer->GetBufferSize(), NULL, (ID3D11VertexShader**)ppShaderBuffer );
				if( FAILED(hr) )
					Logger::GetInstance().OutputString("Create failed vertex shader.\n\n\n");
			}
			/*
			// 入力頂点属性を作成する
			const D3D11_INPUT_ELEMENT_DESC	layout[] =
			{
				{ "IN_POS" , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				//{ "IN_TEXCOORD0", 0, DXGI_FORMAT_R32G32_FLOAT,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				//{ "IN_TEXCOORD1", 0, DXGI_FORMAT_R32G32_FLOAT,  0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			*/
			if (pLayout)
			{
				hr = pDevice->CreateInputLayout( inputelement_desc, ie_num, pVSBuffer->GetBufferPointer(), pVSBuffer->GetBufferSize(), (ID3D11InputLayout**)pLayout );
				if( FAILED(hr) )
					Logger::GetInstance().OutputString("Create failed input layout error.\n\n\n");
			}
			
			SAFE_RELEASE( pVSBuffer );
			
		}break;
	case D3D11_PS:
		{
			ID3DBlob*	pError = NULL;
			char*		psProfile = "ps_4_0";
			ID3DBlob*	pPSBuffer = NULL;
			const char* funcName = effect?"PSeff":"ps";
			HRESULT hr;
			hr = D3DX11CompileFromMemory(csh, lstrlen(csh), NULL, NULL, NULL, funcName, psProfile, flags, 0, NULL, &pPSBuffer, &pError, NULL );
			if( FAILED(hr) )
			{
				OutputDebugStringA( (char*)pError->GetBufferPointer() );
				Logger::GetInstance().OutputString(static_cast< const char* >(pError->GetBufferPointer()));
				Logger::GetInstance().OutputString("Compile error in pixel shader.\n\n\n");
			}
			else
			{
				hr = pDevice->CreatePixelShader( pPSBuffer->GetBufferPointer(), pPSBuffer->GetBufferSize(), NULL, (ID3D11PixelShader**)ppShaderBuffer );
				if( FAILED(hr) )
					Logger::GetInstance().OutputString("Create failed pixel shader.\n\n\n");
			}
			SAFE_RELEASE( pPSBuffer );
			
		}break;
	case D3D11_GS:
		{
			ID3DBlob*	pError = NULL;
			char*		gsProfile = "gs_4_0";
			ID3DBlob*	pGSBuffer = NULL;
			const char* funcName = effect ? "GSeff":"gs";
			HRESULT hr;
			hr = D3DX11CompileFromMemory(csh, lstrlen(csh), NULL, NULL, NULL, funcName, gsProfile, flags, 0, NULL, &pGSBuffer, &pError, NULL );
			if( FAILED(hr) )
			{
				OutputDebugStringA( (char*)pError->GetBufferPointer() );
				Logger::GetInstance().OutputString(static_cast< const char* >(pError->GetBufferPointer()));
				Logger::GetInstance().OutputString("Compile error in geometry shader.\n\n\n");
			}
			else
			{
				hr = pDevice->CreateGeometryShader( pGSBuffer->GetBufferPointer(), pGSBuffer->GetBufferSize(), NULL, (ID3D11GeometryShader**)ppShaderBuffer );
				if( FAILED(hr) )
					Logger::GetInstance().OutputString("Create failed geometry shader.\n\n\n");
			}
			SAFE_RELEASE( pGSBuffer );
			
		}break;
	}
}


} // namespace

shaderD3D11::shaderD3D11(ID3D11Device* pDev, ID3D11DeviceContext* pContext, const char* shaderbuffer)
{
	m_vs = 0;
	m_ps = 0;
	m_pContext = pContext;

	static const D3D11_BUFFER_DESC cbDesc = {
		sizeof(float)*4,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_CONSTANT_BUFFER,
		D3D11_CPU_ACCESS_WRITE,0,0
	};
	pDev->CreateBuffer(&cbDesc, NULL, &m_pConst);

	compileShaderSrc(pDev, D3D11_VS, shaderbuffer, reinterpret_cast<void**>(&m_vs));
	compileShaderSrc(pDev, D3D11_PS, shaderbuffer, reinterpret_cast<void**>(&m_ps));
}
shaderD3D11::~shaderD3D11()
{
	SAFE_RELEASE(m_vs);
	SAFE_RELEASE(m_ps);
}

bool shaderD3D11::valid()
{
	if (m_vs && m_ps)
		return true;
	else
		return false;	
}

void shaderD3D11::bind()
{
	m_pContext->VSSetShader(m_vs, NULL, 0);
	m_pContext->PSSetShader(m_ps, NULL, 0);
}

void shaderD3D11::unbind()
{
	m_pContext->VSSetShader(0, NULL, 0);
	m_pContext->PSSetShader(0, NULL, 0);
}

void shaderD3D11::setUniform(const char* name, float v)
{
	D3D11_MAPPED_SUBRESOURCE mapres;
	m_pContext->Map(m_pConst, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapres);
	*reinterpret_cast<float*>(mapres.pData) = v;
	m_pContext->Unmap(m_pConst, 0);  
	m_pContext->VSSetConstantBuffers(0, 1, &m_pConst);
	m_pContext->PSSetConstantBuffers(0, 1, &m_pConst);
}


bool driverD3D11::setDefaultBackBufferAndDepthBuffer(int w, int h)
{
	HRESULT hr = S_OK;

	// バックバッファの取得
	ID3D11Texture2D* pBackBuffer;
	m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(&pBackBuffer) );
	
	// RenderTargetViewの作成
	hr = m_pDevice->CreateRenderTargetView( pBackBuffer, NULL, &m_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return false;

	// デプステクスチャの作成
	D3D11_TEXTURE2D_DESC depthDesc;
	zeroMem( &depthDesc, sizeof(depthDesc) );
	depthDesc.Width              = w;
	depthDesc.Height             = h;
	depthDesc.MipLevels          = 1;
	depthDesc.ArraySize          = 1;
	depthDesc.Format             = DXGI_FORMAT_D32_FLOAT;
	depthDesc.SampleDesc.Count   = 1;
	depthDesc.SampleDesc.Quality = 0;
	depthDesc.Usage              = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;
	depthDesc.CPUAccessFlags     = 0;
	depthDesc.MiscFlags          = 0;
	hr = m_pDevice->CreateTexture2D( &depthDesc, NULL, &m_pDepthStencil );
	if (FAILED(hr))
		return false;

	// DepthStencilViewの作成
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	zeroMem( &dsvDesc, sizeof(dsvDesc) );	
	dsvDesc.Format = depthDesc.Format;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
	dsvDesc.Texture2D.MipSlice = 0;
	hr = m_pDevice->CreateDepthStencilView(m_pDepthStencil, &dsvDesc, &m_pDepthStencilView);
	if (FAILED(hr))
		return false;
		
	// レンダリングターゲットを設定する
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

	// ついでにビューポートの設定
	D3D11_VIEWPORT	vp;
	vp.Width    = static_cast<float>(w);
	vp.Height   = static_cast<float>(h);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_pDeviceContext->RSSetViewports(1, &vp);

	return true;
}

void driverD3D11::releaseDefaultBackBufferAndDepthBuffer()
{
	ID3D11RenderTargetView* buf = 0;
	m_pDeviceContext->OMSetRenderTargets(1, &buf, 0);

	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pDepthStencilView);
	SAFE_RELEASE(m_pDepthStencil);
}


driverD3D11::driverD3D11()
{
	m_time = 0;
	m_shader = 0;
}

driverD3D11::~driverD3D11()
{
	SAFE_RELEASE(m_pDeviceContext);
	SAFE_RELEASE(m_pDevice);
}

bool driverD3D11::install(void* windowHandle)
{
	const HWND hWnd = static_cast<HWND>(windowHandle);
	const HDC hDC = ::GetDC(hWnd);
	m_hWnd = hWnd;

	RECT rect;
	GetClientRect(hWnd, &rect);

	D3D_DRIVER_TYPE	dtype = D3D_DRIVER_TYPE_HARDWARE;
	UINT            flags = 0;
	D3D_FEATURE_LEVEL featureLevels[] = // featureLevel
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1,
	};
	UINT numFeatureLevels = sizeof(featureLevels) / sizeof(D3D_FEATURE_LEVEL);
	UINT sdkVersion = D3D11_SDK_VERSION;
	D3D_FEATURE_LEVEL validFeatureLevel;

	m_width = rect.right - rect.left;
	m_height = rect.bottom - rect.top;

	// スワップチェーンの設定(D3D9でいうD3DPRESENT_PARAMETERS相当の設定)
	DXGI_SWAP_CHAIN_DESC scDesc;
	zeroMem( &scDesc, sizeof(scDesc) );
	scDesc.BufferCount                        = 1;
	scDesc.BufferDesc.Width                   = m_width;
	scDesc.BufferDesc.Height                  = m_height;
	scDesc.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.BufferDesc.RefreshRate.Numerator   = 60;
	scDesc.BufferDesc.RefreshRate.Denominator = 1;
	scDesc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.OutputWindow                       = hWnd;
	scDesc.SampleDesc.Count                   = 1;
	scDesc.SampleDesc.Quality                 = 0;
	scDesc.Windowed = TRUE;

	// デバイスとデバイスコンテキストとスワップチェーンの作成
	IDXGIAdapter*           adapter = 0;
	HRESULT	hr = D3D11CreateDeviceAndSwapChain(
		adapter,
		dtype,
		NULL,
		flags,
		featureLevels,
		numFeatureLevels,
		sdkVersion,
		&scDesc,
		&m_pSwapChain,
		&m_pDevice,
		&validFeatureLevel,
		&m_pDeviceContext );
	
	setDefaultBackBufferAndDepthBuffer(m_width, m_height);
	
	BitmapFontD3D11::GetInstance().CreateTexture(m_pDevice, m_pDeviceContext);

	return true;
}

bool driverD3D11::uninstall()
{
	if (m_shader)
	{
		delete m_shader;
	}
	BitmapFontD3D11::GetInstance().DeleteTexture();

	return true;
}

shaderPure* driverD3D11::compileShader(const char* shaderbuffer) const
{
	return new shaderD3D11(m_pDevice, m_pDeviceContext, shaderbuffer);
}
	
const char* driverD3D11::getShaderName() const
{
	return "d3d11.fx";
}

void driverD3D11::setShader(shaderPure* shader)
{
	m_shader = static_cast<shaderD3D11*>(shader);
}

void driverD3D11::setTime(float t)
{
	m_time = t;
}

void driverD3D11::render()
{
	float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pDeviceContext->ClearRenderTargetView( m_pRenderTargetView, clearColor );
	m_pDeviceContext->ClearDepthStencilView( m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0 );

	if (!m_shader)
		return;

	/*
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	m_pDeviceContext->Map( m_pConst, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_VS_PER_FRAME* pCB = ( CB_VS_PER_FRAME* )MappedResource.pData;
	pCB->tm = mTime;
	//opt_memcpy(pCB->proj, &projection, sizeof(float[16]));
	m_pDeviceContext->Unmap( m_pConst, 0 );
	m_pDeviceContext->VSSetConstantBuffers( 0, 1, &m_pConst );
	m_pDeviceContext->PSSetConstantBuffers( 0, 1, &m_pConst );
	m_pDeviceContext->GSSetConstantBuffers( 0, 1, &m_pConst );
	*/
	// プリミティブタイプのセット
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	//m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	
	m_shader->setUniform("t", m_time);
	m_shader->bind();
	m_pDeviceContext->Draw(6, 0);
	m_shader->unbind();

	BitmapFontD3D11::GetInstance().DrawString(Logger::GetInstance().GetBuffer(), m_width/(float)m_height, m_width);
	//BitmapFontD3D11::GetInstance().DrawString("AAAAAA", 1, 920);
	
}

void driverD3D11::resize(int w, int h)
{
	m_width  = w;
	m_height = h;

	releaseDefaultBackBufferAndDepthBuffer();

	m_pSwapChain->ResizeBuffers(1, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	setDefaultBackBufferAndDepthBuffer(w, h);
}

void driverD3D11::swapbuffer()
{
	m_pDeviceContext->Flush();
	m_pSwapChain->Present(NULL, NULL);
}

