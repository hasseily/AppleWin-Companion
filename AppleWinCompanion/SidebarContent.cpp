#include "pch.h"
#include "SidebarContent.h"
#include "GameLink.h"
#include <shobjidl.h> 

using namespace std;
namespace fs = std::filesystem;
using namespace nlohmann;

static string SIDEBAR_FORMAT_PLACEHOLDER("{}");

static UINT8* pmem;
static int memsize;

SidebarContent::SidebarContent()
{
    Initialize();
}

void SidebarContent::Initialize()
{
    LoadProfilesFromDisk();
    // Get memory start address
    auto res = GameLink::Init();
    if (res)
    {
        pmem = GameLink::GetMemoryBasePointer();
        memsize = GameLink::GetMemorySize();
    }
}

bool SidebarContent::setActiveProfile(SidebarManager* sbM, std::string name)
{
    if (name == "")
    {
        // this happens if we can't read the shm
        // TODO: load a default profile that says that AW isn't running?
        // Or put other default info?
        return false;
    }
    if (m_activeProfile["meta"]["name"] == name)
    {
        // no change
        return true;
    }
    json j;
    try {
        j = m_allProfiles.at(name);
    }
    catch (std::out_of_range const& exc) {
        m_activeProfile["meta"]["name"] = name;
        std::cerr << "Profile Name not found: " << name << endl << exc.what() << endl;
        return false;
    }

    m_activeProfile = j;
    sbM->DeleteAll();
    UINT8 regstart = 0;
    string title = "";
    bool isFirstRegion = true;
    for (UINT8 i = 0; i < m_activeProfile["sidebar"].size(); i++)
    {
        json aj = m_activeProfile["sidebar"][i];
        if (aj["type"] == "RegionStart")
        {
            if (isFirstRegion)
            {
                title = FormatBlockText(&aj);
                regstart = 0;
                isFirstRegion = false; // finished parsing first region header
            }
            else
            {
                // we finished the previous region
                // let's send it over to the manager
                sbM->AddRegionWithBlocks(title, i - regstart);
                title = FormatBlockText(&aj);
                regstart = i;
            }
        }
    }
    // close the last region
    sbM->AddRegionWithBlocks(title, (UINT8)(m_activeProfile["sidebar"].size()) - regstart - 1);
    return true;
}

void SidebarContent::LoadProfileUsingDialog()
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
        COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr))
    {
        IFileOpenDialog* pFileOpen;

        // Create the FileOpenDialog object.
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

        if (SUCCEEDED(hr))
        {
            // Show the Open dialog box.
            hr = pFileOpen->Show(NULL);

            // Get the file name from the dialog box.
            if (SUCCEEDED(hr))
            {
                IShellItem* pItem;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr))
                {
                    PWSTR pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                    // Display the file name to the user.
                    if (SUCCEEDED(hr))
                    {
                        MessageBoxW(NULL, pszFilePath, L"File Path", MB_OK);
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
}

void SidebarContent::LoadProfilesFromDisk()
{
    if(!m_allProfiles.empty())
        m_allProfiles.clear();
    fs::path currentDir = fs::current_path();
    currentDir += "\\Profiles";
    
    for (const auto& entry : fs::directory_iterator(currentDir))
    {
        OpenProfile(entry);
    }
}

bool SidebarContent::OpenProfile(std::filesystem::directory_entry entry)
{
    if (entry.is_regular_file() && (entry.path().extension().compare("json")))
    {
        json profile = ParseProfile(entry.path());
        if (profile != nullptr)
        {
            string name = profile["meta"]["name"];
            if (!name.empty())
            {
                m_allProfiles[name] = profile;
                return true;
            }
        }
    }
    return false;
}

json SidebarContent::ParseProfile(fs::path filepath)
{
    try
    {
        std::ifstream i(filepath);
        json j;
        i >> j;
        j;
        // OutputDebugStringA(j.dump().c_str());
        return j;
    }
    catch (detail::parse_error err) {
        std::cerr << "Error parsing profile: " << err.what() << "\n";
        return nullptr;
    }
}

// Turn a variable into a string
// using the following json format:
/*
    {
        "memstart": "0x1165A",
        "length" : 16,
        "type" : "ascii_high"
    }
*/

std::string SidebarContent::SerializeVariable(nlohmann::json* pvar)
{
    if (pmem == NULL)
        return "";

    json j = *pvar;
    // initialize variables
    // OutputDebugStringA((j.dump()+string("\n")).c_str());
    string s = "";
    if (j.count("length") != 1)
        return s;
    int length = j["length"];

    if (j.count("memstart") != 1)
        return s;
    int memoffset = std::stoul(j["memstart"].get<std::string>(), nullptr, 0);

    if (j.count("type") != 1)
        return s;

    if ((length == 0) || (length > memsize))
    {
        return s;
    }


    // now we have the memory offset and length, and we need to parse
    if (j["type"] == "ascii")
    {
        for (size_t i = 0; i < length; i++)
        {
            if (*(pmem + memoffset + i) == '\0')
                return s;
            s.append(1, (*(pmem + memoffset + i)));
        }
        return s;
    }
    else if (j["type"] == "ascii_high")
    {
        // ASCII-high is basically ASCII shifted by 0x80
        for (size_t i = 0; i < length; i++)
        {
            if (*(pmem + memoffset + i) == '\0')
                return s;
            s.append(1, (*(pmem + memoffset + i) - 0x80));
        }
        return s;
    }
    else if (j["type"] == "int_bigendian")
    {
        int x = 0;
        for (int i = 0; i < length; i++)
        {
            x += (*(pmem + memoffset + i)) * (int)pow(0x100, i);
        }
        s = to_string(x);
        return s;
    }
    else if (j["type"] == "int_littleendian")
    {
        int x = 0;
        for (int i = 0; i < length; i++)
        {
            x += (*(pmem + memoffset + i)) * (int)pow(0x100, (length - i - 1));
        }
        s = to_string(x);
        return s;
    }
    return s;
}

// This method formats the whole text block using the format string and vars
// Kind of like sprintf using the following json format:
/*
{
    "type": "Content",
    "template" : "Left: {} - Right: {}",
    "vars" : [
                {
                    "memstart": "0x1165A",
                    "length" : 4,
                    "type" : "ascii_high"
                },
                {
                    "memstart": "0x1165E",
                    "length" : 4,
                    "type" : "ascii_high"
                }
            ]
}
*/

std::string SidebarContent::FormatBlockText(json* pdata)
{
// serialize the SHM variables into strings
// and directly put them in the format string
    json data = *pdata;
    string txt = "";
    try
    {
        txt = data["template"].get<string>();
    }
    catch (...)
    {
        return txt;
    }
    array<string, SIDEBAR_MAX_VARS_IN_BLOCK> sVars;
    for (size_t i = 0; i < data["vars"].size(); i++)
    {
        sVars[i] = SerializeVariable(&(data["vars"][i]));
        txt.replace(txt.find(SIDEBAR_FORMAT_PLACEHOLDER), SIDEBAR_FORMAT_PLACEHOLDER.length(), sVars[i]);
    }
    return txt;
}

void SidebarContent::UpdateAllSidebarText(SidebarManager* sbM)
{
    // TODO: Probably shouldn't be calling this method every frame!!!
    if (!setActiveProfile(sbM, GameLink::GetEmulatedProgramName()))
    {
        return;
    }
    // OutputDebugStringA((m_activeProfile["sidebar"].dump()+string("\n")).c_str());
    UINT8 i = 0;
    for (auto& element : m_activeProfile["sidebar"])
    {
        if (!UpdateBlockText(sbM, i, &element))
        {
            std::cout << "Error updating block: " << i << endl;
        }
        i++;
    }
}

// Update and send for display a block of text
// using the following json format:
/*
{
    "type": "Content",
    "template" : "Left: {} - Right: {}",
    "vars" : [
                {
                    "memstart": "0x1165A",
                    "length" : 4,
                    "type" : "ascii_high"
                },
                {
                    "memstart": "0x1165E",
                    "length" : 4,
                    "type" : "ascii_high"
                }
            ]
}
*/

bool SidebarContent::UpdateBlockText(SidebarManager* sbM, UINT8 blockId, json* pdata)
{
    json data = *pdata;
    // OutputDebugStringA((data.dump()+string("\n")).c_str());
    //sbM.ClearBlock(blockId);

    // default for "Content"
    DirectX::XMVECTORF32 color = DirectX::Colors::GhostWhite;
    UINT8 flags = F_TXT_NORMAL;
    bool isGood = false;
    if (data["type"] == "Content")
    {
        isGood = true;
    }

    if (!isGood)
        return false;

    string s = FormatBlockText(pdata);
    // OutputDebugStringA(s.c_str());
    // OutputDebugStringA("\n");
    sbM->DrawTextInBlock(blockId, s, color, flags);
    return true;
}
