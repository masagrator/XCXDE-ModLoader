#include "lib.hpp"
#include "nn/fs.hpp"
#include "nn/os.hpp"
#include "xxhash.h"
#include <algorithm>

#ifndef WEAK
#define WEAK __attribute__((weak))
#endif

extern "C" {
	void* nnutilZlib_zcalloc(void* put_nullptr, int size, int nmemb) WEAK;
	void* nnutilZlib_zcfree(void* put_nullptr, void* pointer) WEAK;
}

namespace nn { namespace codec {
	void* FDKfopen(const char * sNazwaPliku, const char * sTryb ) WEAK;
	size_t FDKfprintf(void* file, const char* string, ...) WEAK;
	int FDKfclose(void * stream) WEAK;
}}



Result countFilesRecursive(u64* count, std::string path, XXH64_hash_t* hashes) {
	nn::fs::DirectoryHandle rootHandle;
	R_TRY(nn::fs::OpenDirectory(&rootHandle, path.c_str(), nn::fs::OpenDirectoryMode_File));
	s64 file_count = 0;
	Result r = nn::fs::GetDirectoryEntryCount(&file_count, rootHandle);
	if (R_FAILED(r)) {
		nn::fs::CloseDirectory(rootHandle);
		return r;
	}
	if (file_count && hashes) {
		nn::fs::DirectoryEntry* entryBuffer = new nn::fs::DirectoryEntry[file_count];
		r = nn::fs::ReadDirectory(&file_count, entryBuffer, rootHandle, file_count);
		if (R_FAILED(r)) {
			delete[] entryBuffer;
			return r;
		}
		for (s64 i = 0; i < file_count; i++) {
			std::string final_path = path;
			final_path += entryBuffer[i].m_Name;
			final_path = final_path.erase(0, strlen("rom:/mod"));
			hashes[*count++] = XXH64(final_path.c_str(), final_path.length(), 0);
		}
		delete[] entryBuffer;
	}
	else *count += file_count;
	nn::fs::CloseDirectory(rootHandle);
	R_TRY(nn::fs::OpenDirectory(&rootHandle, path.c_str(), nn::fs::OpenDirectoryMode_Directory));
	s64 dir_count = 0;
	r = nn::fs::GetDirectoryEntryCount(&dir_count, rootHandle);
	if (R_FAILED(r) || !dir_count) {
		nn::fs::CloseDirectory(rootHandle);
		return r;
	}
	nn::fs::DirectoryEntry* entryBuffer = new nn::fs::DirectoryEntry[dir_count];
	r = nn::fs::ReadDirectory(&dir_count, entryBuffer, rootHandle, dir_count);
	nn::fs::CloseDirectory(rootHandle);
	if (R_FAILED(r)) dir_count = 0;
	else for (s64 i = 0; i < dir_count; i++) {
		std::string next_path = path;
		next_path += entryBuffer[i].m_Name;
		next_path += "/";
		r = countFilesRecursive(count, next_path, hashes);
		if (R_FAILED(r)) break;
	}
	delete[] entryBuffer;
	return r;
}

Result countFiles(u64* out, const char* path) {
	u64 file_count = 0;
	std::string str_path = path;
	Result r = countFilesRecursive(&file_count, str_path, nullptr);
	if (R_SUCCEEDED(r))
		*out = file_count;
	return r;
}

Result hashFilePaths(const char* path, XXH64_hash_t* hashes) {
	u64 file_count = 0;
	std::string str_path = path;
	return countFilesRecursive(&file_count, str_path, hashes);
}

XXH64_hash_t* hashes = 0;

HOOK_DEFINE_TRAMPOLINE(CreateFileStruct) {

    static void Callback(void* x0, char** path) {
		char file_path[512] = "rom:/mod/";
		static bool initialized = false;
		static u64 final_file_count = 0;
		if (!initialized) {
			u64 file_count = 0;
			Result res = countFiles(&file_count, file_path);
			if (R_SUCCEEDED(res) && file_count) {
				hashes = (XXH64_hash_t*)nnutilZlib_zcalloc(nullptr, file_count, sizeof(XXH64_hash_t));
				if (R_SUCCEEDED(hashFilePaths(file_path, hashes))) {
					std::sort(&hashes[0], &hashes[file_count]);
					final_file_count = file_count;
				}
				else nnutilZlib_zcfree(nullptr, hashes);
			}
			initialized = true;
		}
		bool found = false;
		if (final_file_count) {
			XXH64_state_t* state = XXH64_createState();
			XXH64_reset(state, 0);
			if (path[0][0] != '/') {
				XXH64_update(state, "/", 1);
			}
			XXH64_update(state, path[0], strlen(path[0]));
			XXH64_hash_t hashCmp = XXH64_digest(state);
			XXH64_freeState(state);
			found = std::binary_search(&hashes[0], &hashes[final_file_count], hashCmp);
		}
		if (!found) return Orig(x0, path);
		char new_path[512] = "/mod/";
		if (path[0][0] == '/')
			new_path[4] = 0;
		strncat(new_path, path[0], 506);
		char* old_path = path[0];
		path[0] = &new_path[0];
		Orig(x0, path);
		path[0] = old_path;
		return;
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