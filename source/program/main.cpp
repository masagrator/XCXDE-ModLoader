#include "lib.hpp"
#include "nn/fs.hpp"

#ifndef WEAK
#define WEAK __attribute__((weak))
#endif

namespace nn { namespace codec {
	void* FDKfopen(const char * sNazwaPliku, const char * sTryb ) WEAK;
	size_t FDKfclose(void * stream) WEAK;
}}

char new_file_path[512] = "";

HOOK_DEFINE_TRAMPOLINE(CheckFile) {

    static bool Callback(void* x0, int* x1, const char* path, int w3, int w4) {
		char file_path[512] = "rom:/mod";
		strncat(file_path, path, 503);
		void* file = nn::codec::FDKfopen(file_path, "rb");
		if (!file)
			return Orig(x0, x1, path, w3, w4);
		else {
			nn::codec::FDKfclose(file);
			strncpy(new_file_path, "/mod", 5);
			strncat(new_file_path, path, 507);
			return Orig(x0, x1, new_file_path, w3, w4);
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
	CheckFile::InstallAtOffset(0x13C90D4);

	/* Install the hook at the provided function pointer. Function type is checked against the callback function. */
}

extern "C" NORETURN void exl_exception_entry() {
	/* TODO: exception handling */
	EXL_ABORT(0x420);
}