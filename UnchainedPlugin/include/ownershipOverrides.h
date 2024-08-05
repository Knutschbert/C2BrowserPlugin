#pragma once

DECL_HOOK(FOwnershipResponse*, GetOwnershipFromPlayerControllerAndState, (FOwnershipResponse* result, void* PlayerController, void* PlayerState, void* AssetIdToCheck, bool BaseOnly)) {
	FOwnershipResponse* response = o_GetOwnershipFromPlayerControllerAndState(result, PlayerController, PlayerState, AssetIdToCheck, BaseOnly);
	response->owned = true;
	response->level = 0;
	return response;
}

DECL_HOOK(FOwnershipResponse*, CanUseLoadoutItem, (ATBLPlayerController* _this, FOwnershipResponse* result, const void* InLoadOutSelection, const void* InItem)) {
	auto response = o_CanUseLoadoutItem(_this, result, InLoadOutSelection, InItem); response->owned = true;
	response->level = 0;
	result->owned = true;
	return response;
}

DECL_HOOK(FOwnershipResponse*, CanUseCharacter, (ATBLPlayerController* _this, FOwnershipResponse* result, const void* CharacterSubclass)) {
	auto response = o_CanUseCharacter(_this, result, CharacterSubclass);
	response->level = 0;
	response->owned = true;
	return response;
}