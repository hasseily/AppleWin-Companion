#pragma once

constexpr UINT8 SIDEBAR_MAX_BLOCKS = 8;

constexpr UINT8 F_TXT_NORMAL	{ 0b0000'0001 };
constexpr UINT8 F_TXT_BOLD		{ 0b0000'0010 };
constexpr UINT8 F_TXT_ITALIC	{ 0b0000'0100 };
constexpr UINT8 F_TXT_SHADOW	{ 0b0000'1000 };
constexpr UINT8 F_TXT_OUTLINE	{ 0b0001'0000 };

enum
{
	ERR_NO_BLOCKS_REMAINING		= 255,
	ERR_ZERO_BLOCKS				= 254,
	ERR_RANGE_BEGIN				= 200
};

class SidebarManager
{

public:
	SidebarManager();
	void Initialize();
	// Splits the sidebar into count blocks. Erases all info on the sidebar
	// Also deletes all regions
	// Returns number of blocks created
	UINT8 SetNumberOfBlocks(UINT8 count);
	// Returns current number of blocks in the sidebar
	UINT8 GetNumberOfBlocks();
	// Clear all blocks without resetting the blocks or regions
	void Clear();
	// Clears given block number
	bool ClearBlock(UINT8 blockId);
	// Clears given region number
	bool ClearRegion(UINT8 regionId);
	// Sets the number of blocks to span for a given region. Set count=0 to span to the end.
	// Returns the RegionId, or an error
	// Check that the returned UINT8 > ERR_RANGE_BEGIN for errors
	UINT8 AddRegionWithBlocks(UINT8 count);
	// Deletes the last region created
	bool DeleteLastRegion();

	void DrawTextInRegion(UINT8 regionId, std::string text, UINT8 flags);
};

static std::wstring s2ws(const std::string& s);


