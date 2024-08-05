﻿// TODO: remove this file entirely. The concept of a main.h
// is nonsensical from an organization persepctive

#pragma once
#include <Sig.hpp>
// TODO: this include gives HMODULE and stuff, for when this file eventually gets
// deleted and its contents moved elsewhere
//#include <windows.h> 
#include "sigs.h"

// Helper functions

long long FindSignature(HMODULE baseAddr, DWORD size, const char* title, const char* signature)
{
	const void* found = nullptr;
	found = Sig::find(baseAddr, size, signature);
	long long diff = 0;
	if (found != nullptr)
	{
		diff = (long long)found - (long long)baseAddr;
#ifndef NDEBUG
		//std::cout << title << ": 0x" << std::hex << diff << std::endl;
		printf("?? -> %s : 0x%llx\n", title, diff);
#endif
	}
#ifndef NDEBUG
	else
		printf("!! -> %s : nullptr\n", title);
		//std::cout << title << ": nullptr" << std::endl;
#endif

		return diff;

}

inline static void Ptch_Nop(unsigned char* address, int size)
{
	unsigned long protect[2];
	VirtualProtect((void*)address, size, PAGE_EXECUTE_READWRITE, &protect[0]);
	memset((void*)address, 0x90, size);
	VirtualProtect((void*)address, size, protect[0], &protect[1]);
}

inline static void Ptch_Repl(unsigned char* address, DWORD newVal)
{
	DWORD d;
	VirtualProtect((void*)address, 1, PAGE_EXECUTE_READWRITE, &d);
	*address = 0xEB; // Patch to JMP
	VirtualProtect((void*)address, 1, d, NULL);
}

// Hook macros

HMODULE baseAddr;
MODULEINFO moduleInfo;

#define DECL_HOOK(retType, funcType, args)    \
	typedef retType (*funcType##_t) args;		\
	funcType##_t o_##funcType;					\
	retType hk_##funcType args

#define HOOK_ATTACH(moduleBase, funcType) \
	MH_CreateHook(moduleBase + curBuild.offsets[F_##funcType], hk_##funcType, reinterpret_cast<void**>(&o_##funcType)); \
	MH_EnableHook(moduleBase + curBuild.offsets[F_##funcType]); 

#define HOOK_FIND_SIG(funcType) \
	if (curBuild.offsets[F_##funcType] == 0)\
		curBuild.offsets[F_##funcType] = FindSignature(baseAddr, moduleInfo.SizeOfImage, #funcType, signatures[F_##funcType]); \
	else printf("-> %s : (conf)\n", #funcType);
	//long long sig_##funcType = FindSignature(baseAddr, moduleInfo.SizeOfImage, #funcType, signatures[F_##funcType]);
