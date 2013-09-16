#ifndef INCLUDE_DRIVER_OGL_H
#define INCLUDE_DRIVER_OGL_H

#include "DriverPure.h"
#include "windows.h"

class shaderOGL : public shaderPure
{
public:
	shaderOGL(const char* shaderbuffer);
	~shaderOGL();

	bool valid();
	void bind();
	void unbind();
	void setUniform(const char* name, float v);
	
private:
	unsigned int m_prg;
};

class driverOGL : public driverPure
{
public:
	driverOGL();
	~driverOGL();

	bool install(void* windowHandle);
	bool uninstall();
	const char* getDriverName() const { return "OGLDriver"; }
	DRIVER_TYPE getDriverType() const { return TYPE_OGL;    }
	
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
	shaderOGL* m_shader;
	float m_time;
	int m_width;
	int m_height;
};

#endif // INCLUDE_DRIVER_OGL_H
