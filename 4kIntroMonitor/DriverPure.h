#ifndef INCLUDE_DRIVER_PURE_H
#define INCLUDE_DRIVER_PURE_H

class shaderPure
{
public:
	virtual bool valid() = 0;
	virtual void bind()   = 0;
	virtual void unbind() = 0;
	virtual void setUniform(const char* name, float v) = 0;

	virtual ~shaderPure(){};
protected:
	// インスタンス作成禁止
	shaderPure(){};
	
};

class driverPure
{
public:
	enum DRIVER_TYPE
	{
		TYPE_NULL = 0,
		TYPE_OGL,
		TYPE_D3D9,
		TYPE_D3D11,

		DRIVER_NUM
	};

	virtual bool install(void* windowHandle)   = 0;
	virtual bool uninstall()                   = 0;
	virtual const char* getDriverName() const  = 0;
	virtual DRIVER_TYPE getDriverType() const  = 0;
	
	// shader compile & link
	virtual shaderPure* compileShader(const char* shaderbuffer) const = 0;
	
	// basic render
	virtual const char* getShaderName() const  = 0;
	virtual void setTime(float t) = 0;
	virtual void setShader(shaderPure* shader) = 0;
	virtual void render()                      = 0;
	
	// effect(optional)
	virtual const char* getBufferShaderName() const { return 0; };
	virtual void setBufferShader(shaderPure* shader){};
	virtual void bindBuffer(){};
	virtual void unbindBuffer(){};
	virtual void renderBuffer(){};

	// operation
	virtual void resize(int w, int h) = 0;
	virtual void swapbuffer()         = 0;

	virtual ~driverPure(){};
protected:
	// インスタンス作成禁止
	driverPure(){};

};

#endif
