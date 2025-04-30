#include "lib.hpp"
#include "nn/fs.hpp"
#include <algorithm>
#include <array>
#include "xxhash.h"
#ifdef XCXDEBUG
#include "nn/os.hpp"
#include <cstdlib>
#endif

#ifndef WEAK
#define WEAK __attribute__((weak))
#endif

extern "C" {
	void* nnutilZlib_zcalloc(void* put_nullptr, int size, int nmemb) WEAK;
	void nnutilZlib_zcfree(void* put_nullptr, void* pointer) WEAK;
}

namespace nn { 
	namespace codec {
		void* FDKfopen(const char * sNazwaPliku, const char * sTryb ) WEAK;
		size_t FDKfread(void* buffer, int size, uint count, void* stream) WEAK;
		size_t FDKfprintf(void* file, const char* string, ...) WEAK;
		int FDKfseek(void* stream, int offset, int origin) WEAK;
		long FDKftell(void* stream) WEAK;
		int FDKfclose(void * stream) WEAK;
		int FDKatoi(const char* string) WEAK;
	}

	namespace util {
		int SNPrintf(char* s, size_t n, const char* format, ...) WEAK;
	}
}

template <typename T>
requires (std::is_arithmetic_v<T>)
class BucketSortedArray {
private:
	std::make_signed_t<T>* m_start = 0;
	size_t m_count = 0;
	size_t m_negabits = 0;
	std::make_unsigned_t<T>* m_hashes = 0;
	bool valid = false;
	
	//We are using here functions provided by nnSDK that are wrappers around malloc() and free() to get access to game's heap.
	//This is to avoid situations where some libraries are including malloc() and free() which results in using plugin's own heap.
	constexpr void* Allocate(size_t size, size_t nmemb) {
		return nnutilZlib_zcalloc(nullptr, size, nmemb);
	}

	constexpr void Unallocate(void* ptr) {
		return nnutilZlib_zcfree(nullptr, ptr);
	}
public:
	BucketSortedArray(T* hashes, const size_t size, uint8_t bits_to_and) {
		if (!hashes || !size) return;
		m_hashes = (std::make_unsigned_t<T>*)Allocate(sizeof(T), size);
		if (!m_hashes) return;
		std::copy(&hashes[0], &hashes[size], m_hashes);
		std::sort(&m_hashes[0], &m_hashes[size]);
		//Flatten out m_count since anything above amount of bits T represents won't be used anyway
		if (bits_to_and > sizeof(T) * 8)
			bits_to_and = sizeof(T) * 8;
		m_count = 1 << bits_to_and;
		m_start = (std::make_signed_t<T>*)Allocate(sizeof(std::make_signed_t<T>), m_count+1);
		if (!m_start) return;
		memset(m_start, -1, sizeof(std::make_signed_t<T>) * (m_count+1));
		m_negabits = (sizeof(T) * 8) - bits_to_and;
		for (size_t i = 0; i < size; i++) {
			size_t index = (m_hashes[i] >> m_negabits) & (m_count - 1);
			if (m_start[index] == -1) {
				m_start[index] = i;
			}
		}
		m_start[m_count] = size;
		std::make_signed_t<T> last_good_end = 0;
		for (size_t i = m_count; i > 0; i--) {
			if (m_start[i] != -1) last_good_end = m_start[i];
			else m_start[i] = last_good_end * -1;
		}
		valid = true;
	}
	~BucketSortedArray() {
		if (m_start)
			Unallocate(m_start);
		if (m_hashes)
			Unallocate(m_hashes);
	}

	const bool find(T hash) {
		size_t index = ((std::make_unsigned_t<T>)hash >> m_negabits) & (m_count - 1);
		if (m_start[index] < 0) return false;
		return std::binary_search(&m_hashes[m_start[index]], &m_hashes[std::abs(m_start[index+1])], hash);
	}

	//Use to check if constructed class was created properly since this class avoids using exceptions
	const bool isValid() {
		return valid;
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

BucketSortedArray<XXH64_hash_t>* hash_bucket = 0;
BucketSortedArray<uint32_t>* music_bucket = 0;

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
					hash_bucket = new BucketSortedArray(hashes, file_count, 8);
					if (hash_bucket -> isValid() == false) {
						delete hash_bucket;
						hash_bucket = 0;
					}
				}
				nnutilZlib_zcfree(nullptr, hashes);
			}
			nn::fs::DirectoryHandle rootHandle;
			nn::fs::OpenDirectory(&rootHandle, "rom:/sound/", nn::fs::OpenDirectoryMode_File);
			s64 fsfile_count = 0;
			Result r = nn::fs::GetDirectoryEntryCount(&fsfile_count, rootHandle);
			if (R_FAILED(r)) {
				nn::fs::CloseDirectory(rootHandle);
			}
			else {
				nn::fs::DirectoryEntry* entryBuffer = (nn::fs::DirectoryEntry*)nnutilZlib_zcalloc(nullptr, sizeof(nn::fs::DirectoryEntry), fsfile_count);
				r = nn::fs::ReadDirectory(&fsfile_count, entryBuffer, rootHandle, fsfile_count);
				nn::fs::CloseDirectory(rootHandle);
				if (R_SUCCEEDED(r)) {
					size_t wem_count = 0;
					for (s64 i = 0; i < fsfile_count; i++) {
						std::string filename = entryBuffer[i].m_Name;
						if (filename.ends_with(".wem") == true)
							wem_count += 1;
					}
					if (wem_count) {
						uint32_t* wem_hashes = (uint32_t*)nnutilZlib_zcalloc(nullptr, sizeof(uint32_t), wem_count);
						size_t wem_hashes_count = 0;
						for (s64 i = 0; i < fsfile_count; i++) {
							std::string filename = entryBuffer[i].m_Name;
							if (filename.ends_with(".wem") == true) {
								filename = filename.substr(0, filename.length() - 4);
								wem_hashes[wem_hashes_count++] = nn::codec::FDKatoi(filename.c_str());
							}
						}
						music_bucket = new BucketSortedArray(wem_hashes, wem_count, 8);
						if (music_bucket -> isValid() == false) {
							delete music_bucket;
							music_bucket = 0;
						}
						nnutilZlib_zcfree(nullptr, wem_hashes);
					}
				}
				nnutilZlib_zcfree(nullptr, entryBuffer);
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
			static nn::os::Tick average = nn::os::Tick(0);
			static size_t average_count = 0;
			average = nn::os::Tick(((average.GetInt64Value() * average_count) + (end-start).GetInt64Value()) / (average_count+1));
			average_count++;
			nn::codec::FDKfprintf(file, "Ticks average: %d\n", average);
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

struct WemMeta {
	bool LoadExternal;
};

HOOK_DEFINE_TRAMPOLINE(LoadWEMFile) {

    static int Callback(void* x0, uint32_t hash_name, int w2, WemMeta* x3, void* x4, void* x5) {
		if (music_bucket && music_bucket -> find(hash_name))
			x3 -> LoadExternal = true;
		return Orig(x0, hash_name, w2, x3, x4, x5);
    }
};

int getWemRiffLength(uint32_t hash30, double* duration, double* startLoop) {
	char filepath[128] = "";
	nn::util::SNPrintf(filepath, sizeof(filepath), "rom:/sound/%u.wem", hash30);
	void* file = nn::codec::FDKfopen(filepath, "rb");
	if (!file)
		return 1;
	uint32_t magic = 0;
	nn::codec::FDKfread(&magic, 4, 1, file);
	if (magic != 0x46464952) {
		nn::codec::FDKfclose(file);
		return 1;
	}
	nn::codec::FDKfseek(file, 0x14, 0);
	uint16_t codec = 0;
	nn::codec::FDKfread(&codec, 2, 1, file);
	if (codec != 12345) {
		nn::codec::FDKfclose(file);
		return 1;
	}
	nn::codec::FDKfseek(file, 0x2C, 0);
	uint32_t time = 0;
	nn::codec::FDKfread(&time, 4, 1, file);
	nn::codec::FDKfseek(file, 0x3C, 0);
	nn::codec::FDKfread(&magic, 4, 1, file);
	if (magic == 0x6173616D) {
		nn::codec::FDKfseek(file, 8, 1);
		nn::codec::FDKfread(startLoop, 8, 1, file);
	}
	nn::codec::FDKfclose(file);
	*duration = (double)time / 48.0;
	return 0;
}

HOOK_DEFINE_TRAMPOLINE(ParseHIRC) {

    static int Callback(void* x0, void* data, int w2, void* x3, int w4) {
		static bool initialized = false;
		if (initialized || !music_bucket || !data)
			return Orig(x0, data, w2, x3, w4);
		uintptr_t ptr = (uintptr_t)data;
		ptr &= ~0xFFFF;
		uint32_t bank_hash = *(uint32_t*)(ptr + 0xC);
		if (bank_hash != 0x1899AC8D)
			return Orig(x0, data, w2, x3, w4);
		initialized = true;
		uint32_t HIRC_Magic = *(uint32_t*)(ptr + 0x20);
		if (HIRC_Magic != 0x43524948)
			return Orig(x0, data, w2, x3, w4);
		uint32_t entry_count = *(uint32_t*)(ptr + 0x28);
		ptr += 0x28;
		uint32_t MusicSegmentID = 0;
		double srcDuration = -1.0;
		double startLoop = -1.0;
		for (uint32_t i = 0; i < entry_count; i++) {
			char type = (*(char*)(ptr++));
			uint32_t size = (*(uint32_t*)ptr);
			ptr += 4;
			uintptr_t next_ptr = ptr + size;
			if (type == 0xB) {
				startLoop = -1.0;
				srcDuration = -1.0;
				MusicSegmentID = 0;
				ptr += 5;
				uint32_t numSources = (*(uint32_t*)ptr);
				ptr += 4;
				ptr += numSources * 14;
				uint32_t numPlaylistItems = (*(uint32_t*)ptr);
				ptr += 4;
				for (uint32_t x = 0; x < numPlaylistItems; x++) {
					ptr += 4;
					uint32_t hash_filename = (*(uint32_t*)ptr);
					ptr += 32;
					if (music_bucket -> find(hash_filename)) {
						int Result = getWemRiffLength(hash_filename, &srcDuration, &startLoop);
						if (!Result)
							*(double*)ptr = srcDuration;
					}
					ptr += 8;
				}
				ptr += 17;
				MusicSegmentID = (*(uint32_t*)ptr);
			}
			else if (type == 0xA) {
				uint32_t ID = (*(uint32_t*)(ptr));
				if (ID == MusicSegmentID) {
					ptr += 19;
					uint8_t numProps = (*(uint8_t*)(ptr++));
					ptr += 5 * numProps;
					numProps = (*(uint8_t*)(ptr++));
					ptr += 5 * numProps;
					if ((*(uint8_t*)(ptr++)) == 3)
						ptr++;
					ptr += 11;
					uint8_t numStateProps = (*(uint8_t*)(ptr++));
					ptr += 5 * numStateProps;
					uint8_t numStateGroups = (*(uint8_t*)(ptr++));
					ptr += 5 * numStateGroups;
					uint16_t numRTPC = (*(uint16_t*)(ptr));
					ptr += 2;
					ptr += 5 * numRTPC;
					ptr += 31;
					uint32_t numStingers = (*(uint32_t*)(ptr));
					ptr += 4;
					ptr += 5 * numStingers;
					if (srcDuration != -1.0) (*(double*)(ptr)) = srcDuration;
					uint32_t numMarkers = (*(uint32_t*)(ptr));
					ptr += 4;
					for (size_t i = 0; i < numMarkers; i++) {
						uint32_t markerID = (*(uint32_t*)(ptr));
						ptr += 4;
						if (markerID == 43573010 && startLoop != -1.0) *(double*)(ptr) = startLoop;
						else if (markerID == 1539036744 && srcDuration != 1.0) *(double*)(ptr) = srcDuration;
						ptr += 9;
					}
				}
			}
			ptr = next_ptr;
		}
		return Orig(x0, data, w2, x3, w4);
    }
};

extern "C" void exl_main(void* x0, void* x1) {
	/* Setup hooking enviroment. */
	exl::hook::Initialize();
	uint8_t pattern[] = {0xFD, 0x7B, 0xBC, 0xA9, 0xF7, 0x0B, 0x00, 0xF9, 0xF6, 0x57, 0x02, 0xA9, 0xF4, 0x4F, 0x03, 0xA9, 0xFD, 0x03, 0x00, 0x91, 0x28, 0x20, 0x40, 0xF9};
	//REF: FD 7B BC A9 F7 0B 00 F9 F6 57 02 A9 F4 4F 03 A9 FD 03 00 91 28 20 40 F9
	std::array offsets = {
		0x13C5710,	//1.0.1
		0x13B59D0	//1.0.2
	};
	//REF: FF 43 02 D1 FD 7B 03 A9 FC 6F 04 A9 FA 67 05 A9 F8 5F 06 A9 F6 57 07 A9 F4 4F 08 A9 FD C3 00 91 13 E0 00 91 F6 03 00 AA F4 03 05 AA E0 03 13 AA F5 03 04 AA F7 03 03 AA F8 03 02 2A F9 03 01 2A
	std::array WEM_offsets = {
		0x2BED0,
		0x2C050
	};
	//REF: FD 7B BB A9 F9 0B 00 F9 F8 5F 02 A9 F6 57 03 A9 F4 4F 04 A9 FD 03 00 91 F7 03 01 AA 21 10 40 B8
	std::array HIRC_offsets = {
		0xB4C900,
		0xB3CB00
	};

	for (size_t i = 0; i < offsets.size(); i++) {
		uintptr_t pointer = exl::util::modules::GetTargetOffset(offsets[i]);
		if (!memcmp(pattern, (void*)pointer, sizeof(pattern))) {
			CreateFileStruct::InstallAtOffset(offsets[i]);
			LoadWEMFile::InstallAtOffset(WEM_offsets[i]);
			ParseHIRC::InstallAtOffset(HIRC_offsets[i]);
			break;
		}
	}
}

extern "C" NORETURN void exl_exception_entry() {
	/* TODO: exception handling */
	EXL_ABORT(0x420);
}