#include "Engine.h"

#include <exception>
#include <Windows.h>

#include "exception/StimplyExceptionBase.h"

int main() {
#ifdef _DEBUG
	// PLEASE only use this in Debug, as it's a major security concern.
	system(".\\Shaders\\build_debug.bat");
#endif
	try
	{
		Engine engine(1800, 1000);
		engine.Run();
	}
	catch (const StimplyExceptionBase& e)
	{
		MessageBoxA(nullptr, e.what(), e.get_exception_type(), MB_OK | MB_ICONEXCLAMATION);
	}
	catch (std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONEXCLAMATION);
	}
	
	return 0;
}