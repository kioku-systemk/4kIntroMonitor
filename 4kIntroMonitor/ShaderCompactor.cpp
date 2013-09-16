//#include "4kGfxMon_driver.h"
#include "Util.h"

void commentDelete(const char* src, char* dest, unsigned long src_size)
{
	unsigned long d = 0;
	const unsigned long end_s = src_size;
	for (unsigned long s = 0; s < end_s; s++)
	{
		if (src[s] == '/' && src[s+1] == '/')
		{
			while(src[++s] != 0x0D || src[++s] != 0x0A); // skip
		}
		dest[d++] = src[s];
	}
}

void commentDeleteC(const char* src, char* dest, unsigned long src_size)
{
	unsigned long d = 0;
	const unsigned long end_s = src_size;
	int cnt = 0;
	for (unsigned long s = 0; s < end_s; s++)
	{
		if (src[s] == '/' && src[s+1] == '*')
		{
			cnt++;
			s+=2;
			//while(src[++s] != 0x0D || src[++s] != 0x0A); // skip
		}
		if (src[s] == '*' && src[s+1] == '/')
		{
			cnt--;
			s+=2;
		}

		if (cnt < 0)
			cnt = 0;

		if (cnt==0)
			dest[d++] = src[s];
	}
}


void toSingleSpace(char* src, char* dest, unsigned long src_size)
{
	unsigned long d = 0;
	const unsigned long end_s = src_size;
	for (unsigned long s = 0; s < end_s; s++)
	{
		if (src[s] == ' ')
		{
			while(src[++s] == ' '); // skip
			s--;
		}
		dest[d++] = src[s];
	}
}

void replaceChar(char* src, char s, char d)
{
	const char* end_src = src + lstrlen(src);
	while(end_src != src)
	{
		if (*src == s)
			*src = d;
		src++;
	}
}

bool isAlphabet(char c)
{
	if ((c >= 'A' && c <= 'Z')
	||  (c >= 'a' && c <= 'z'))
		return true;
	else
		return false;
}

bool isNumber(char c)
{
	if (c >= '0' && c <= '9')
		return true;
	else
		return false;
}
void spaceCheck(char* src, char* dest, unsigned long src_size)
{
	unsigned long d = 0;
	const unsigned long end_s = src_size;
	for (unsigned long s = 0; s < end_s; s++)
	{
		if (src[s] == ' ')
		{
			if (!((isAlphabet(src[s+1]) || isNumber(src[s+1]))
			&&    (isAlphabet(src[s-1]) || isNumber(src[s-1]))))
				s++;
		}
		dest[d++] = src[s];
	}
}

void compactionSrc(const char* src, char*& dest, unsigned long& dest_size)
{
	unsigned long src_size = lstrlen(src) + 1;
	char* tmpOrg = reinterpret_cast<char*>(tmalloc(src_size));
	// コメント削除 ( "//"検出 -> 0x0D,0x0Aまで削除 )
	commentDelete(src, tmpOrg, src_size);

	// Cコメント削除
	unsigned long src_size_c = lstrlen(tmpOrg) + 1;
	char* tmp = reinterpret_cast<char*>(tmalloc(src_size_c));
	commentDeleteC(tmpOrg, tmp, src_size_c);
	

	// 0x09,0x0A,0x0D to 0x20 = " "
	replaceChar(tmp, 0x09, 0x20);
	replaceChar(tmp, 0x0A, 0x20);
	replaceChar(tmp, 0x0D, 0x20);
	
	// search double space to single space ( if "  " replace " " )
	unsigned long tmp_size = lstrlen(tmp) + 1;
	char* tmp2 = reinterpret_cast<char*>(tmalloc(tmp_size));
	toSingleSpace(tmp, tmp2, tmp_size);
	
	// if 英字もしくは数字の間ならスペースは残す
	// or if スペース前後どちらかが記号（英字と数字以外）ならスペースは削除する
	unsigned long tmp2_size = lstrlen(tmp2) + 1;
	char* tmp3 = reinterpret_cast<char*>(tmalloc(tmp2_size));
	spaceCheck(tmp2, tmp3, tmp2_size);
	
	tfree(tmp);
	tfree(tmp2);
	tfree(tmpOrg);
	
	dest = tmp3;
	dest_size = lstrlen(tmp3);
}

void wrapCompactionSrc(char** new_shader/*[driver::SHADER_NUM]*/, unsigned long* shadersize/*[driver::SHADER_NUM]*/)
{
	// TODO
	//opt_memset(new_shader, 0, sizeof(char*) * driver::SHADER_NUM);
	//opt_memset(shadersize, 0, sizeof(unsigned long) * driver::SHADER_NUM);
//	for(int i=0; i<driver::SHADER_NUM; ++i){
//		const char* shaderSource = gDriver->getShaderSource(static_cast< driver::eShaderType >(i));
//		if( shaderSource ){
//			compactionSrc(shaderSource, new_shader[i], shadersize[i]);
//		}
//	}
}

bool cmpStrN(const char* src, const char* keyStr, unsigned long keySize)
{
	for (unsigned long i = 0; i < keySize; i++)
	{
		if (src[i] != keyStr[i])
			return false;
	}
	return true;
}

void replaceStr(const char* src, char* dest, const char* searchStr, const char* repStr)
{
	unsigned long d = 0;
	const unsigned long end_s = static_cast<unsigned long>(lstrlen(src));
	const unsigned long keySize = lstrlen(searchStr);
	for (unsigned long s = 0; s < end_s; s++)
	{
		if (cmpStrN(&src[s], searchStr, keySize))
		{
			const unsigned long repSize = lstrlen(repStr);
			for(unsigned long r = 0; r < repSize; r++)
			{
				dest[d++] = repStr[r];
			}
			s += keySize;
		}
		dest[d++] = src[s];
	}
}

bool output_src(const char** shaderSource/*[driver::SHADER_NUM]*/, const char* filename)
{
	HANDLE f = OpenFile(filename);
	if (f != INVALID_HANDLE_VALUE)
	{
		unsigned long templatesize;
		char* templatebuf;
		ReadFileToBuffer(f, templatebuf, templatesize);
		CloseFile(f);
		
		unsigned long exp_size = templatesize /*+ lstrlen(new_vsh) + lstrlen(new_fsh)*/ + 4 + 1;//FIXME なぜ +4 している？
// TODO:
//		for(int i=0; i<driver::SHADER_NUM; ++i){
//			if( shaderSource[i] ){
//				exp_size += lstrlen(shaderSource[i]);
//			}
//		}

		char* tmpbuf1 = reinterpret_cast<char*>(tmalloc(exp_size));
		char* tmpbuf2 = reinterpret_cast<char*>(tmalloc(exp_size));
		char* expbuf = NULL;

		char* buf1 = tmpbuf1;
		char* buf2 = tmpbuf2;
		lstrcpy(buf1, templatebuf);

// TODO:
		//for(int i=0; i<driver::SHADER_NUM; ++i){
		//	if( shaderSource[i] ){
		//		replaceStr(buf1, buf2, driver::getReplaceNameFromType(static_cast< driver::eShaderType >(i)), shaderSource[i]);
		//		expbuf = buf2;

		//		// swap buffer
		//		char* tmp = buf1;
		//		buf1 = buf2;
		//		buf2 = tmp;
		//	}
		//}
		
		bool r = WriteToFile("template.cpp", expbuf);
		
		tfree(templatebuf);
		tfree(tmpbuf1);
		tfree(tmpbuf2);
		return r;	
	}
	return false;
}
