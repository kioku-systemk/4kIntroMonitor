#ifndef INCLUDE_DRIVER_NULL_H
#define INCLUDE_DRIVER_NULL_H

#include "DriverPure.h"

class shaderNull : public shaderPure
{
public:
	shaderNull()  {};
	~shaderNull() {};

	bool valid(){ return true; };
	void bind()   {};
	void unbind() {};
	void setUniform(const char* name, float v) {};
};

class driverNull : public driverPure
{
public:
	driverNull()  {};
	~driverNull() {};

	bool install(void* windowHandle)  { return true;         }
	bool uninstall()                  { return true;         }
	const char* getDriverName() const { return "NullDriver"; }
	DRIVER_TYPE getDriverType() const { return TYPE_NULL;    }
	
	// shader compile & link
	shaderPure* compileShader(const char* shaderbuffer) const { return 0; }
	
	// basic render
	const char* getShaderName() const  { return ""; };
	void setShader(shaderPure* shader) {};
	void setTime(float t){};
	void render() {};
	
	// operation
	void resize(int w, int h) {};
	void swapbuffer() {};

};

#endif // INCLUDE_DRIVER_NULL_H
