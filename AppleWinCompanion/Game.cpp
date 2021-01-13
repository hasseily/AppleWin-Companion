//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;

constexpr int APPLEWIN_WIDTH = 600;
constexpr int APPLEWIN_HEIGHT = 420;
constexpr int SIDEBAR_WIDTH = 200;

// Size of the client frame inside the window
int m_clientFrameWidth = 0;
int m_clientFrameHeight = 0;
float m_aspectRatio = 1.f;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

Game::~Game()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{
    // Set the frame and aspect sizes, we need to know what area of the window we're allowed to work with
    SetClientFrameSize(width, height);  // Client area
    RECT wR;
    GetWindowRect(window, &wR);
    // Now that we have the real window size, set the correct aspect ratio that we'll keep all through the life of the app
    SetAspectRatio(static_cast<FLOAT>(wR.right - wR.left) / static_cast<FLOAT>(wR.bottom - wR.top));

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
    /*
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
    */
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");

    float elapsedTime = float(timer.GetElapsedSeconds());

    // TODO: Add your game logic here.
    elapsedTime;

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Add rendering code here.

    // TODO: Demo rendering of text
    ID3D12DescriptorHeap* heaps[] = { m_resourceDescriptors->Heap() };
    commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

    m_spriteBatch->Begin(commandList);

    const wchar_t* output = L"Hello World";

    Vector2 origin = m_font->MeasureString(output) / 2.f;

    m_font->DrawString(m_spriteBatch.get(), output,
        m_fontPos, Colors::White, 0.f, origin);

    const wchar_t* output2 = L"Hello World";

    m_font->DrawString(m_spriteBatch.get(), output2,
        m_fontPos + Vector2(0, 20.f), Colors::Red, 0.f, origin);

    m_spriteBatch->End();

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());
    PIXEndEvent();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::CornflowerBlue, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto viewport = m_deviceResources->GetScreenViewport();
    auto scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);

    PIXEndEvent(commandList);
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void Game::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();

    // TODO: Game window is being resized.
}

// Properties
int Game::GetSidebarWidth() noexcept
{
    return SIDEBAR_WIDTH;
}

void Game::GetDefaultSize(int& width, int& height) noexcept
{
    width = APPLEWIN_WIDTH + GetSidebarWidth();
    height = APPLEWIN_HEIGHT;
}

float Game::GetAspectRatio() noexcept
{
    return m_aspectRatio;
}

void Game::GetClientFrameSize(int& width, int& height) noexcept
{
    width = m_clientFrameWidth;
    height = m_clientFrameHeight;
}

void Game::SetAspectRatio(float aspect) noexcept
{
    m_aspectRatio = aspect;
}

void Game::SetClientFrameSize(const int width, const int height) noexcept
{
    m_clientFrameWidth = width;
    m_clientFrameHeight = height;
}

#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto command_queue = m_deviceResources->GetCommandQueue();
    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);
    m_resourceDescriptors = std::make_unique<DescriptorHeap>(device, Descriptors::Count);

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    wchar_t buff[MAX_PATH];
    DX::FindMediaFile(buff, MAX_PATH, L"a2.spritefont");
    m_font = std::make_unique<SpriteFont>(device, resourceUpload,
        buff,
        m_resourceDescriptors->GetCpuHandle(Descriptors::A2Font),
        m_resourceDescriptors->GetGpuHandle(Descriptors::A2Font));

    RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
    SpriteBatchPipelineStateDescription pd(rtState);
    m_spriteBatch = std::make_unique<SpriteBatch>(device, resourceUpload, pd);

    auto uploadResourcesFinished = resourceUpload.End(command_queue);

    uploadResourcesFinished.wait();
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    D3D12_VIEWPORT viewport = m_deviceResources->GetScreenViewport();
    m_spriteBatch->SetViewport(viewport);
    // TODO: dont position fonts here!
    m_fontPos.x = viewport.Width / 2.f;
    m_fontPos.y = viewport.Height / 2.f;
}

void Game::OnDeviceLost()
{
    m_graphicsMemory.reset();
    m_font.reset();
    m_resourceDescriptors.reset();
    m_spriteBatch.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
