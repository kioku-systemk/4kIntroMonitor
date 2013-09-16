#include "DriverOGL.h"

#include "Util.h"
#include "ShaderCompactor.h"

#include "Logger.h"
#include "lcfont_bw112x66.h"

#include <GL/gl.h>
#include <GL/glext.h>

namespace {

class BitmapFontGL
{
	private:
		unsigned int texName;
		BitmapFontGL(){};
		
	public:
		static BitmapFontGL& GetInstance()
		{
			static BitmapFontGL* singleton = NULL;
			if (singleton == NULL)
				singleton = new BitmapFontGL();
			return *singleton;
		}
		void CreateTexture()
		{
			glPushAttrib(GL_ENABLE_BIT);
			glEnable(GL_TEXTURE_2D);
			glGenTextures(1, &texName);
			glBindTexture(GL_TEXTURE_2D, texName);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexImage2D(GL_TEXTURE_2D, 0, 3, 112, 66,
				0, GL_LUMINANCE, GL_UNSIGNED_BYTE, lcfont_bw112x66);
			glPopAttrib();
		}
		
		void DrawString(const char* strbuf, float aspect, float width)
		{
			unsigned int fontWidth = 7;
			unsigned int fontHeight = 11;
			unsigned int ddx = 0;
			unsigned int ddy = 0;
			glPushMatrix();
			glScalef(0.25f * 8.0f / width,
					 0.25f * 8.0f / width * aspect,
					 0.25f * 8.0f / width);
			glPushAttrib(GL_ENABLE_BIT);
			glEnable(GL_TEXTURE_2D);
			glEnable(GL_BLEND);
			glDisable(GL_DEPTH_TEST);
			//glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
			glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR);
			//glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			//glEnable(GL_COLOR_MATERIAL);
			//glColor3f(0.4f, 1.0f, 0.4f);

			glBindTexture(GL_TEXTURE_2D, texName);
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
				
					glBegin(GL_QUADS);
						glTexCoord2f(ptx              , pty       );
						glVertex2i  (ddx              , ddy       );
						glTexCoord2f(ptx + dtx        , pty       );
						glVertex2i  (ddx + fontWidth  , ddy       );
						glTexCoord2f(ptx + dtx        , pty - dty );
						glVertex2i  (ddx + fontWidth  , ddy - fontHeight  );
						glTexCoord2f(ptx              , pty - dty );
						glVertex2i  (ddx              , ddy - fontHeight  );
					glEnd();	
				}
				
				ddx += fontWidth;
				if (strbuf[strptr] == 0xA || strbuf[strptr] == 0xC)
				{
					ddx = 0;
					ddy -= fontHeight;
				}
			}
			glPopAttrib();
			glPopMatrix();
		}
};


// エラーログのトレース
void getErrorLog(GLuint shader)
{
	GLsizei bufSize=0;
	((PFNGLGETSHADERIVPROC)(wglGetProcAddress("glGetShaderiv")))(shader, GL_INFO_LOG_LENGTH , &bufSize);

	if (bufSize > 1)
	{
		GLchar *infoLog;

		infoLog = (GLchar*)tmalloc(bufSize);
		if (infoLog != NULL)
		{
			GLsizei length;

			((PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog"))(shader, bufSize, &length, infoLog);
			char* buf = (GLchar*)tmalloc(bufSize+1024);
			wsprintf(buf, "COMPILE STATUS:\n%s\n\n", infoLog);
			Logger::GetInstance().OutputStringD(buf);
			tfree(buf);
			tfree(infoLog);
		}
		else
			Logger::GetInstance().OutputString("Could not allocate InfoLog buffer.\n");
	}
}

// プログラム情報の出力
void printProgramInfoLog(GLuint program)
{
	GLsizei bufSize=0;
	((PFNGLGETPROGRAMIVNVPROC)(wglGetProcAddress("glGetProgramiv")))(program, GL_INFO_LOG_LENGTH , &bufSize);

	if (bufSize > 1)
	{
		GLchar *infoLog;

		infoLog = (GLchar*)tmalloc(bufSize);
		if (infoLog != NULL)
		{
			GLsizei length;
			((PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog"))(program, bufSize, &length, infoLog);

			char* buf = (GLchar*)tmalloc(bufSize+256);
			wsprintf(buf, "LINK STATUS:\n%s\n\n", infoLog);
			Logger::GetInstance().OutputStringD(buf);
			tfree(buf);
			tfree(infoLog);
		}
		else
			Logger::GetInstance().OutputString("Could not allocate InfoLog buffer.\n");
	}
}

enum eShaderType
{
	GLSL_VS,
	GLSL_FS
};

// シェーダーをコンパイル
unsigned int compileShader(eShaderType type, const char* shaderSource)
{
	GLint compiled;
	unsigned int vshaderprg = 0;
	unsigned int fshaderprg = 0;
	switch( type ){
	case GLSL_VS:
		{
			if (vshaderprg != 0)
				((PFNGLDELETESHADERPROC)(wglGetProcAddress("glDeleteShader")))(vshaderprg);
			vshaderprg = ((PFNGLCREATESHADERPROC)(wglGetProcAddress("glCreateShader")))(GL_VERTEX_SHADER);
			((PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource"))(vshaderprg,1, &shaderSource,0);
			((PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader"))(vshaderprg);

			((PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv"))(vshaderprg, GL_COMPILE_STATUS, &compiled);
			if (compiled == GL_FALSE)
			{
				getErrorLog(vshaderprg);
				Logger::GetInstance().OutputString("Compile error in vertex shader.\n\n\n");
				((PFNGLDELETESHADERPROC)(wglGetProcAddress("glDeleteShader")))(vshaderprg);
				vshaderprg = 0;
			}
			return vshaderprg;
		}break;
	case GLSL_FS:
		{
			if (fshaderprg != 0)
				((PFNGLDELETESHADERPROC)(wglGetProcAddress("glDeleteShader")))(fshaderprg);
			fshaderprg = ((PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader"))(GL_FRAGMENT_SHADER);
			((PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource"))(fshaderprg,1, &shaderSource,0);
			((PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader"))(fshaderprg);
			//GLint compiled;
			((PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv"))(fshaderprg, GL_COMPILE_STATUS, &compiled);
			if (compiled == GL_FALSE)
			{
				getErrorLog(fshaderprg);
				Logger::GetInstance().OutputString("Compile error in fragment shader.\n\n\n");
				((PFNGLDELETESHADERPROC)(wglGetProcAddress("glDeleteShader")))(fshaderprg);
				fshaderprg = 0;
			}
			return fshaderprg;
		}break;
	}
	return 0;
}

// シェーダーをリンク
unsigned int linkShader(unsigned int vshaderprg, unsigned int fshaderprg)
{
	unsigned int program = 0;
	if (vshaderprg != 0 && fshaderprg != 0)
	{
		program = ((PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram"))();
		((PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader"))(program,vshaderprg);
		((PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader"))(program,fshaderprg);
		// ------ Link -----
		((PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram"))(program);
		GLint linked;
		((PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv"))(program, GL_LINK_STATUS, &linked);
		printProgramInfoLog(program);
		if (linked == GL_FALSE)
		{
			Logger::GetInstance().OutputString("Link error.\n");
			((PFNGLDELETEPROGRAMPROC)wglGetProcAddress("glDeleteProgram"))(program);
			program = 0;
		}
	}
	return program;
}

void deleteShader(unsigned int prg)
{
	if (prg != 0)
		((PFNGLDELETEPROGRAMPROC)wglGetProcAddress("glDeleteProgram"))(prg);
}

} // namespace

shaderOGL::shaderOGL(const char* shaderbuffer)
{
	m_prg = 0;
	static const char* vsshader = "attribute vec2 pos;void main(){gl_Position=vec4(pos.xy,0,1);}";
	unsigned int vsh = compileShader(GLSL_VS, vsshader);
	if (!vsh)
		Logger::GetInstance().OutputString("VertexShader error.\n");
	unsigned int fsh = compileShader(GLSL_FS, shaderbuffer);
	if (!fsh)
		Logger::GetInstance().OutputString("FragmentShader error.\n");

	if (!vsh || !fsh)
	{
		if (vsh)
			((PFNGLDELETESHADERPROC)(wglGetProcAddress("glDeleteShader")))(vsh);
		if (fsh)
			((PFNGLDELETESHADERPROC)(wglGetProcAddress("glDeleteShader")))(fsh);
		return;
	}

	unsigned int prg = linkShader(vsh, fsh);
	if (prg)
	{
		deleteShader(m_prg);
		m_prg = prg;

		((PFNGLDELETESHADERPROC)(wglGetProcAddress("glDeleteShader")))(vsh);
		((PFNGLDELETESHADERPROC)(wglGetProcAddress("glDeleteShader")))(fsh);
	}
}
shaderOGL::~shaderOGL()
{
	if (m_prg != 0)
	{
		((PFNGLDELETEPROGRAMPROC)wglGetProcAddress("glDeleteProgram"))(m_prg);
		m_prg = 0;
	}
}

bool shaderOGL::valid()
{
	return m_prg != 0;
}

void shaderOGL::bind()
{
	((PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram"))(m_prg);
}

void shaderOGL::unbind()
{
	((PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram"))(0);
}

void shaderOGL::setUniform(const char* name, float v)
{
	if (m_prg)
	{
		const int id = ((PFNGLGETUNIFORMLOCATIONPROC)wglGetProcAddress("glGetUniformLocation"))(m_prg, name);
		if (id != -1)
			((PFNGLPROGRAMUNIFORM1FEXTPROC)wglGetProcAddress("glProgramUniform1f"))(m_prg, id, v);
	}
}


driverOGL::driverOGL()
{
	m_time = 0;
	m_shader = 0;
}

driverOGL::~driverOGL()
{
}

bool driverOGL::install(void* windowHandle)
{
	const HWND hWnd = static_cast<HWND>(windowHandle);
	const HDC hDC = ::GetDC(hWnd);
	m_hWnd = hWnd;
	
	static const PIXELFORMATDESCRIPTOR pfd = {
		0,1,PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER, 32, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, 0, 0, 0, 0, 0
	};

	SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd) , &pfd);
	wglMakeCurrent(hDC, wglCreateContext(hDC));
	
	BitmapFontGL::GetInstance().CreateTexture();
	//ShowCursor(0);
	//LoadShaders();

	ReleaseDC(hWnd, hDC);

	return true;
}

bool driverOGL::uninstall()
{
	if (m_shader)
	{
		delete m_shader;
	}
	HGLRC hRC = wglGetCurrentContext();
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hRC);

	return true;
}

shaderPure* driverOGL::compileShader(const char* shaderbuffer) const
{
	return new shaderOGL(shaderbuffer);
}
	
const char* driverOGL::getShaderName() const
{
	return "shader.glsl";
}

void driverOGL::setShader(shaderPure* shader)
{
	m_shader = static_cast<shaderOGL*>(shader);
}

void driverOGL::setTime(float t)
{
	m_time = t;
}

void driverOGL::render()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();

	if (m_shader)
	{
		m_shader->bind();
		m_shader->setUniform("t", m_time);
		m_shader->setUniform("w", static_cast<float>(m_width));
		m_shader->setUniform("h", static_cast<float>(m_height));
		glRecti(1,1,-1,-1);
		m_shader->unbind();
	}

	glPushMatrix();
	glTranslatef(-0.98f,0.98f,0);
	BitmapFontGL::GetInstance().DrawString(Logger::GetInstance().GetBuffer(), m_width/static_cast<float>(m_height), 1920/2);
	glPopMatrix();
}

void driverOGL::resize(int w, int h)
{
	m_width  = w;
	m_height = h;
	glViewport(0,0,w,h);
}

void driverOGL::swapbuffer()
{
	const HWND hWnd = m_hWnd;
	HDC hdc = GetDC(hWnd);
	SwapBuffers(hdc);
	ReleaseDC(hWnd, hdc);
}

