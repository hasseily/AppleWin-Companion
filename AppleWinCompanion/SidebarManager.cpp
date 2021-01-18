#include "pch.h"
#include "SidebarManager.h"
#include <vector>
#include <iostream>

struct RegionStruct
{
    UINT8 id;
    RECT boundsRect{};
    std::string title;
    UINT8 blockStart;
    UINT8 blockEnd;
};
struct BlockStruct
{
    UINT8 id;
    RECT boundsRect;
    BlockType type;
};

std::vector<RegionStruct> v_Regions;
std::vector<BlockStruct> v_Blocks;

// Size of the client frame inside the window
int m_clientFrameWidth = 0;
int m_clientFrameHeight = 0;
float m_aspectRatio = 1.f;

static std::wstring s2ws(const std::string& s)
{
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
}

SidebarManager::SidebarManager()
{
    v_Regions = {};
    v_Blocks = {};
    v_Regions.reserve(SIDEBAR_MAX_BLOCKS);
    v_Blocks.reserve(SIDEBAR_MAX_BLOCKS);
    Initialize();
}

void SidebarManager::Initialize()
{
    fontsAvailable = {
    L"a2-12pt.spritefont",
    L"a2-bold-12pt.spritefont",
    L"a2-italic-12pt.spritefont",
    L"a2-bolditalic-12pt.spritefont"
    };
    v_Regions = std::vector<RegionStruct>();
    v_Blocks = std::vector<BlockStruct>();
    SetNumberOfBlocks(SIDEBAR_MAX_BLOCKS);
    allTexts = std::map<UINT8, TextSpriteStruct>();
}

// Properties
float SidebarManager::GetSidebarRatio() noexcept
{
    return ((float)SIDEBAR_WIDTH / ((float)(APPLEWIN_WIDTH + SIDEBAR_WIDTH)));
}

void SidebarManager::GetDefaultSize(int& width, int& height) noexcept
{
    width = APPLEWIN_WIDTH + SIDEBAR_WIDTH;
    height = APPLEWIN_HEIGHT;
}

float SidebarManager::GetAspectRatio() noexcept
{
    return m_aspectRatio;
}

void SidebarManager::GetClientFrameSize(int& width, int& height) noexcept
{
    width = m_clientFrameWidth;
    height = m_clientFrameHeight;
}

void SidebarManager::SetAspectRatio(float aspect) noexcept
{
    m_aspectRatio = aspect;
}

void SidebarManager::SetClientFrameSize(const int width, const int height) noexcept
{
    m_clientFrameWidth = width;
    m_clientFrameHeight = height;
}

// Splits the sidebar into count blocks. Erases all info on the sidebar. Also deletes all regions.
// All size calculations are done on the default size. It is up to Game.cpp to scale accordingly
// Returns number of blocks created
// TODO:    This system does not allow for a region header, so region title will overlap.
//          Maybe make each block the size of a text line + padding, and when creating a region
//          allocate the first block to the region header
UINT8 SidebarManager::SetNumberOfBlocks(UINT8 count)
{
    allTexts.clear();
    v_Regions.clear();
    v_Blocks.clear();
    UINT8 blocksCount = count;
    if (count > SIDEBAR_MAX_BLOCKS)
        blocksCount = SIDEBAR_MAX_BLOCKS;
    int wW, wH;
    GetDefaultSize(wW, wH);
    int blockHeight = (wH - 2*SIDEBAR_OUTSIDE_MARGIN) / blocksCount;
    int xStart = APPLEWIN_WIDTH + SIDEBAR_OUTSIDE_MARGIN;
    int yStart = APPLEWIN_HEIGHT + SIDEBAR_OUTSIDE_MARGIN;
    for (UINT8 i = 0; i < blocksCount; i++)
    {
        RECT r = {
            xStart,                         // LEFT
            yStart + i * blockHeight,       // TOP
            xStart + SIDEBAR_WIDTH,         // RIGHT
            yStart + (i + 1) * blockHeight  // BOTTOM
        };
        BlockStruct b = { i, r, BlockType::Empty };
        v_Blocks.push_back(b);
    }
    return blocksCount;
}

// Returns current number of blocks in the sidebar
UINT8 SidebarManager::GetNumberOfBlocks()
{
    return (UINT8)v_Blocks.size();
}

// Clear all blocks without resetting the blocks or regions
void SidebarManager::Clear()
{
    for (UINT8 i = 0; i < GetNumberOfBlocks(); i++)
    {
        ClearBlock(i);
    }
}

// Clears given block number
bool SidebarManager::ClearBlock(UINT8 blockId)
{
    try
    {
        v_Blocks[blockId].type = BlockType::Empty;
        BOOL ret = (bool)allTexts.erase(blockId);
        return ret;
    }
    catch (std::out_of_range const& exc) {
        std::cerr << "Error clearing block: " << blockId << "\n" << exc.what() << "\n";
        return false;
    }
}

// Sets the number of blocks to span for a given region. Set count=0 to span to the end.
// Right now we only reserve the top block for the region title. We don't do anything special with the last block
// Returns the RegionId, or an error
// Check that the returned UINT8 > ERR_RANGE_BEGIN for errors
UINT8 SidebarManager::AddRegionWithBlocks(std::string title, UINT8 count)
{
    RegionStruct latest = v_Regions.back();
    if (count == 0)
        count = GetNumberOfBlocks() - latest.blockEnd - 1;

    if ((GetNumberOfBlocks() - latest.blockEnd - 1) > count)
    {
        return ERR_NO_BLOCKS_REMAINING;
    }
    RegionStruct rs;
    rs.id = latest.id + 1;
    rs.title = title;
    rs.blockStart = latest.blockEnd + 1;
    rs.blockEnd = GetNumberOfBlocks() - 1;
    UnionRect(&rs.boundsRect, &v_Blocks.at(rs.blockStart).boundsRect, &v_Blocks.at(rs.blockEnd).boundsRect);
    v_Regions.push_back(rs);

    // Set the block types to content for this region, except for the first block which will be RegionStart
    if (count > 1)  // a block count of 1 would just have the region title
    {
        for (size_t i = 1; i < count; i++)
        {
            v_Blocks[rs.blockStart + i].type = BlockType::Content;
        }
    }

    // Also add the title to the region, where we use the same code as the block thing
    v_Blocks[rs.blockStart].type = BlockType::RegionStart;
    DrawTextInBlock(rs.blockStart, rs.title, DirectX::Colors::SeaShell, F_TXT_REGIONTITLE);
    return rs.id;
}

// Deletes the last region created
bool SidebarManager::DeleteLastRegion()
{
    if (v_Regions.empty())
        return false;
    RegionStruct rs = v_Regions.back();
    ClearRegion(rs.id);
    v_Regions.pop_back();
    return true;
}

// Clears given region number, without resetting anything
// Essentially clears the blocks that the region spans
bool SidebarManager::ClearRegion(UINT8 regionId)
{
    RegionStruct rs;
    try {
        rs = v_Regions.at(regionId);
    }
    catch (std::out_of_range const& exc) {
        std::cerr << "No region with id: " << regionId << "\n" << exc.what() << "\n";
        return false;
    }
    for (UINT8 i = rs.blockStart; i < rs.blockEnd; i++)
    {
        ClearBlock(i);
    }
    return true;
}

///////////////////////////////////////////////////////////////
// DRAWING SECTION
///////////////////////////////////////////////////////////////

void SidebarManager::DrawTextInBlock(UINT8 blockId, std::string text)
{
    DrawTextInBlock(blockId, text, DirectX::Colors::GhostWhite, F_TXT_NORMAL);
}

void SidebarManager::DrawTextInBlock(UINT8 blockId, std::string text, DirectX::XMVECTORF32 color, UINT8 flags)
{
    // This method should update an array of block data, using fontid, text, color and font position calculated based on the region
    // Then Game.cpp should call up this region array and render it inside Render()

    BlockStruct bs;
    try {
        bs = v_Blocks.at(blockId);
    }
    catch (std::out_of_range const& exc) {
        char buf[sizeof(exc.what()) + 100];
        sprintf_s(buf, "Block doesn't exist: %s\n", exc.what());
        OutputDebugStringA(buf);
        return;
    };

    TextSpriteStruct tss;
    tss.fontId = FontDescriptors::A2FontRegular;
    tss.color = color;  // e.g. DirectX::Colors::GhostWhite
    tss.text = text;
    tss.blockId = blockId;
    // Default drawing position for inside a block. Overridden when drawing region title
    tss.position = { (float)(bs.boundsRect.left + SIDEBAR_BLOCK_PADDING), (float)(bs.boundsRect.top - SIDEBAR_BLOCK_PADDING/2) };

    if (flags & F_TXT_REGIONTITLE)
    {
        tss.fontId = FontDescriptors::A2FontBold;   // TODO: Have a special region title font
        tss.position = { (float)(bs.boundsRect.left + SIDEBAR_BLOCK_PADDING), (float)(bs.boundsRect.top + SIDEBAR_BLOCK_PADDING) };
    }
    else if (flags & F_TXT_BOLD & F_TXT_ITALIC)
    {
        tss.fontId = FontDescriptors::A2FontBoldItalic;
    }
    else if (flags & F_TXT_BOLD)
    {
        tss.fontId = FontDescriptors::A2FontBold;
    }
    else if (flags & F_TXT_ITALIC)
    {
        tss.fontId = FontDescriptors::A2FontItalic;
    }
    // TODO: Shadow and Outline
    // See https://github.com/Microsoft/DirectXTK12/wiki/Drawing-text


    
    allTexts.insert_or_assign(blockId, tss);
}