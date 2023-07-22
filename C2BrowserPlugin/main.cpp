#include <Windows.h>
#include <Psapi.h>
#include <MinHook/include/MinHook.h>
#include <iostream>
#include <Sig/Sig.hpp>

//#define TARGET_API_ROOT L"localhost"
#define TARGET_API_ROOT L"servers.polehammer.net"

struct FString {
	FString(const wchar_t* str) {
		this->letter_count = lstrlenW(str) + 1;
		this->max_letters = this->letter_count;
		this->str = const_cast<wchar_t*>(str);
	}

	wchar_t* str;
	int letter_count;
	int max_letters;
};

struct GCGObj {
	FString url_base;
};

void log(const char* str) {
#ifndef _DEBUG
	return;
#endif
	std::cout << str << std::endl;

}

int logWideString(wchar_t* str) {
#ifndef _DEBUG
	return 0;
#endif
	int i = 0;
	while (*(char*)str != 0) {
		std::cout << *(char*)str;
		str+=2;
		i++;
	}
	std::cout << std::endl;
	return i;
}

typedef void* (*GetCurrentGames_t)(GCGObj*, void*, void*, void*);
GetCurrentGames_t o_GetCurrentGames;

typedef FString* (*ConcatUrl_t)(FString*, FString*);
ConcatUrl_t o_ConcatUrl;

typedef wchar_t** (*GetMotd_t)(GCGObj* this_ptr, void* a2, void* a3, void* a4);
GetMotd_t o_GetMotd;

void* hk_GetMotd(GCGObj* this_ptr, void* a2, void* a3, void* a4) {
	auto old_base = this_ptr->url_base;
	this_ptr->url_base = FString(L"http://" TARGET_API_ROOT L"/api/tbio");
	void* res = o_GetMotd(this_ptr, a2, a3, a4);

	this_ptr->url_base = old_base;
	log("GetMotd returned");
	return res;
}

void* hk_GetCurrentGames(GCGObj* this_ptr, void* a2, void* a3, void* a4) {
	log("GetCurrentGames called");
	auto old_base = this_ptr->url_base;

	this_ptr->url_base = FString( L"http://" TARGET_API_ROOT "/api/tbio" );
	void* res{ o_GetCurrentGames(this_ptr, a2, a3, a4) };

	this_ptr->url_base = old_base;
	log("GetCurrentGames returned");
	return res;
}

FString* hk_ConcatUrl(FString* final_url, FString* url_path) {
	if (wcscmp(url_path->str, L"/Client/Matchmake") != 0) return o_ConcatUrl(final_url, url_path);
	
	log("ConcatUrl: Substituting matchmaking call");

	const wchar_t* custom_url{ L"http://" TARGET_API_ROOT "/api/playfab/Client/Matchmake" };
	final_url->letter_count = lstrlenW(custom_url) + 1;

	wcscpy_s(final_url->str, static_cast<size_t>(final_url->letter_count), custom_url);
	return final_url;
}


typedef wchar_t** (*SendRequest_t)(GCGObj* this_ptr, FString* a2, FString* a3, FString* a4, FString* a5);
SendRequest_t o_SendRequest;


void* hk_SendRequest(GCGObj* this_ptr, FString* a2, FString* a3, FString* a4, FString* a5) {

	if (a2->letter_count > 0 &&
		wcscmp(L"https://EBF8D.playfabapi.com/Client/Matchmake?sdk=Chiv2_Version", a2->str) == 0)
	{
		wcscpy_s(a2->str, static_cast<size_t>(a2->max_letters), L"http://" TARGET_API_ROOT "/api/playfab/Client/Matchmake");
		log("hk_SendRequest Client/Matchmake");
	}
	return o_SendRequest(this_ptr, a2, a3, a4, a5);
}

void FindSignature(HMODULE baseAddr, DWORD size, const char* title, const char* signature)
{
	const void* found = nullptr;
	found = Sig::find(baseAddr, size, signature);
	if (found != nullptr)
		std::cout << title << ": 0x" << std::hex << (long long)found - (long long)baseAddr << std::endl;
	else
		std::cout << title << ": nullptr" << std::endl;

}

unsigned long main_thread(void* lpParameter) {
	log("BrowserPlugin started!");
	MH_Initialize();
	// https://github.com/HoShiMin/Sig
	const void* found = nullptr;

	//found = Sig::find(buf, size, "11 22 ? 44 ?? 66 AA bb cC Dd");

	//HMODULE var = GetModuleHandle(NULL);
	const HMODULE baseAddr = GetModuleHandleA("Chivalry2-Win64-Shipping.exe");
	MODULEINFO moduleInfo;	
	GetModuleInformation(GetCurrentProcess(), baseAddr, &moduleInfo, sizeof(moduleInfo));
	//GetModuleInformation(GetCurrentProcess(), var, &moduleInfo, sizeof(moduleInfo));

	std::cout << "MINFO: " << moduleInfo.SizeOfImage << " " << moduleInfo.EntryPoint << std::endl;
	

	unsigned char* module_base{ reinterpret_cast<unsigned char*>(baseAddr) };
	
	FindSignature(baseAddr, moduleInfo.SizeOfImage, "SendRequest", "48 89 5C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 55 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 40 48 8B D9 49 8B F9");
	FindSignature(baseAddr, moduleInfo.SizeOfImage, "GetMotd", "4C 89 4C 24 ?? 4C 89 44 24 ?? 48 89 4C 24 ?? 55 56 57 41 54 48 8D 6C 24 ?? 48 81 EC D8 00 00 00 83 79 ?? 01 4C 8B E2 48 8B F9 7F ?? 33 F6 48 8B C2 48 89 32 48 89 72 ?? 48 81 C4 D8 00 00 00 41 5C 5F 5E 5D C3 48 89 9C 24 ?? ?? ?? ?? 48 8D 55 ?? 4C 89 AC 24 ?? ?? ?? ?? 4C 89 B4 24 ?? ?? ?? ?? 4C 89 BC 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 4C 8B 6D ?? 48 8D 4D ?? 33 F6 48 89 75 ?? 48 89 75 ?? 49 8B 45 00 8D 56 ?? 48 8B 40 ?? 48 89 45 ?? E8 ?? ?? ?? ?? 8B 55 ?? 8D 5A ?? 89 5D ?? 3B 5D ?? 7E ?? 48 8D 4D ?? E8 ?? ?? ?? ?? 8B 5D ?? 4C 8B 75 ?? 48 8D 15 ?? ?? ?? ?? 49 8B CE 41 B8 12 00 00 00");
	FindSignature(baseAddr, moduleInfo.SizeOfImage, "GetCurrentGames", "4C 89 4C 24 ?? 4C 89 44 24 ?? 48 89 4C 24 ?? 55 56 57 41 54 48 8D 6C 24 ?? 48 81 EC D8 00 00 00 83 79 ?? 01 4C 8B E2 48 8B F9 7F ?? 33 F6 48 8B C2 48 89 32 48 89 72 ?? 48 81 C4 D8 00 00 00 41 5C 5F 5E 5D C3 48 89 9C 24 ?? ?? ?? ?? 48 8D 55 ?? 4C 89 AC 24 ?? ?? ?? ?? 4C 89 B4 24 ?? ?? ?? ?? 4C 89 BC 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 4C 8B 6D ?? 48 8D 4D ?? 33 F6 48 89 75 ?? 48 89 75 ?? 49 8B 45 00 8D 56 ?? 48 8B 40 ?? 48 89 45 ?? E8 ?? ?? ?? ?? 8B 55 ?? 8D 5A ?? 89 5D ?? 3B 5D ?? 7E ?? 48 8D 4D ?? E8 ?? ?? ?? ?? 8B 5D ?? 4C 8B 75 ?? 48 8D 15 ?? ?? ?? ?? 49 8B CE 41 B8 22 00 00 00 E8 ?? ?? ?? ?? 8B 4F ?? 83 F9 01 7F ?? 4D 8B FE 4C 8B F6 EB ?? 85 DB 74 ?? 44 8D 7B ?? 41 8B C7 EB ?? 44 8B FE 8B C6 48 8B 5D ?? 8D 14 ?? 48 8B F9 48 89 75 ?? 45 33 C0 89 7D ?? 48 8D 4D ?? 48 8B 1B E8 ?? ?? ?? ?? 48 8B 4D ?? 4C 8D 04 ?? 48 8B D3 E8 ?? ?? ?? ?? 45 8B C7 48 8D 4D ?? 49 8B D6 E8 ?? ?? ?? ?? 4C 8B 7D ?? 8B 5D ?? 48 89 75 ?? 48 89 75 ?? 48 89 75 ?? 8B D6 89 55 ?? 8B CE 89 4D ?? 85 DB 74 ?? 49 8B FF 4D 85 FF 74 ?? EB ?? 48 8D 3D ?? ?? ?? ?? 66 39 37 74 ?? 48 C7 C3 FF FF FF FF 48 FF C3 66 39 34 ?? 75 ?? FF C3 85 DB 7E ?? 8B D3 48 8D 4D ?? E8 ?? ?? ?? ?? 8B 4D ?? 8B 55 ?? 8D 04 ?? 89 45 ?? 3B C1 7E ?? 48 8D 4D ?? E8 ?? ?? ?? ?? 48 8B 4D ?? 48 8B D7 4C 63 C3 4D 03 C0 E8 ?? ?? ?? ?? 48 8D 55 ?? 49 8B CD FF 55 ?? 48 8B 4D ?? 48 85 C9 74 ?? E8 ?? ?? ?? ?? 4D 85 FF 74 ?? 49 8B CF E8 ?? ?? ?? ?? 4D 85 F6 74 ?? 49 8B CE E8 ?? ?? ?? ?? 4C 8B 75 ?? 8B CE 49 83 C6 10 89 4D ?? 45 8B 56 ?? 49 8D 7E ?? C7 45 ?? 01 00 00 00 44 8B C6 48 89 7D ?? 41 BB 1F 00 00 00 C7 45 ?? FF FF FF FF 48 89 75 ?? 45 85 D2 74 ?? 48 8B 47 ?? 4C 8B CF 48 85 C0 4C 0F 45 C8 41 8D 42 ?? 99 41 23 D3 8D 1C ?? 41 8B 11 C1 FB 05 85 D2 75 ?? 0F 1F 80 00 00 00 00 FF C1 41 83 C0 20 89 4D ?? 44 89 45 ?? 3B CB 7F ?? 48 63 C1 C7 45 ?? FF FF FF FF 41 8B 14 ?? 85 D2 74 ?? 8B C2 F7 D8 23 C2 0F BD C8 89 45 ?? 74 ?? 41 8B C3 2B C1 EB ?? B8 20 00 00 00 44 2B C0 41 8D 40 ?? 89 45 ?? 41 3B C2 7E ?? 44 89 55 ?? 41 8B 56 ?? 41 BD FF FF FF FF 0F 10 55 ?? 8B CA 4C 89 75 ?? 0F 10 45 ?? 41 23 CB 44 8B C2 0F 11 55 ?? 41 D3 E5 44 8B CA 0F 11 45 ?? 41 C1 F8 05 41 83 E1 E0 66 0F 15 D2 45 8B FA F2 0F 11 55 ?? 44 89 6D ?? 89 55 ?? 0F 10 45 ?? 0F 10 4D ?? 0F 11 45 ?? 0F 11 4D ?? 41 3B D2 74 ?? 48 8B 47 ?? 4C 8B D7 48 85 C0 49 63 C8 4C 0F 45 D0 41 8D 47 ?? 99 41 23 D3 8D 1C ?? 41 8B 14 ?? C1 FB 05 41 23 D5 75 ?? 41 FF C0 41 83 C1 20 44 3B C3 7F ?? 49 63 C0 C7 45 ?? FF FF FF FF 41 8B 14 ?? 85 D2 74 ?? 8B C2 F7 D8 23 C2 0F BD C8 74 ?? 44 2B D9 EB ?? 41 BB 20 00 00 00 45 2B CB 41 8D 41 ?? 89 45 ?? 41 3B C7 7E ?? 44 89 7D ?? 48 8B 5D ?? 4C 8B BC 24 ?? ?? ?? ?? 4C 8B AC 24 ?? ?? ?? ?? 48 C1 EB 20 48 63 45 ?? 48 8B 55 ?? 3B C3 75 ?? 48 39 7D ?? 75 ?? 49 3B D6 74 ?? 48 8D 0C ?? 48 8B 02 48 8D 14 ?? 48 8B 4D ?? 4C 8D 42 ?? 48 8B 01 FF 50 ?? 8B 45 ?? 48 8D 4D ?? F7 D0 21 45 ?? E8 ?? ?? ?? ?? EB ?? 48 8B 4D ?? 48 8D 55 ?? E8 ?? ?? ?? ?? 48 8B 4D ?? 48 8B 01 FF 90 ?? ?? ?? ?? 4C 8B B4 24 ?? ?? ?? ?? 4C 8D 45 ?? 48 8B D8 48 8B CE 48 8B 45 ?? 8B D6 48 89 4D ?? 89 55 ?? 49 3B C0 74 ?? 39 48 ?? 74 ?? 4C 8B 00 4D 85 C0 74 ?? 49 8B 00 48 8D 55 ?? 49 8B C8 FF 50 ?? 8B 55 ?? 48 8B 4D ?? 48 89 75 ?? 89 75 ?? 85 D2 74 ?? 48 85 C9 74 ?? 48 8B 01 48 8D 55 ?? FF 50 ?? 48 8B 55 ?? 4C 8D 4D ?? 4C 8D 05 ?? ?? ?? ?? 48 8D 4D ?? E8 ?? ?? ?? ?? 48 8B D0 48 8B CB E8 ?? ?? ?? ?? 39 75 ?? 74 ?? 48 8B 4D ?? 48 85 C9 74 ?? 48 8B 01 33 D2 FF 50 ?? 48 8B 45 ?? 48 85 C0 74 ?? 45 33 C0 33 D2 48 8B C8 E8 ?? ?? ?? ?? 48 89 45 ?? 89 75 ?? EB ?? 48 8B 45 ?? 48 85 C0 74 ?? 48 8B C8 E8 ?? ?? ?? ?? 8B 45 ?? 48 8B 4D ?? 85 C0 74 ?? 48 85 C9 75 ?? 85 C0 74 ?? 48 85 C9 74 ?? 48 8B 01 33 D2 FF 50 ?? 48 8B 4D ?? 48 85 C9 74 ?? 45 33 C0 33 D2 E8 ?? ?? ?? ?? 48 8B C8 48 89 45 ?? 89 75 ?? 48 85 C9 74 ?? E8 ?? ?? ?? ?? 48 8B 4D ?? 48 8B 01 FF 90 ?? ?? ?? ?? 48 8B 45 ?? 49 89 04 24 48 8B 45 ?? 49 89 44 24 ?? 48 85 C0 74 ?? FF 40 ?? 48 8B 5D ?? 48 85 DB 74 ?? 83 6B ?? 01 75 ?? 48 8B 03 48 8B CB FF 10 83 6B ?? 01 75 ?? 48 8B 03 BA 01 00 00 00 48 8B CB FF 50 ?? 48 8B 9C 24 ?? ?? ?? ?? 49 8B C4 48 81 C4 D8 00 00 00 41 5C 5F 5E 5D C3");

	// this doesn't work
	//FindSignature(baseAddr, moduleInfo.SizeOfImage, "IsNonPakFilenameAllowed", "48 89 5C 24 ?? 48 89 6C 24 ?? 56 57 41 56 48 83 EC 30 48 8B F1 45 33 C0 48 8D 4C 24 ?? 4C 8B F2");
	FindSignature(baseAddr, moduleInfo.SizeOfImage, "IsNonPakFilenameAllowed", "48 89 5C 24 ?? 48 89 6C 24 ?? 56 57 41 56 48 83 EC 30 48 8B F1 45 33 C0");
	FindSignature(baseAddr, moduleInfo.SizeOfImage, "FindFileInPakFiles_1", "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 41 54 41 55 41 56 41 57 48 83 EC 30 33 FF");
	FindSignature(baseAddr, moduleInfo.SizeOfImage, "FindFileInPakFiles_2", "48 8B C4 4C 89 48 ?? 4C 89 40 ?? 48 89 48 ?? 55 53 48 8B EC");
	FindSignature(baseAddr, moduleInfo.SizeOfImage, "UTBLLocalPlayer::Exec", "75 1A 45 84 ED 75 15 48 85 F6 74 10 40 38 BE ? ? ? ? 74 07 32 DB E9 ? ? ? ? 48 8B 5D 60 49 8B D6 4C 8B 45 58 4C 8B CB 49 8B CF");


	//FindSignature(baseAddr, moduleInfo.SizeOfImage, "", "");
	/*found = Sig::find(baseAddr, moduleInfo.SizeOfImage, "48 89 5C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 55 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 40 48 8B D9 49 8B F9");
	std::cout << "SendRequest: 0x" << std::hex << (long long)found - (long long)baseAddr << std::endl;
	found = Sig::find(baseAddr, moduleInfo.SizeOfImage, "4C 89 4C 24 ?? 4C 89 44 24 ?? 48 89 4C 24 ?? 55 56 57 41 54 48 8D 6C 24 ?? 48 81 EC D8 00 00 00 83 79 ?? 01 4C 8B E2 48 8B F9 7F ?? 33 F6 48 8B C2 48 89 32 48 89 72 ?? 48 81 C4 D8 00 00 00 41 5C 5F 5E 5D C3 48 89 9C 24 ?? ?? ?? ?? 48 8D 55 ?? 4C 89 AC 24 ?? ?? ?? ?? 4C 89 B4 24 ?? ?? ?? ?? 4C 89 BC 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 4C 8B 6D ?? 48 8D 4D ?? 33 F6 48 89 75 ?? 48 89 75 ?? 49 8B 45 00 8D 56 ?? 48 8B 40 ?? 48 89 45 ?? E8 ?? ?? ?? ?? 8B 55 ?? 8D 5A ?? 89 5D ?? 3B 5D ?? 7E ?? 48 8D 4D ?? E8 ?? ?? ?? ?? 8B 5D ?? 4C 8B 75 ?? 48 8D 15 ?? ?? ?? ?? 49 8B CE 41 B8 12 00 00 00");
	std::cout << "o_GetMotd: 0x" << std::hex << (long long)found - (long long)baseAddr << std::endl;
	found = Sig::find(baseAddr, moduleInfo.SizeOfImage, "4C 89 4C 24 ?? 4C 89 44 24 ?? 48 89 4C 24 ?? 55 56 57 41 54 48 8D 6C 24 ?? 48 81 EC D8 00 00 00 83 79 ?? 01 4C 8B E2 48 8B F9 7F ?? 33 F6 48 8B C2 48 89 32 48 89 72 ?? 48 81 C4 D8 00 00 00 41 5C 5F 5E 5D C3 48 89 9C 24 ?? ?? ?? ?? 48 8D 55 ?? 4C 89 AC 24 ?? ?? ?? ?? 4C 89 B4 24 ?? ?? ?? ?? 4C 89 BC 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 4C 8B 6D ?? 48 8D 4D ?? 33 F6 48 89 75 ?? 48 89 75 ?? 49 8B 45 00 8D 56 ?? 48 8B 40 ?? 48 89 45 ?? E8 ?? ?? ?? ?? 8B 55 ?? 8D 5A ?? 89 5D ?? 3B 5D ?? 7E ?? 48 8D 4D ?? E8 ?? ?? ?? ?? 8B 5D ?? 4C 8B 75 ?? 48 8D 15 ?? ?? ?? ?? 49 8B CE 41 B8 22 00 00 00 E8 ?? ?? ?? ?? 8B 4F ?? 83 F9 01 7F ?? 4D 8B FE 4C 8B F6 EB ?? 85 DB 74 ?? 44 8D 7B ?? 41 8B C7 EB ?? 44 8B FE 8B C6 48 8B 5D ?? 8D 14 ?? 48 8B F9 48 89 75 ?? 45 33 C0 89 7D ?? 48 8D 4D ?? 48 8B 1B E8 ?? ?? ?? ?? 48 8B 4D ?? 4C 8D 04 ?? 48 8B D3 E8 ?? ?? ?? ?? 45 8B C7 48 8D 4D ?? 49 8B D6 E8 ?? ?? ?? ?? 4C 8B 7D ?? 8B 5D ?? 48 89 75 ?? 48 89 75 ?? 48 89 75 ?? 8B D6 89 55 ?? 8B CE 89 4D ?? 85 DB 74 ?? 49 8B FF 4D 85 FF 74 ?? EB ?? 48 8D 3D ?? ?? ?? ?? 66 39 37 74 ?? 48 C7 C3 FF FF FF FF 48 FF C3 66 39 34 ?? 75 ?? FF C3 85 DB 7E ?? 8B D3 48 8D 4D ?? E8 ?? ?? ?? ?? 8B 4D ?? 8B 55 ?? 8D 04 ?? 89 45 ?? 3B C1 7E ?? 48 8D 4D ?? E8 ?? ?? ?? ?? 48 8B 4D ?? 48 8B D7 4C 63 C3 4D 03 C0 E8 ?? ?? ?? ?? 48 8D 55 ?? 49 8B CD FF 55 ?? 48 8B 4D ?? 48 85 C9 74 ?? E8 ?? ?? ?? ?? 4D 85 FF 74 ?? 49 8B CF E8 ?? ?? ?? ?? 4D 85 F6 74 ?? 49 8B CE E8 ?? ?? ?? ?? 4C 8B 75 ?? 8B CE 49 83 C6 10 89 4D ?? 45 8B 56 ?? 49 8D 7E ?? C7 45 ?? 01 00 00 00 44 8B C6 48 89 7D ?? 41 BB 1F 00 00 00 C7 45 ?? FF FF FF FF 48 89 75 ?? 45 85 D2 74 ?? 48 8B 47 ?? 4C 8B CF 48 85 C0 4C 0F 45 C8 41 8D 42 ?? 99 41 23 D3 8D 1C ?? 41 8B 11 C1 FB 05 85 D2 75 ?? 0F 1F 80 00 00 00 00 FF C1 41 83 C0 20 89 4D ?? 44 89 45 ?? 3B CB 7F ?? 48 63 C1 C7 45 ?? FF FF FF FF 41 8B 14 ?? 85 D2 74 ?? 8B C2 F7 D8 23 C2 0F BD C8 89 45 ?? 74 ?? 41 8B C3 2B C1 EB ?? B8 20 00 00 00 44 2B C0 41 8D 40 ?? 89 45 ?? 41 3B C2 7E ?? 44 89 55 ?? 41 8B 56 ?? 41 BD FF FF FF FF 0F 10 55 ?? 8B CA 4C 89 75 ?? 0F 10 45 ?? 41 23 CB 44 8B C2 0F 11 55 ?? 41 D3 E5 44 8B CA 0F 11 45 ?? 41 C1 F8 05 41 83 E1 E0 66 0F 15 D2 45 8B FA F2 0F 11 55 ?? 44 89 6D ?? 89 55 ?? 0F 10 45 ?? 0F 10 4D ?? 0F 11 45 ?? 0F 11 4D ?? 41 3B D2 74 ?? 48 8B 47 ?? 4C 8B D7 48 85 C0 49 63 C8 4C 0F 45 D0 41 8D 47 ?? 99 41 23 D3 8D 1C ?? 41 8B 14 ?? C1 FB 05 41 23 D5 75 ?? 41 FF C0 41 83 C1 20 44 3B C3 7F ?? 49 63 C0 C7 45 ?? FF FF FF FF 41 8B 14 ?? 85 D2 74 ?? 8B C2 F7 D8 23 C2 0F BD C8 74 ?? 44 2B D9 EB ?? 41 BB 20 00 00 00 45 2B CB 41 8D 41 ?? 89 45 ?? 41 3B C7 7E ?? 44 89 7D ?? 48 8B 5D ?? 4C 8B BC 24 ?? ?? ?? ?? 4C 8B AC 24 ?? ?? ?? ?? 48 C1 EB 20 48 63 45 ?? 48 8B 55 ?? 3B C3 75 ?? 48 39 7D ?? 75 ?? 49 3B D6 74 ?? 48 8D 0C ?? 48 8B 02 48 8D 14 ?? 48 8B 4D ?? 4C 8D 42 ?? 48 8B 01 FF 50 ?? 8B 45 ?? 48 8D 4D ?? F7 D0 21 45 ?? E8 ?? ?? ?? ?? EB ?? 48 8B 4D ?? 48 8D 55 ?? E8 ?? ?? ?? ?? 48 8B 4D ?? 48 8B 01 FF 90 ?? ?? ?? ?? 4C 8B B4 24 ?? ?? ?? ?? 4C 8D 45 ?? 48 8B D8 48 8B CE 48 8B 45 ?? 8B D6 48 89 4D ?? 89 55 ?? 49 3B C0 74 ?? 39 48 ?? 74 ?? 4C 8B 00 4D 85 C0 74 ?? 49 8B 00 48 8D 55 ?? 49 8B C8 FF 50 ?? 8B 55 ?? 48 8B 4D ?? 48 89 75 ?? 89 75 ?? 85 D2 74 ?? 48 85 C9 74 ?? 48 8B 01 48 8D 55 ?? FF 50 ?? 48 8B 55 ?? 4C 8D 4D ?? 4C 8D 05 ?? ?? ?? ?? 48 8D 4D ?? E8 ?? ?? ?? ?? 48 8B D0 48 8B CB E8 ?? ?? ?? ?? 39 75 ?? 74 ?? 48 8B 4D ?? 48 85 C9 74 ?? 48 8B 01 33 D2 FF 50 ?? 48 8B 45 ?? 48 85 C0 74 ?? 45 33 C0 33 D2 48 8B C8 E8 ?? ?? ?? ?? 48 89 45 ?? 89 75 ?? EB ?? 48 8B 45 ?? 48 85 C0 74 ?? 48 8B C8 E8 ?? ?? ?? ?? 8B 45 ?? 48 8B 4D ?? 85 C0 74 ?? 48 85 C9 75 ?? 85 C0 74 ?? 48 85 C9 74 ?? 48 8B 01 33 D2 FF 50 ?? 48 8B 4D ?? 48 85 C9 74 ?? 45 33 C0 33 D2 E8 ?? ?? ?? ?? 48 8B C8 48 89 45 ?? 89 75 ?? 48 85 C9 74 ?? E8 ?? ?? ?? ?? 48 8B 4D ?? 48 8B 01 FF 90 ?? ?? ?? ?? 48 8B 45 ?? 49 89 04 24 48 8B 45 ?? 49 89 44 24 ?? 48 85 C0 74 ?? FF 40 ?? 48 8B 5D ?? 48 85 DB 74 ?? 83 6B ?? 01 75 ?? 48 8B 03 48 8B CB FF 10 83 6B ?? 01 75 ?? 48 8B 03 BA 01 00 00 00 48 8B CB FF 50 ?? 48 8B 9C 24 ?? ?? ?? ?? 49 8B C4 48 81 C4 D8 00 00 00 41 5C 5F 5E 5D C3");
	std::cout << "o_GetCurrentGames: 0x" << std::hex << (long long)found - (long long)baseAddr << std::endl;*/

	MH_CreateHook(module_base + 0x13da7d0, hk_GetMotd, reinterpret_cast<void**>(&o_GetMotd));
	MH_EnableHook(module_base + 0x13da7d0);

	MH_CreateHook(module_base + 0x13DA280, hk_GetCurrentGames, reinterpret_cast<void**>(&o_GetCurrentGames));
	MH_EnableHook(module_base + 0x13DA280);

	/*MH_CreateHook(module_base + 0x14ACB70, hk_ConcatUrl, reinterpret_cast<void**>(&o_ConcatUrl));
	MH_EnableHook(module_base + 0x14ACB70);*/

	MH_CreateHook(module_base + 0x14a1250, hk_SendRequest, reinterpret_cast<void**>(&o_SendRequest));
	MH_EnableHook(module_base + 0x14a1250);

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