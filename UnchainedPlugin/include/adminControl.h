#pragma once

#include <regex>

DECL_HOOK(FString, ConsoleCommand, (void* this_ptr, FString const& str, bool b)) {
#ifdef _DEBUG_
	static void* cached_this;
	if (this_ptr == NULL) {
		this_ptr = cached_this;
	}
	else {
		if (cached_this != this_ptr) {
			cached_this = this_ptr;
			//std::cout << "0x" << std::hex << this_ptr << std::endl;
		}
	}

	log("[RCON][DEBUG]: PlayerController Exec called with:");
	logWideString(str.str);

	const wchar_t* interceptPrefix = L"RCON_INTERCEPT";
	//if the command starts with the intercept prefix
	//TODO: clean up mutex stuff here. Way too sloppy to be final
	if (wcslen(str.str) >= 14 && memcmp(str.str, interceptPrefix, lstrlenW(interceptPrefix) * sizeof(wchar_t)) == 0) {

		log("[RCON][DEBUG]: Intercept command detected");
	}
#endif

	return o_ConsoleCommand(this_ptr, str, b);
}

DECL_HOOK(void, ExecuteConsoleCommand, (FString2* param)) {
	log("EXECUTECONSOLECMD:");
	logWideString(param->str);
	o_ExecuteConsoleCommand(param);
}

//FText* __cdecl FText::AsCultureInvariant(FText* __return_storage_ptr__, FString* param_1)
DECL_HOOK(void*, FText_AsCultureInvariant, (void* ret_ptr, FString2* input)) {
	// This is extremely loud in the console
	//if (input->str != NULL) {
	//	printf("FText_AsCultureInvariant: ");
	//	wprintf(input->str);
	//  printf("\n");
	//}
	return o_FText_AsCultureInvariant(ret_ptr, input);
}

//void __thiscall ATBLGameMode::BroadcastLocalizedChat(ATBLGameMode *this,FText *param_1,Type param_2)
DECL_HOOK(void, BroadcastLocalizedChat, (void* game_mode, FText* text, uint8_t chat_type)) {
	log("BroadcastLocalizedChat");
	return o_BroadcastLocalizedChat(game_mode, text, chat_type);
}

// TODO: make this a proper header-impl file, with other related things
// maybe put it in commandline.h/cpp??
bool extractPlayerCommand(const wchar_t* input, std::wstring& playerName, std::wstring& command) {
	// Define the regular expression pattern
	std::wregex pattern(L"(.+) <0>: /cmd (.+)");

	// Convert the input to a wstring
	std::wstring inputString(input);

	// Define a wsmatch object to store the matched groups
	std::wsmatch matches;

	// Try to match the pattern in the input string
	if (std::regex_search(inputString, matches, pattern)) {
		if (matches.size() == 3) {
			playerName = matches[1].str();
			command = matches[2].str();
			return true; // Match found
		}
	}

	return false; // No match found
}

// TODO: make this a proper header-impl file, with other related things
bool IsServerStart()
{
	bool isHeadless = CmdGetParam(L"-nullrhi") != -1;
	bool isSetToTravel = CmdGetParam(L"--next-map-name") != -1;
	return isHeadless || isSetToTravel;
}

// TODO: nasty global. Make this a static local
// and use a getter-pattern call to this function to receive it
// wherever its value is needed. Make sure this pattern will actually
// work, too
void* CurGameMode = NULL;
// ATBLGameMode * __cdecl UTBLSystemLibrary::GetTBLGameMode(UObject *param_1)
DECL_HOOK(void*, GetTBLGameMode, (void* uobj)) {
	//log("GetTBLGameMode");
	CurGameMode = o_GetTBLGameMode(uobj);
	return CurGameMode;
}

/*
void __thiscall
APlayerController::ClientMessage
		  (APlayerController *this,FString *param_1,FName param_2,float param_3)
*/
DECL_HOOK(void, ClientMessage, (void* this_ptr, FString* param_1, void* param_2, float param_3))
{
	std::wstring commandLine = GetCommandLineW();
	size_t flagLoc = commandLine.find(L"--next-map");
	bool egs = CmdGetParam(L"-epicapp=Peppermint") != -1;
	static uint64_t init = false;
	//log("ClientMessage");

	char* pValue;
	size_t len;
	char ladBuff[256];
	errno_t err = _dupenv_s(&pValue, &len, "LOCALAPPDATA");

	// TODO: make this nicer
	strncpy_s(ladBuff, 256, pValue, len);
	strncpy_s(ladBuff + len - 1, 256 - len, "\\Chivalry 2\\Saved\\Logs\\Unchained", 34);

	_mkdir(ladBuff);
	sprintf_s(ladBuff, 256, "%s\\Chivalry 2\\Saved\\Logs\\Unchained\\ClientMessage%s%s.log",
		pValue, (IsServerStart() ? "-server" : "-client"), (egs ? "-egs" : "-steam"));
	if (!init)
		log(ladBuff);
	//log(ladBuff);
	std::wofstream  out(ladBuff, init++ ? std::ios_base::app : std::ios_base::trunc);
	if (out.is_open())
		out << init << L":: " << param_1->str << std::endl;
	else
		log("Can't open ClientMessage log for writing.");

	/*if (flagLoc == std::wstring::npos) {
		o_ClientMessage(this_ptr, param_1, param_2, param_3);
		return;
	}*/

	static std::wstring playerName;
	auto command = std::make_unique<std::wstring>();

	if (extractPlayerCommand(param_1->str, playerName, *command)) {
		std::wcout << L"Player Name: " << playerName << std::endl;
		std::wcout << L"Command: " << command->c_str() << std::endl;

		FText txt;
		void* res = o_FText_AsCultureInvariant(&txt, new FString2(L"Command detected"));
		if (res != NULL && CurGameMode != NULL)
		{
			log("[ChatCommands] Could print server text");
			o_BroadcastLocalizedChat(CurGameMode, (FText*)res, 3);
		}

		log("[ChatCommands] Executing command");
		logWideString(const_cast<wchar_t*>(command->c_str()));

		auto empty = FString2(command->c_str());

		o_ExecuteConsoleCommand(&empty);
	}
	else {
		//std::wcout << L"No valid match found." << std::endl;
	}
	o_ClientMessage(this_ptr, param_1, param_2, param_3);
}


#ifdef PRINT_CLIENT_MSG
/*
void __thiscall
APlayerController::ClientMessage
		  (APlayerController *this,FString *param_1,FName param_2,float param_3)
*/
DECL_HOOK(void, ClientMessage, (void* this_ptr, FString* param_1, void* param_2, float param_3))
{
	std::wstring commandLine = GetCommandLineW();
	size_t flagLoc = commandLine.find(L"--next-map");
	bool egs = CmdGetParam(L"-epicapp=Peppermint") != -1;
	static uint64_t init = false;
	log("ClientMessage");

	wprintf(L"CLIENT_MESSAGE: %ls %.2f\n", param_1->str, param_3);

	std::wstring ws(param_1->str);
	std::string msg_str(ws.begin(), ws.end());
	o_ClientMessage(this_ptr, param_1, param_2, param_3);
}

#endif // PRINT_CLIENT_MSG