#pragma once
#include <filesystem>
#include "SidebarManager.h"

class SidebarContent
{
public:
	SidebarContent::SidebarContent();
	void Initialize();
	void UpdateAllSidebarText(SidebarManager sbM);
	void UpdateBlockText(SidebarManager sbM, UINT8 blockId); 
private:
	void LoadProfilesFromDisk();
	void ParseProfile(std::filesystem::path filepath);
};

