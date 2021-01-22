#include "pch.h"
#include "SidebarContent.h"
#include "GameLink.h"
#include <shobjidl.h> 
#include <DirectXPackedVector.h>
#include <DirectXMath.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace DirectX::PackedVector;
using namespace std;
namespace fs = std::filesystem;

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

bool SidebarContent::setActiveProfile(SidebarManager* sbM, std::string* name, bool force=false)
{
    if (*name == "")
    {
        // this happens if we can't read the shm
        // TODO: load a default profile that says that AW isn't running?
        // Or put other default info?
        return false;
    }
    if ((!force) && (m_activeProfile["meta"]["name"] == *name))
    {
        // no change
        return true;
    }
    nlohmann::json j;
    try {
        j = m_allProfiles.at(*name);
    }
    catch (std::out_of_range const& exc) {
        m_activeProfile["meta"]["name"] = *name;
        std::cerr << "Profile Name not found: " << *name << endl << exc.what() << endl;
        return false;
    }

    m_activeProfile = j;
    sbM->DeleteAll();
    UINT8 regstart = 0;
    string title = "";
    XMVECTOR regionColor;
    bool isFirstRegion = true;
    UINT8 numBlocks = m_activeProfile["sidebar"].size();
    sbM->SetNumberOfBlocks(numBlocks);
    for (UINT8 i = 0; i < numBlocks; i++)
    {
        nlohmann::json aj = m_activeProfile["sidebar"][i];
        if (aj["type"] == "RegionStart")
        {
            if (isFirstRegion)
            {
                isFirstRegion = false; // finished parsing first region header
            }
            else
            {
                // we finished the previous region
                // let's send it over to the manager
                UINT8 res = sbM->AddRegionWithBlocks(title, i - regstart, &regionColor);
                if (res == ERR_NO_BLOCKS_REMAINING)
                {
                    break;
                }
            }
            title = FormatBlockText(&aj);
            regstart = i;
            regionColor = Colors::CadetBlue;
            if (aj.contains("color"))
            {
                regionColor = XMVectorSet(aj["color"][0], aj["color"][1], aj["color"][2], aj["color"][3]);
            }
        }
    }
    // close the last region
    sbM->AddRegionWithBlocks(title, (UINT8)(m_activeProfile["sidebar"].size()) - regstart, &regionColor);
    return true;
}

void SidebarContent::LoadProfileUsingDialog(SidebarManager* sbM)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_DISABLE_OLE1DDE);
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
                    // TODO: PASS THAT TO THE PROFILE ACTIVATION
                    // Display the file name to the user.
                    if (SUCCEEDED(hr))
                    {
                        fs::directory_entry dir = fs::directory_entry(pszFilePath);
                        std::string profileName = OpenProfile(dir);
                        setActiveProfile(sbM, &profileName, true);
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

std::string SidebarContent::OpenProfile(std::filesystem::directory_entry entry)
{
    if (entry.is_regular_file() && (entry.path().extension().compare("json")))
    {
        nlohmann::json profile = ParseProfile(entry.path());
        if (profile != nullptr)
        {
            string name = profile["meta"]["name"];
            if (!name.empty())
            {
                m_allProfiles[name] = profile;
                return name;
            }
        }
    }
    return "";
}

nlohmann::json SidebarContent::ParseProfile(fs::path filepath)
{
    try
    {
        std::ifstream i(filepath);
        nlohmann::json j;
        i >> j;
        return j;
    }
    catch (nlohmann::detail::parse_error err) {
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

    nlohmann::json j = *pvar;
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
    else if (j["type"] == "lookup")
    {
        try
        {
            int x = *(pmem + memoffset);
            char buf[500];
            snprintf(buf, 500, "%s/0x%02x", j["lookup"].get<std::string>().c_str(), x);
            nlohmann::json::json_pointer jp(buf);
            return m_activeProfile.value(jp, "UNKNOWN VALUE");
        }
        catch (exception e)
        {
            OutputDebugStringA(e.what());
        }
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

std::string SidebarContent::FormatBlockText(nlohmann::json* pdata)
{
// serialize the SHM variables into strings
// and directly put them in the format string
    nlohmann::json data = *pdata;
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

bool SidebarContent::UpdateBlockText(SidebarManager* sbM, UINT8 blockId, nlohmann::json* pdata)
{
    nlohmann::json data = *pdata;
    // OutputDebugStringA((data.dump()+string("\n")).c_str());
    //sbM.ClearBlock(blockId);

    // default for "Content"
    XMVECTOR color = DirectX::Colors::GhostWhite;
    UINT8 flags = F_TXT_NORMAL;
    try
    {
        bool isGood = false;
        if (data["type"] == "Content")
        {
            isGood = true;
        }

        if (!isGood)
            return false;

        if (data.contains("color"))
        {
            color = XMVectorSet(data["color"][0], data["color"][1], data["color"][2], data["color"][3]);
        }

        string s = FormatBlockText(pdata);
        // OutputDebugStringA(s.c_str());
        // OutputDebugStringA("\n");
        sbM->DrawTextInBlock(blockId, s, &color, flags);
        return true;
    }
    catch (...)
    {
        return false;
    }
}
