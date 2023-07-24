﻿#include <Windows.h>
#include <Psapi.h>
#include <MinHook/include/MinHook.h>
#include <iostream>
#include "include/main.h"
#include "tiny-json/tiny-json.h"
#include <deque>
#include <fcntl.h>
#include <io.h>
#include <fstream>
#include <iomanip>
//#define TARGET_API_ROOT L"localhost"
#define TARGET_API_ROOT L"servers.polehammer.net"

#include <cstdint>
#include <nmmintrin.h> // SSE4.2 intrinsics

struct BuildType {
	uint32_t fileHash = 0;
	uint32_t buildId = 0;
	uint32_t offsets[F_MaxFuncType] = {};
	std::string name = "<unnamed>";
};

std::deque<BuildType*> configBuilds;

uint32_t calculateCRC32(const std::string& filename) {
	std::ifstream file(filename, std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "Error opening file: " << filename << std::endl;
		return 0;
	}

	uint32_t crc = 0; // Initial value for CRC-32

	char buffer[4096];
	while (file) {
		file.read(buffer, sizeof(buffer));
		std::streamsize bytesRead = file.gcount();

		for (std::streamsize i = 0; i < bytesRead; ++i) {
			crc = _mm_crc32_u8(crc, buffer[i]);
		}
	}

	file.close();
	return crc ^ 0xFFFFFFFF; // Final XOR value for CRC-32
}

DECL_HOOK(void*, GetMotd, (GCGObj* this_ptr, void* a2, void* a3, void* a4)) {
	auto old_base = this_ptr->url_base;
	this_ptr->url_base = FString(L"http://" TARGET_API_ROOT L"/api/tbio");
	void* res = o_GetMotd(this_ptr, a2, a3, a4);

	this_ptr->url_base = old_base;
	log("GetMotd returned");
	return res;
}

DECL_HOOK(void*, GetCurrentGames, (GCGObj* this_ptr, void* a2, void* a3, void* a4)) {
	log("GetCurrentGames called");
	auto old_base = this_ptr->url_base;

	this_ptr->url_base = FString( L"http://" TARGET_API_ROOT "/api/tbio" );
	void* res{ o_GetCurrentGames(this_ptr, a2, a3, a4) };

	this_ptr->url_base = old_base;
	log("GetCurrentGames returned");
	return res;
}

DECL_HOOK(void*, SendRequest, (GCGObj* this_ptr, FString* a2, FString* a3, FString* a4, FString* a5)) {

	if (a2->letter_count > 0 &&
		wcscmp(L"https://EBF8D.playfabapi.com/Client/Matchmake?sdk=Chiv2_Version", a2->str) == 0)
	{
		wcscpy_s(a2->str, static_cast<size_t>(a2->max_letters), L"http://" TARGET_API_ROOT "/api/playfab/Client/Matchmake");
		log("hk_SendRequest Client/Matchmake");
	}
	return o_SendRequest(this_ptr, a2, a3, a4, a5);
}


// AssetLoaderPlugin

DECL_HOOK(long long, FindFileInPakFiles_1, (void* this_ptr, const wchar_t* Filename, void** OutPakFile, void* OutEntry)) {
	auto attr{ GetFileAttributesW(Filename) };
	if (attr != INVALID_FILE_ATTRIBUTES && Filename && wcsstr(Filename, L"../../../")) {
		if (OutPakFile) OutPakFile = nullptr;
		return 0;
	}

	return o_FindFileInPakFiles_1(this_ptr, Filename, OutPakFile, OutEntry);
}

DECL_HOOK(long long, FindFileInPakFiles_2, (void* this_ptr, const wchar_t* Filename, void** OutPakFile, void* OutEntry)) {
	auto attr{ GetFileAttributesW(Filename) };
	if (attr != INVALID_FILE_ATTRIBUTES && Filename && wcsstr(Filename, L"../../../")) {
		if (OutPakFile) OutPakFile = nullptr;
		return 0;
	}

	return o_FindFileInPakFiles_2(this_ptr, Filename, OutPakFile, OutEntry);
}

DECL_HOOK(long long, IsNonPakFilenameAllowed, (void* this_ptr, void* InFilename)) {
	return 1;
}

//FString* __cdecl
//UUserFeedbackAndBugReportsLibrary::GetGameInfo(FString* __return_storage_ptr__, UWorld* param_1)
DECL_HOOK(FString*, GetGameInfo, (FString* ret_ptr, void* uWorld))
{
	auto val = o_GetGameInfo(ret_ptr, uWorld);
#ifdef _DEBUG
	std::wcout << "GetGameInfo: " << *val->str << std::endl;
#endif
	return val;
}

//FViewport* __thiscall FViewport::FViewport(FViewport* this, FViewportClient* param_1)
struct FViewport_C
{
	uint8_t ph[0x20];
	FString AppVersionString;
};

// FIXME
struct BuildInfo
{
	BuildInfo() {}
	BuildInfo(const wchar_t* str, uint32_t id) //: buildStr(str), buildId(id) {}
	{
		size_t sz;
		buildStr = (char*)malloc(128 * MB_CUR_MAX);
		wcstombs_s(&sz, buildStr, 128 * MB_CUR_MAX, str, 127);
		//std::cout << "parsed: " << buildStr << std::endl;
		//wctomb(buildStr, *str);
		buildId = id;
	}
	char * buildStr;
	uint32_t buildId;
};

//BuildInfo* curBuildInfo = nullptr;
BuildType curBuild;
bool jsonDone = false;
bool offsetsLoaded = false;
bool needsSerialization = true;

void serializeBuilds()
{
	char buff[2048];
	char* dest = buff;
	/*if (offsetsLoaded && curBuild == nullptr)
	{
		needsSerialization = false;
		BuildType build;
		build.name = "<unnamed>";
		build.buildId = 0;
		build.fileHash = 0;
		curBuild = new BuildType(build);
		log("unnamed init");
	}*/
	if (curBuild.buildId > 0)
	{

		char* pValue;
		size_t len;
		char ladBuff[512];
		errno_t err = _dupenv_s(&pValue, &len, "LOCALAPPDATA");
		strncpy_s(ladBuff, 256, pValue, len);
		strncpy_s(ladBuff + len - 1, 256 - len, "\\Chivalry 2\\Saved\\Config\\c2uc.builds.json", 42);

		/*for (uint8_t i = 0; i < F_MaxFuncType; ++i)
			curBuild.offsets[i] = addrOffsets[i];*/

		log(ladBuff);
		std::ofstream out(ladBuff);
		/*out << "{\n\"" << curBuildInfo->buildStr << "\": {";
		out << "\n\"Build\" : " << curBuildInfo->buildId;
		out << ",\n\"FileHash\" : " << calculateCRC32("Chivalry2-Win64-Shipping.exe");
		for (uint8_t i = 0; i < F_MaxFuncType; ++i)
			out << ",\n\"" << strFunc[i] << "\": " << addrOffsets[i];*/

		out << "\n{\n\"" << curBuild.name << "\": {";
		out << "\n\"Build\" : " << curBuild.buildId;
		out << ",\n\"FileHash\" : " << curBuild.fileHash;
		for (uint8_t i = 0; i < F_MaxFuncType; ++i)
			out << ",\n\"" << strFunc[i] << "\": " << curBuild.offsets[i];

		for (auto build : configBuilds)
		{
			out << "\n},\n\"" << build->name << "\": {";
			out << "\n\"Build\" : " << build->buildId;
			out << ",\n\"FileHash\" : " << build->fileHash;
			for (uint8_t i = 0; i < F_MaxFuncType; ++i)
				out << ",\n\"" << strFunc[i] << "\": " << build->offsets[i];
		}
		out << "\n}";
		out << "\n}";

	}

}

DECL_HOOK(FString*, FViewport, (FViewport_C* this_ptr, void* viewportClient))
{
	auto val = o_FViewport(this_ptr, viewportClient);
	wchar_t* buildNr = wcschr(this_ptr->AppVersionString.str, L'+') + 1;
	if (buildNr != nullptr)
	{
		uint32_t buildId = _wtoi(buildNr);
		if (curBuild.buildId == 0)
		{
			needsSerialization = true;
			auto bi = new BuildInfo(this_ptr->AppVersionString.str + 7, buildId); // FIXME

			curBuild.name = bi->buildStr;
			curBuild.buildId = buildId;
			curBuild.fileHash = calculateCRC32("Chivalry2-Win64-Shipping.exe");
			/*for (uint8_t i = 0; i < F_MaxFuncType; ++i)
				curBuild.offsets[i] = addrOffsets[i];*/
			//curBuild = new BuildType(build);
			log("Viewport constructor!");
			if (offsetsLoaded)
				serializeBuilds();
		}
		/*else if (curBuild && curBuild->buildId == 0)
		{ }*/
		else
			log("Viewport constructor! (loaded)");
			
			

#ifdef _DEBUG
		std::wcout << this_ptr->AppVersionString.str << ": " << buildNr << " " << buildId << std::endl;
#endif
	}
	return val;
}

int LoadBuildConfig()
{
	// load config file
	char* pValue;
	size_t len;
	char ladBuff[256];
	errno_t err = _dupenv_s(&pValue, &len, "LOCALAPPDATA");
	strncpy_s(ladBuff, 256, pValue, len);
	strncpy_s(ladBuff + len - 1, 256 - len, "\\Chivalry 2\\Saved\\Config\\c2uc.builds.json", 42);

	std::ifstream file(ladBuff, std::ios::binary);
	if (!file.is_open()) {
		std::cout << "Error opening build config" << std::endl;
		return 0;
	}
	std::string buffer;
	file.seekg(0, std::ios::end);
	buffer.reserve(file.tellg());
	file.seekg(0, std::ios::beg);

	buffer.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();

	// parse json
	json_t mem[2048];
	const json_t* json = json_create(const_cast<char*>(buffer.c_str()), mem, 2048);

	if (!json) {
		puts("Failed to create json parser");
		return EXIT_FAILURE;
	}
	uint32_t curFileHash = calculateCRC32("Chivalry2-Win64-Shipping.exe");

	json_t const* buildEntry;
	needsSerialization = true;
	for (buildEntry = json_getChild(json); buildEntry != 0; buildEntry = json_getSibling(buildEntry)) {
		if (JSON_OBJ == json_getType(buildEntry)) {

			char const* fileSize = json_getPropertyValue(buildEntry, "FileSize");
			json_t const* build = json_getProperty(buildEntry, "Build");
			char const* buildName = json_getName(buildEntry);
			printf("parsing %s\n", buildName);

			json_t const* fileHash = json_getProperty(buildEntry, "FileHash");
			if (!fileHash || JSON_INTEGER != json_getType(fileHash)) {
				puts("Error, the fileHash property is not found.");
				return EXIT_FAILURE;
			}

			uint64_t fileHashVal = json_getInteger(fileHash);
			bool hashMatch = fileHashVal == curFileHash;

			if (hashMatch)
			{
				needsSerialization = false;
				printf("Hash match (0x%llx) Build: %s\n", fileHashVal, buildName);
			}

			BuildType bd;
			bd.name = std::string(buildName);
			bd.buildId = (uint32_t)json_getInteger(build);
			bd.fileHash = (uint32_t)fileHashVal;
			for (uint8_t i = 0; i < F_MaxFuncType; ++i)
			{
				if (const json_t* GetMotd_j = json_getProperty(buildEntry, strFunc[i]))
				{
					if (JSON_INTEGER == json_getType(GetMotd_j))
						if (uint32_t offsetVal = (uint32_t)json_getInteger(GetMotd_j))
							bd.offsets[i] = offsetVal;
				}
				else if (hashMatch) needsSerialization = true;
			}

			if (hashMatch)
				curBuild = bd;
			else
				configBuilds.push_back(new BuildType(bd));
		}
	}
}



unsigned long main_thread(void* lpParameter) {
	log(logo);
	log("Chivalry 2 Unchained");
	MH_Initialize();
	// https://github.com/HoShiMin/Sig
	const void* found = nullptr;
	//found = Sig::find(buf, size, "11 22 ? 44 ?? 66 AA bb cC Dd");
	//HMODULE var = GetModuleHandle(NULL);
	//GetModuleInformation(GetCurrentProcess(), var, &moduleInfo, sizeof(moduleInfo));
	LoadBuildConfig();
	baseAddr = GetModuleHandleA("Chivalry2-Win64-Shipping.exe");
	//cerTest();
	//std::cout << std::hex << calculateCRC32("Chivalry2-Win64-Shipping.exe") << std::endl;
	int file_descript;
	//unsigned long file_size;
	errno_t err = _sopen_s(&file_descript, "Chivalry2-Win64-Shipping.exe", O_RDONLY, _SH_DENYNO, 0);
	if (err)
		std::cout << "error " << err << std::endl;

	// Get the size of the file
	off_t file_size = _filelength(file_descript);

	//file_size = get_size_by_fd(file_descript);
	/*printf("file size:\t%lu\n", file_size);*/

	//MODULEINFO moduleInfo;	
	GetModuleInformation(GetCurrentProcess(), baseAddr, &moduleInfo, sizeof(moduleInfo));

	//std::cout << "MINFO: " << moduleInfo.SizeOfImage << " " << moduleInfo.EntryPoint << std::endl;

	//uint32_t crc = crc32c_append(0, (uint8_t *)baseAddr, moduleInfo.SizeOfImage);

	unsigned char* module_base{ reinterpret_cast<unsigned char*>(baseAddr) };

	for (uint8_t i = 0; i < F_MaxFuncType; ++i)
	{
		if (curBuild.offsets[i] == 0)
			curBuild.offsets[i] = FindSignature(baseAddr, moduleInfo.SizeOfImage, strFunc[i], signatures[i]);
		else printf("ok -> %s : (conf)\n", strFunc[i]);
		if (i == F_FViewport)
		{
			HOOK_ATTACH(module_base, FViewport);
		}
	}



	char buff[512];
	char* dest = buff;

	offsetsLoaded = true;
	serializeBuilds();
	// official
	//auto sig_SendRequest= 0x14a1250;
	//auto sig_GetMotd = 0x13da7d0;
	//auto sig_GetCurrentGames = 0x13da280;
	//auto sig_IsNonPakFilenameAllowed = 0x2fc3ce0;
	//auto sig_FindFileInPakFiles_1 = 0x2fbf1a0;
	//auto sig_FindFileInPakFiles_2 = 0x2fbf280;
	//auto sig_UTBLLocalPlayer = 0x199cda3;

	// ptr
	//auto sig_SendRequest = 0x1425a10;
	//auto sig_GetMotd = 0x135eb70;
	//auto sig_GetCurrentGames = 0x135e620;
	//auto sig_IsNonPakFilenameAllowed = 0x2f4dd80;
	//auto sig_FindFileInPakFiles_1 = 0x2f49240;
	//auto sig_FindFileInPakFiles_2 = 0x2f49320;
	//auto sig_UTBLLocalPlayer = 0x1924926;

	//HOOK_ATTACH(module_base, FViewport);
	HOOK_ATTACH(module_base, GetMotd);
	HOOK_ATTACH(module_base, GetCurrentGames);
	HOOK_ATTACH(module_base, SendRequest);
	HOOK_ATTACH(module_base, IsNonPakFilenameAllowed);
	HOOK_ATTACH(module_base, FindFileInPakFiles_1);
	HOOK_ATTACH(module_base, FindFileInPakFiles_2);
	HOOK_ATTACH(module_base, GetGameInfo);


	// ServerPlugin
	auto cmd_permission{ module_base + curBuild.offsets[F_UTBLLocalPlayer_Exec] }; // Patch for command permission when executing commands (UTBLLocalPlayer::Exec)

	// 75 1A 45 84 ED 75 15 48 85 F6 74 10 40 38 BE ? ? ? ? 74 07 32 DB E9 ? ? ? ? 48 8B 5D 60 49 8B D6 4C 8B 45 58 4C 8B CB 49 8B CF (Points directly to instruction: first JNZ)

	DWORD d;
	VirtualProtect(cmd_permission, 1, PAGE_EXECUTE_READWRITE, &d);
	*cmd_permission = 0xEB; // Patch to JMP
	VirtualProtect(cmd_permission, 1, d, NULL); //TODO: Convert patch to hook.

	ExitThread(0);
	return 0;
}

int __stdcall DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	#ifdef _DEBUG
	if (!GetConsoleWindow()) {
		AllocConsole();
		FILE* file_pointer{};
		freopen_s(&file_pointer, "CONOUT$", "w", stdout);
	}
	#endif
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH: {
		HANDLE thread_handle{ CreateThread(NULL, 0, main_thread, hModule, 0, NULL) };
		if (thread_handle) CloseHandle(thread_handle);
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return 1;
}