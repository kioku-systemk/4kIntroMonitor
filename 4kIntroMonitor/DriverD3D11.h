#ifndef INCLUDE_DRIVER_D3D11_H
#define INCLUDE_DRIVER_D3D11_H

#include "DriverPure.h"
#include "windows.h"

struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11Texture2D;
struct ID3D11DepthStencilView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11GeometryShader;
struct ID3D11Buffer;

class shaderD3D11 : public shaderPure
{
public:
	shaderD3D11(ID3D11Device* pDev, ID3D11DeviceContext* pContext, const char* shaderbuffer);
	~shaderD3D11();

	bool valid();
	void bind();
	void unbind();
	void setUniform(const char* name, float v);
	
private:
	ID3D11DeviceContext* m_pContext;
	ID3D11VertexShader* m_vs;
	ID3D11PixelShader* m_ps;
	ID3D11Buffer* m_pConst;
};

class driverD3D11 : public driverPure
{
public:
	driverD3D11();
	~driverD3D11();

	bool install(void* windowHandle);
	bool uninstall();
	const char* getDriverName() const { return "D3D11Driver"; }
	DRIVER_TYPE getDriverType() const { return TYPE_D3D11;    }
	
	// shader compile & link
	shaderPure* compileShader(const char* shaderbuffer) const;
	
	// basic render
	const char* getShaderName() const;
	void setShader(shaderPure* shader);
	void setTime(float t);
	void render();
	
	// operation
	void resize(int w, int h);
	void swapbuffer();

private:
	
	bool setDefaultBackBufferAndDepthBuffer(int w, int h);
	void releaseDefaultBackBufferAndDepthBuffer();
	
	HWND m_hWnd;
	shaderD3D11* m_shader;
	float m_time;
	int m_width;
	int m_height;

	IDXGISwapChain*         m_pSwapChain     ; // �o�b�N�o�b�t�@�̐؂�ւ�
	ID3D11Device*           m_pDevice        ; // ���\�[�X�쐬
	ID3D11DeviceContext*    m_pDeviceContext ; // �`��A�X�e�[�g�̐؂�ւ�
	
	ID3D11RenderTargetView* m_pRenderTargetView; // �o�b�N�o�b�t�@�p
	ID3D11Texture2D*        m_pDepthStencil;     // �f�v�X�o�b�t�@�̌�
	ID3D11DepthStencilView* m_pDepthStencilView; // �f�v�X�o�b�t�@�p


};

#endif // INCLUDE_DRIVER_D3D11_H
