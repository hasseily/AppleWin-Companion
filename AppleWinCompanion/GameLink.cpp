#include "pch.h"
#include "GameLink.h"

//------------------------------------------------------------------------------
// Local Definitions
//------------------------------------------------------------------------------

#define SYSTEM_NAME		"AppleWin"
#define PROTOCOL_VER		4
#define GAMELINK_MUTEX_NAME		"DWD_GAMELINK_MUTEX_R4"
#define GAMELINK_MMAP_NAME		"DWD_GAMELINK_MMAP_R4"

using namespace GameLink;

//------------------------------------------------------------------------------
// Shared Memory Structure
//------------------------------------------------------------------------------

#pragma pack( push, 1 )

	//
	// sSharedMMapFrame_R1
	//
	// Server -> Client Frame. 32-bit RGBA up to MAX_WIDTH x MAX_HEIGHT
	//
struct sSharedMMapFrame_R1
{
	UINT16 seq;
	UINT16 width;
	UINT16 height;

	UINT8 image_fmt; // 0 = no frame; 1 = 32-bit 0xAARRGGBB
	UINT8 reserved0;

	UINT16 par_x; // pixel aspect ratio
	UINT16 par_y;

	enum { MAX_WIDTH = 1280 };
	enum { MAX_HEIGHT = 1024 };

	enum { MAX_PAYLOAD = MAX_WIDTH * MAX_HEIGHT * 4 };
	UINT8 buffer[MAX_PAYLOAD];
};

//
// sSharedMMapInput_R2
//
// Client -> Server Input Data
//

struct sSharedMMapInput_R2
{
	float mouse_dx;
	float mouse_dy;
	UINT8 ready;
	UINT8 mouse_btn;
	UINT keyb_state[8];

	enum { READY_NO = 0 };					// Input not ready
	enum { READY_GC = 1 };					// Input from GC
	enum { READY_OTHER = 17 };				// Input from other app
};

//
// sSharedMMapPeek_R2
//
// Memory reading interface, an obsolete way of requesting RAM address values.
// This is unnecessary now for reading RAM as the RAM is completely mapped at the end of the SHM
// However we can use this interface to request processor registers!
struct sSharedMMapPeek_R2
{
	enum { PEEK_SPECIAL_PC_H = UINT_MAX - 1 };	// Set this address to request program counter high byte
	enum { PEEK_SPECIAL_PC_L = UINT_MAX - 2 };	// Set this address to request program counter low byte
	enum { PEEK_LIMIT = 16 * 1024 };

	UINT addr_count;
	UINT addr[PEEK_LIMIT];
	UINT8 data[PEEK_LIMIT];
};

//
// sSharedMMapBuffer_R1
//
// General buffer (64Kb)
//
struct sSharedMMapBuffer_R1
{
	enum { BUFFER_SIZE = (64 * 1024) };

	UINT16 payload;
	UINT8 data[BUFFER_SIZE];
};

//
// sSharedMMapAudio_R1
//
// Audio control interface.
//
struct sSharedMMapAudio_R1
{
	UINT8 master_vol_l;
	UINT8 master_vol_r;
};

//
// sSharedMemoryMap_R4
//
// Memory Map (top-level object)
//

constexpr int FLAG_WANT_KEYB = 1 << 0;
constexpr int FLAG_WANT_MOUSE = 1 << 1;
constexpr int FLAG_NO_FRAME = 1 << 2;
constexpr int FLAG_PAUSED = 1 << 3;
constexpr int SYSTEM_MAXLEN = 64;
constexpr int PROGRAM_MAXLEN = 260;

struct sSharedMemoryMap_R4
{
	UINT8 version; // = PROTOCOL_VER
	UINT8 flags;
	char system[SYSTEM_MAXLEN] = {}; // System name.
	char program[PROGRAM_MAXLEN] = {}; // Program name. Zero terminated.
	UINT program_hash[4] = { 0,0,0,0 }; // Program code hash (256-bits)

	sSharedMMapFrame_R1 frame;
	sSharedMMapInput_R2 input;
	sSharedMMapPeek_R2 peek;
	sSharedMMapBuffer_R1 buf_tohost;
	sSharedMMapBuffer_R1 buf_recv; // a message to us.
	sSharedMMapAudio_R1 audio;

	// added for protocol v4
	UINT ram_size;

	sSharedMMapInput_R2 input_other;	// A second app's input channel so it isn't clobbered by GC

};

#pragma pack( pop )

//------------------------------------------------------------------------------
// Local Data
//------------------------------------------------------------------------------

static HANDLE g_mutex_handle;
static HANDLE g_mmap_handle;

static bool g_TrackOnly;

static sSharedMemoryMap_R4* g_p_shared_memory;

constexpr int MEMORY_MAP_CORE_SIZE = sizeof(sSharedMemoryMap_R4);
static UINT8* ramPointer;

//------------------------------------------------------------------------------
// Mutex methods
//------------------------------------------------------------------------------

bool GetMutex()
{
	g_mutex_handle = OpenMutexA(SYNCHRONIZE, FALSE, GAMELINK_MUTEX_NAME);
	if (g_mutex_handle == 0)
		return false;
	return true;
}

void CloseMutex()
{
	if (g_mutex_handle != 0)
	{
		CloseHandle(g_mutex_handle);
		g_mutex_handle = NULL;
	}
}

//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------

int GameLink::Init()
{
	if (g_p_shared_memory)
		return 1;

	g_mmap_handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, GAMELINK_MMAP_NAME);
	if (g_mmap_handle)
	{
//		UINT8 *shm = reinterpret_cast<UINT8*>(
//			MapViewOfFile(g_mmap_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0)
//			);
		g_p_shared_memory = reinterpret_cast<sSharedMemoryMap_R4*>(
			MapViewOfFile(g_mmap_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0)
			);

		if (g_p_shared_memory)
		{
			// Make sure to always request the PC of the processor
			g_p_shared_memory->peek.addr_count = 2;
			g_p_shared_memory->peek.addr[0] = sSharedMMapPeek_R2::PEEK_SPECIAL_PC_H;
			g_p_shared_memory->peek.addr[1] = sSharedMMapPeek_R2::PEEK_SPECIAL_PC_L;
			// The ram is right after the end of the shared memory pointer here
			ramPointer = reinterpret_cast<UINT8*>(g_p_shared_memory + 1);
			if (GetMutex()) {
				// All is good, tell the emulator to go native video, we'll take care of the flipping in hardware!
				SendCommand(std::string(":videonative"));
				return 1;
			}
			OutputDebugStringW(L"WARNING: Found shared memory but couldn't get mutex!\n");
		}
		// tidy up file mapping.
		CloseHandle(g_mmap_handle);
		g_mmap_handle = NULL;
		g_p_shared_memory = NULL;
	}
	// Failure
	return 0;
}

void GameLink::Destroy()
{
	CloseMutex();
	g_p_shared_memory = NULL;
}

std::string GameLink::GetEmulatedProgramName()
{
	if (g_p_shared_memory)
		return std::string(g_p_shared_memory->program);
	return "";
}

int GameLink::GetMemorySize()
{
	if (g_p_shared_memory)
		return g_p_shared_memory->ram_size;
	return 0;
}

UINT8* GameLink::GetMemoryBasePointer()
{
	return ramPointer;
}

UINT8 GameLink::GetPeekAt(UINT position)
{
	if (g_p_shared_memory)
	{
		if (position < g_p_shared_memory->peek.addr_count)
			return g_p_shared_memory->peek.data[position];
	}
	return 0;
}

bool GameLink::IsActive()
{
	return (g_p_shared_memory != NULL);
}

bool GameLink::IsTrackingOnly()
{
	int flags = g_p_shared_memory->flags;
	return (flags & FLAG_NO_FRAME);
}

void GameLink::SendCommand(std::string command)
{
	UINT16 sz = (UINT16)command.size() + 1;
	g_p_shared_memory->buf_tohost.payload = sz;
	snprintf((char *)g_p_shared_memory->buf_tohost.data, sz, "%s", command.c_str());
}

void GameLink::Pause()
{
	SendCommand(std::string(":pause"));
}

void GameLink::Reset()
{
	SendCommand(std::string(":reset"));
}

void GameLink::Shutdown()
{
	SendCommand(std::string(":shutdown"));
}

void GameLink::SetSoundVolume(UINT8 main, UINT8 mockingboard)
{
	if (main < 0)
		main = 0;
	if (main > 100)
		main = 100;
	if (mockingboard < 0)
		mockingboard = 0;
	if (mockingboard > 100)
		mockingboard = 100;
	DWORD dwWaitResult = WaitForSingleObject(g_mutex_handle, 3000);
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		g_p_shared_memory->audio.master_vol_l = main;
		g_p_shared_memory->audio.master_vol_r = mockingboard;
		ReleaseMutex(g_mutex_handle);
		break;
	case WAIT_ABANDONED:
		ReleaseMutex(g_mutex_handle);
		[[fallthrough]];
	case WAIT_TIMEOUT:
		[[fallthrough]];
	case WAIT_FAILED:
		[[fallthrough]];
	default:
		break;
	}
	return;
}

int GameLink::GetSoundVolumeMain()
{
	DWORD dwWaitResult = WaitForSingleObject(g_mutex_handle, 3000);
	int ret = 0;
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		ret = g_p_shared_memory->audio.master_vol_l;
		ReleaseMutex(g_mutex_handle);
		break;
	case WAIT_ABANDONED:
		ReleaseMutex(g_mutex_handle);
		[[fallthrough]];
	case WAIT_TIMEOUT:
		[[fallthrough]];
	case WAIT_FAILED:
		[[fallthrough]];
	default:
		break;
	}
	return ret;
}

int GameLink::GetSoundVolumeMockingboard()
{
	DWORD dwWaitResult = WaitForSingleObject(g_mutex_handle, 3000);
	int ret = 0;
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		ret = g_p_shared_memory->audio.master_vol_r;
		ReleaseMutex(g_mutex_handle);
		break;
	case WAIT_ABANDONED:
		ReleaseMutex(g_mutex_handle);
		[[fallthrough]];
	case WAIT_TIMEOUT:
		[[fallthrough]];
	case WAIT_FAILED:
		[[fallthrough]];
	default:
		break;
	}
	return ret;
}

void GameLink::SendKeystroke(UINT iVK_Code, LPARAM lParam)
{
	DWORD dwWaitResult = WaitForSingleObject(g_mutex_handle, 3000);
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
	{
		// Tell AppleWin we're ready and we're using directly iVK code and lparam
		// Nasty hack but it's not worth doing it cleanly for now.
		// To enable this we pass the value GAMELINK_IVK_HACK in the "ready" field
		// We use the input_other struct because Grid Cartographer would otherwise clobber 75% of the keystrokes
		// This way we can run AppleWin + GC + Companion cleanly

		g_p_shared_memory->input_other.ready = sSharedMMapInput_R2::READY_OTHER;
		g_p_shared_memory->input_other.keyb_state[0] = iVK_Code;
		g_p_shared_memory->input_other.keyb_state[1] = (UINT)lParam;
		g_p_shared_memory->input_other.keyb_state[2] = 0;
		g_p_shared_memory->input_other.keyb_state[3] = 0;
		g_p_shared_memory->input_other.keyb_state[4] = 0;
		g_p_shared_memory->input_other.keyb_state[5] = 0;
		g_p_shared_memory->input_other.keyb_state[6] = 0;
		g_p_shared_memory->input_other.keyb_state[7] = 0;

		ReleaseMutex(g_mutex_handle);
		break;
	}
	case WAIT_ABANDONED:
		ReleaseMutex(g_mutex_handle);
		[[fallthrough]];
	case WAIT_TIMEOUT:
		[[fallthrough]];
	case WAIT_FAILED:
		[[fallthrough]];
	default:
		break;
	}
}

sFramebufferInfo GameLink::GetFrameBufferInfo()	// TODO: STACK OVERFLOW
{
	sFramebufferInfo fbI = sFramebufferInfo();
	DWORD dwWaitResult = WaitForSingleObject(g_mutex_handle, 1000);
	switch (dwWaitResult)
	{
	case WAIT_ABANDONED:
		OutputDebugStringW(L"Abandoned\n");
		ReleaseMutex(g_mutex_handle);
		break;
	case WAIT_FAILED:
		OutputDebugStringW(L"Failed\n");
		break;
	case WAIT_TIMEOUT:
		OutputDebugStringW(L"Timeout in getting mutex for frame buffer info. Still grabbing the read-only data anyway\n");
		[[fallthrough]];
	default:
		sSharedMMapFrame_R1* f = &g_p_shared_memory->frame;
		fbI.frameBuffer = f->buffer;
		fbI.width = f->width;
		fbI.height = f->height;
		fbI.imageFormat = f->image_fmt;
		if (fbI.imageFormat == 0)
		{
			fbI.bufferLength = 0;
		}
		else
		{
			fbI.bufferLength = fbI.width * fbI.height * sizeof(UINT32);
		}
		fbI.parX = f->par_x;
		fbI.parY = f->par_y;
		fbI.wantsMouse = (g_p_shared_memory->flags & FLAG_WANT_MOUSE);
		ReleaseMutex(g_mutex_handle);
		break;
	}
	return fbI;
}

UINT16 GameLink::GetFrameSequence()
{
	return g_p_shared_memory->frame.seq;
}


