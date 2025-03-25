#include "lib.hpp"
#include "nn/fs.hpp"

#ifndef WEAK
#define WEAK __attribute__((weak))
#endif

namespace nn { namespace codec {
	void* FDKfopen(const char * sNazwaPliku, const char * sTryb ) WEAK;
	int FDKfclose(void * stream) WEAK;
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
			char* old_path = path[0];
			path[0] = &new_path[0];
			Orig(x0, path);
			delete[] new_path;
			path[0] = old_path;
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

extern "C" void exl_main(void* x0, void* x1) {
	/* Setup hooking enviroment. */
	nn::fs::SetResultHandledByApplication(true);
	exl::hook::Initialize();
	//REF: 7F E2 00 F9 7F EA 00 F9 7F DA 01 B9 7F E6 07 39
	CreateFileStruct::InstallAtOffset(0x13C5710);
}

extern "C" NORETURN void exl_exception_entry() {
	/* TODO: exception handling */
	EXL_ABORT(0x420);
}