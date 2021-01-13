#include "pch.h"
#include "SidebarManager.h"
#include <vector>
#include <iostream>
#include "Game.h"

constexpr UINT8 SIDEBAR_OUTSIDE_MARGIN = 5; // margin around the sidebar
constexpr UINT8 SIDEBAR_BLOCK_MARGIN = 5;   // margin between blocks

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
};

std::vector<RegionStruct> v_Regions;
std::vector<BlockStruct> v_Blocks;


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
    Initialize();
}

void SidebarManager::Initialize()
{
    SetNumberOfBlocks(1);
}

// Splits the sidebar into count blocks. Erases all info on the sidebar
// Also deletes all regions
// Returns number of blocks created
UINT8 SidebarManager::SetNumberOfBlocks(UINT8 count)
{
    v_Regions.clear();
    v_Blocks.clear();
    UINT8 blocksCount = count;
    if (count > SIDEBAR_MAX_BLOCKS)
        blocksCount = SIDEBAR_MAX_BLOCKS;
    int wW, wH;
    Game::GetDefaultSize(wW, wH);   // TODO: Check that the size is the usable area, not area with menu bar and the rest
    int blockHeight = (wH - 2*SIDEBAR_OUTSIDE_MARGIN) / blocksCount;
    int yStart = SIDEBAR_OUTSIDE_MARGIN;
    for (UINT8 i = 0; i < blocksCount; i++)
    {
        RECT r = {
            yStart + i * blockHeight,       // TOP
            SIDEBAR_OUTSIDE_MARGIN,         // LEFT
            SIDEBAR_OUTSIDE_MARGIN,         // RIGHT
            yStart + (i + 1) * blockHeight  // BOTTOM
        };
        BlockStruct b = { i, r };
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
    // TODO: clear block
    blockId;
    return false;
}

// Sets the number of blocks to span for a given region. Set count=0 to span to the end.
// Returns the RegionId, or an error
// Check that the returned UINT8 > ERR_RANGE_BEGIN for errors
UINT8 SidebarManager::AddRegionWithBlocks(UINT8 count)
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
    rs.title = std::string("");
    rs.blockStart = latest.blockEnd + 1;
    rs.blockEnd = GetNumberOfBlocks() - 1;
    UnionRect(&rs.boundsRect, &v_Blocks.at(rs.blockStart).boundsRect, &v_Blocks.at(rs.blockEnd).boundsRect);
    v_Regions.push_back(rs);
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

void SidebarManager::TestDraw()
{
    // TODO
}

void SidebarManager::DrawTextInRegion(UINT8 regionId, std::string text, UINT8 flags)
{
    std::string fontFile = "a2-sharplarge.spritefont";
    if (flags & F_TXT_BOLD & F_TXT_ITALIC)
    {
        fontFile = "a2-sharplarge-bolditalic.spritefont";
    }
    else if (flags & F_TXT_BOLD)
    {
        fontFile = "a2-sharplarge-bold.spritefont";
    }
    else if (flags & F_TXT_ITALIC)
    {
        fontFile = "a2-sharplarge-italic.spritefont";
    }
    // TODO: Shadow and Outline
    // See https://github.com/Microsoft/DirectXTK12/wiki/Drawing-text

    // TODO: Draw
    regionId;
    text;
}