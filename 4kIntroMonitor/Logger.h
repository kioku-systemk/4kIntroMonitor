
class Logger
{
	private:
		unsigned long bufferSize;
		char* buffer;
		
		Logger()
		{
			bufferSize = 0;
			buffer = NULL;
		}
		~Logger()
		{
			if (buffer)
			{
				bufferSize = 0;
				tfree(buffer);
			}
		}	

		void PushString(const char* str)
		{
			unsigned int strsize = lstrlen(str);
			unsigned int bufstrsize = lstrlen(buffer);
			if (bufstrsize + strsize > bufferSize)
			{
				while(bufstrsize + strsize >= bufferSize)
					bufferSize += 64 * 1024;
					
				char* newBuffer = (char*)tmalloc(bufferSize);
				lstrcpy(newBuffer, buffer);
				tfree(buffer);
				buffer = newBuffer;
			}
			lstrcat(buffer, str);
		}
public:
		static Logger& GetInstance()
		{
			static Logger* singleton = NULL;
			if (singleton == NULL)
				singleton = new Logger();
			return *singleton;
		}
		
		void OutputStringD(const char* str)
		{
			PushString(str);
		}
		void OutputString(const char* format, ...)
		{
			char* pBuffer = static_cast< char* >(tmalloc(8192));
			opt_memset(pBuffer, 0, 8192);

			va_list args;
			va_start(args, format);
			wvsprintf(pBuffer, format, args);
			va_end(args);

			OutputDebugStringA(pBuffer);//TEMP
			PushString(pBuffer);

			tfree(pBuffer);
		}

		void Clear()
		{
			if (buffer)
			{
				buffer[0] = '\0';
			}
		}
		
		const char* GetBuffer()
		{
			return buffer;
		}
};
