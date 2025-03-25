#include "lib.hpp"
#include "nn/fs.hpp"

#ifndef WEAK
#define WEAK __attribute__((weak))
#endif

namespace nn { namespace codec {
	void* FDKfopen(const char * sNazwaPliku, const char * sTryb ) WEAK;
	void FDKfclose(void * stream) WEAK;
}}

HOOK_DEFINE_TRAMPOLINE(CreateFileStruct) {

    static void Callback(void* x0, char** path) {
		char file_path[512] = "rom:/mod";
		if (path[0][0] != '/')
			strcat(file_path, "/");
		strncat(file_path, path[0], 503);
		void* file = nn::codec::FDKfopen(file_path, "rb");
		if (!file)
			return Orig(x0, path);
		else {
			nn::codec::FDKfclose(file);
			char* new_path = new char[512]();
			strcpy(new_path, "/mod");
			if (path[0][0] != '/')
				strcat(new_path, "/");
			strncat(new_path, path[0], 506);
			path[0] = &new_path[0];
			Orig(x0, path);
			delete[] new_path;
			return;
		}
    }
};

namespace nn::fs {
    /*
        If not set to true, instead of returning result with error code
        in case of any fs function failing, Application will abort.
    */
    Result SetResultHandledByApplication(bool enable);
};

/* Define hook StubCopyright. Trampoline indicates the original function should be kept. */
/* HOOK_DEFINE_REPLACE can be used if the original function does not need to be kept. */

extern "C" void exl_main(void* x0, void* x1) {
	/* Setup hooking enviroment. */
	nn::fs::SetResultHandledByApplication(true);
	exl::hook::Initialize();
	CreateFileStruct::InstallAtOffset(0x13C5710);

	/* Install the hook at the provided function pointer. Function type is checked against the callback function. */
}

extern "C" NORETURN void exl_exception_entry() {
	/* TODO: exception handling */
	EXL_ABORT(0x420);
}
