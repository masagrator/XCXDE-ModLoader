#include "lib.hpp"
#include "nn/fs.hpp"
#include "xxhash.h"
#include <algorithm>
#ifdef XCXDEBUG
#include "nn/os.hpp"
#include <cstdlib>
#endif

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

template <typename T>
class BucketSorter {
private:
	T** m_buckets = 0;
	size_t* m_amount = 0;
	size_t m_count = 0;
public:
	BucketSorter(T* hashes, size_t size, uint8_t bits_to_and) {
		m_count = 1 << bits_to_and;
		size_t counts[m_count] = {0};
		for (size_t i = 0; i < size; i++) {
			counts[hashes[i] & (m_count - 1)]++;
		}
		m_buckets = (T**)nnutilZlib_zcalloc(nullptr, sizeof(T*), m_count);
		for (size_t i = 0; i < m_count; i++) {
			m_buckets[i] = (T*)nnutilZlib_zcalloc(nullptr, sizeof(T), counts[i]);
		}
		m_amount = (T*)nnutilZlib_zcalloc(nullptr, sizeof(size_t), m_count);
		memcpy(m_amount, counts, sizeof(counts));
		memset(counts, 0, sizeof(counts));
		for (size_t i = 0; i < size; i++) {
			m_buckets[hashes[i] & (m_count - 1)][counts[hashes[i] & (m_count - 1)]++] = hashes[i];
		}
	}
	~BucketSorter() {
		for (size_t i = 0; i < m_count; i++) {
			nnutilZlib_zcfree(nullptr, m_buckets[i]);
		}
		nnutilZlib_zcfree(nullptr, m_amount);
	}

	const bool find(T hash) {
		return std::binary_search(&m_buckets[hash & (m_count - 1)][0], &m_buckets[hash & (m_count - 1)][m_amount[hash & (m_count - 1)]], hash);
	}
};

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
		nn::fs::DirectoryEntry* entryBuffer = (nn::fs::DirectoryEntry*)nnutilZlib_zcalloc(nullptr, sizeof(nn::fs::DirectoryEntry), file_count);
		r = nn::fs::ReadDirectory(&file_count, entryBuffer, rootHandle, file_count);
		if (R_FAILED(r)) {
			nnutilZlib_zcfree(nullptr, entryBuffer);
			return r;
		}
		for (s64 i = 0; i < file_count; i++) {
			std::string final_path = path;
			final_path += entryBuffer[i].m_Name;
			final_path = final_path.erase(0, strlen("rom:/mod"));
			hashes[*count] = XXH64(final_path.c_str(), final_path.length(), 0);
			*count += 1;
		}
		nnutilZlib_zcfree(nullptr, entryBuffer);
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
	nn::fs::DirectoryEntry* entryBuffer = (nn::fs::DirectoryEntry*)nnutilZlib_zcalloc(nullptr, sizeof(nn::fs::DirectoryEntry), dir_count);
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
	nnutilZlib_zcfree(nullptr, entryBuffer);
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

BucketSorter<XXH64_hash_t>* hash_bucket = 0;

HOOK_DEFINE_TRAMPOLINE(CreateFileStruct) {

    static void Callback(void* x0, char** path) {
		static bool initialized = false;
		#ifdef XCXDEBUG
		static void* file = 0;
		#endif
		if (!initialized) {
			#ifdef XCXDEBUG
			nn::fs::MountSdCardForDebug("sdmc");
			file = nn::codec::FDKfopen("sdmc:/XCX_DEBUG.txt", "w");
			nn::codec::FDKfprintf(file, "DEBUG INITIALIZED.\n");
			#endif
			char file_path[] = "rom:/mod/";
			initialized = true;
			u64 file_count = 0;
			Result res = countFiles(&file_count, file_path);
			if (R_SUCCEEDED(res) && file_count) {
				#ifdef XCXDEBUG
				u64 orig_file_count = file_count;
				file_count = 104444;
				#endif
				XXH64_hash_t* hashes = (XXH64_hash_t*)nnutilZlib_zcalloc(nullptr, sizeof(XXH64_hash_t), file_count);
				if (R_SUCCEEDED(hashFilePaths(file_path, hashes))) {
					#ifdef XCXDEBUG
					for (size_t i = orig_file_count; i < file_count; i++) {
						hashes[i] = rand();
					}
					#endif
					std::sort(&hashes[0], &hashes[file_count]);
					hash_bucket = new BucketSorter(hashes, file_count, 8);
				}
				nnutilZlib_zcfree(nullptr, hashes);
			}
		}
		bool found = false;
		if (hash_bucket) {
			#ifdef XCXDEBUG
			nn::os::Tick start = nn::os::GetSystemTick();
			#endif
			XXH64_state_t* state = XXH64_createState();
			XXH64_reset(state, 0);
			if (path[0][0] != '/') {
				XXH64_update(state, "/", 1);
			}
			XXH64_update(state, path[0], strlen(path[0]));
			XXH64_hash_t hashCmp = XXH64_digest(state);
			XXH64_freeState(state);
			found = hash_bucket -> find(hashCmp);
			#ifdef XCXDEBUG
			nn::os::Tick end = nn::os::GetSystemTick();
			nn::codec::FDKfprintf(file, "Ticks: %d.\n", end-start);
			#endif
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

extern "C" void exl_main(void* x0, void* x1) {
	/* Setup hooking enviroment. */
	exl::hook::Initialize();
	//REF: 7F E2 00 F9 7F EA 00 F9 7F DA 01 B9 7F E6 07 39
	CreateFileStruct::InstallAtOffset(0x13C5710);
}

extern "C" NORETURN void exl_exception_entry() {
	/* TODO: exception handling */
	EXL_ABORT(0x420);
}