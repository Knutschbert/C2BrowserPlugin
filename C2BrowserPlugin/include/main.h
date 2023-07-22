#pragma once
#include <Sig/Sig.hpp>
#include "sigs.h"

// UE Types
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

long long FindSignature(HMODULE baseAddr, DWORD size, const char* title, const char* signature)
{
	const void* found = nullptr;
	found = Sig::find(baseAddr, size, signature);
	long long diff = 0;
	if (found != nullptr)
	{
		diff = (long long)found - (long long)baseAddr;
#ifdef _DEBUG
		std::cout << title << ": 0x" << std::hex << diff << std::endl;
#endif
	}
#ifdef _DEBUG
	else
		std::cout << title << ": nullptr" << std::endl;
#endif

	return diff;

}

// Helper functions

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
		str += 2;
		i++;
	}
	std::cout << std::endl;
	return i;
}

// Hook macros

HMODULE baseAddr;
MODULEINFO moduleInfo;

#define DECL_HOOK(retType, funcType, args)    \
	typedef retType (*funcType##_t) args;		\
	funcType##_t o_##funcType;					\
	retType hk_##funcType args

#define HOOK_ATTACH(moduleBase, funcType) \
	MH_CreateHook(moduleBase + sig_##funcType, hk_##funcType, reinterpret_cast<void**>(&o_##funcType)); \
	MH_EnableHook(moduleBase + sig_##funcType); 

#define HOOK_FIND_SIG(funcType) \
	long long sig_##funcType = FindSignature(baseAddr, moduleInfo.SizeOfImage, #funcType, signatures[F_##funcType]);
