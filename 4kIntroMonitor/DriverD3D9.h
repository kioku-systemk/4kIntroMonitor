#ifndef INCLUDE_DRIVER_D3D9_H
#define INCLUDE_DRIVER_D3D9_H

#include "DriverPure.h"
#include "windows.h"

struct IDirect3DDevice9;
struct IDirect3D9;
struct IDirect3DVertexShader9;
struct IDirect3DPixelShader9;

class shaderD3D9 : public shaderPure
{
public:
	shaderD3D9(IDirect3DDevice9* dev, const char* shaderbuffer);
	~shaderD3D9();

	bool valid();
	void bind();
	void unbind();
	void setUniform(const char* name, float v);
	
private:
	IDirect3DPixelShader9*  m_ps;
	IDirect3DVertexShader9* m_vs;
	IDirect3DDevice9* m_pDev;
};

class driverD3D9 : public driverPure
{
public:
	driverD3D9();
	~driverD3D9();

	bool install(void* windowHandle);
	bool uninstall();
	const char* getDriverName() const { return "D3D9Driver"; }
	DRIVER_TYPE getDriverType() const { return TYPE_D3D9;    }
	
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
	HWND m_hWnd;
	shaderD3D9* m_shader;
	float m_time;
	int m_width;
	int m_height;

	IDirect3DDevice9*     m_pDev;
	IDirect3D9*           m_pd3d9;
	//D3DPRESENT_PARAMETERS m_DPP;
};

#endif // INCLUDE_DRIVER_D3D9_H
