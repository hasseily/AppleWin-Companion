#include "pch.h"
#include "SidebarContent.h"
#include "SidebarManager.h"
#include <filesystem>
#include "nlohmann/json.hpp"
#include <iostream>

namespace fs = std::filesystem;
using namespace nlohmann;

SidebarContent::SidebarContent()
{
    Initialize();
}

void SidebarContent::Initialize()
{
    LoadProfilesFromDisk();
}


void SidebarContent::LoadProfilesFromDisk()
{
    fs::path currentDir = fs::current_path();
    currentDir += "\\Profiles";
    
    for (const auto& entry : fs::directory_iterator(currentDir))
    {
        if (entry.is_regular_file() && (entry.path().extension().compare("json")))
        {
            ParseProfile(entry.path());
        }
    }
}

void SidebarContent::ParseProfile(fs::path filepath)
{
    try
    {
        std::ifstream i(filepath);
        json j;
        i >> j;
        j;
        std::cout << std::setw(4) << j << std::endl;
        OutputDebugStringA(j.dump().c_str());
    }
    catch (detail::parse_error err) {
        std::cerr << "Error parsing profile: " << err.what() << "\n";
        return;
    }
}

void SidebarContent::UpdateAllSidebarText(SidebarManager sbM)
{
    // TODO: Add your implementation code here.
}


void SidebarContent::UpdateBlockText(SidebarManager sbM, UINT8 blockId)
{
    // TODO: Add your implementation code here.
}
