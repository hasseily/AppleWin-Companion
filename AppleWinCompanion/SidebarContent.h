#pragma once
#include <filesystem>
#include "SidebarManager.h"
#include "nlohmann/json.hpp"
#include <map>

constexpr UINT8 SIDEBAR_MAX_VARS_IN_BLOCK = 9;

class SidebarContent
{
public:
	SidebarContent::SidebarContent();
	void Initialize();
	bool setActiveProfile(SidebarManager* sbM, std::string name);
	void UpdateAllSidebarText(SidebarManager* sbM);
	bool UpdateBlockText(SidebarManager* sbM, UINT8 blockId, nlohmann::json* pdata);
private:
	void LoadProfilesFromDisk();
	nlohmann::json ParseProfile(std::filesystem::path filepath);
	std::string SerializeVariable(nlohmann::json* pvar);
	std::string FormatBlockText(nlohmann::json* pdata);

	std::map<std::string, nlohmann::json> m_allProfiles;
	nlohmann::json m_activeProfile;
};

