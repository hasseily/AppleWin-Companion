#pragma once
#include <vector>
#include <map>
#include <SimpleMath.h>

constexpr UINT8 SIDEBAR_MAX_BLOCKS = 20;
constexpr UINT8 SIDEBAR_OUTSIDE_MARGIN = 5; // margin around the sidebar
constexpr UINT8 SIDEBAR_BLOCK_PADDING = 2;   // PADDING around each block

constexpr int APPLEWIN_WIDTH = 600;
constexpr int APPLEWIN_HEIGHT = 420;
constexpr int SIDEBAR_WIDTH = 200;

constexpr UINT8 F_TXT_NORMAL{ 0b0000'0001 };
constexpr UINT8 F_TXT_BOLD{ 0b0000'0010 };
constexpr UINT8 F_TXT_ITALIC{ 0b0000'0100 };
constexpr UINT8 F_TXT_SHADOW{ 0b0000'1000 };
constexpr UINT8 F_TXT_OUTLINE{ 0b0001'0000 };
constexpr UINT8 F_TXT_REGIONTITLE{ 0b0010'0000 };

enum class FontDescriptors
{
	A2FontRegular,
	A2FontBold,
	A2FontItalic,
	A2FontBoldItalic,
	Count
};

enum class BlockType
{
	Content,
	RegionStart,
	RegionEnd,
	Empty,
	Count
};

// Any text to draw (or redraw) on the screen
// TODO: See if we need a flag to determine if it should redraw
struct TextSpriteStruct
{
	int blockId;
	FontDescriptors fontId;
	std::string text;
	DirectX::XMVECTORF32 color;
	DirectX::SimpleMath::Vector2 position;
};

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
	UINT8 AddRegionWithBlocks(std::string title, UINT8 count);
	// Delete all regions
	void DeleteAll();
	// Deletes the last region created
	bool DeleteLastRegion();

	void DrawTextInBlock(UINT8 blockId, std::string text);
	void DrawTextInBlock(UINT8 blockId, std::string text, DirectX::XMVECTORF32 color, UINT8 flags);

	static float GetSidebarRatio() noexcept;
	static void GetDefaultSize(int& width, int& height) noexcept;
	static float GetAspectRatio() noexcept;
	static void GetClientFrameSize(int& width, int& height) noexcept;
	void SetAspectRatio(float aspect) noexcept;
	void SetClientFrameSize(const int width, const int height) noexcept;

	// Properties
	std::vector<std::wstring> fontsAvailable;
	std::map<UINT8, TextSpriteStruct> allTexts;
};

static std::wstring s2ws(const std::string& s);


