
void commentDelete(const char* src, char* dest, unsigned long src_size);
void commentDeleteC(const char* src, char* dest, unsigned long src_size);
void toSingleSpace(char* src, char* dest, unsigned long src_size);
void replaceChar(char* src, char s, char d);
bool isAlphabet(char c);
bool isNumber(char c);
void spaceCheck(char* src, char* dest, unsigned long src_size);
void compactionSrc(const char* src, char*& dest, unsigned long& dest_size);
void wrapCompactionSrc(char** new_shader/*[driver::SHADER_NUM]*/, unsigned long* shadersize/*[driver::SHADER_NUM]*/);
bool cmpStrN(const char* src, const char* keyStr, unsigned long keySize);
void replaceStr(const char* src, char* dest, const char* searchStr, const char* repStr);
bool output_src(const char** shaderSource/*[driver::SHADER_NUM]*/, const char* filename);
