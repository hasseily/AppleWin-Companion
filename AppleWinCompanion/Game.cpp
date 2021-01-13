//
// Game.cpp
//

#include "pch.h"
#include "Game.h"
#include "SidebarManager.h"
#include "GameLink.h"
#include "HAUtils.h"

extern void ExitGame() noexcept;

using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;
using namespace HA;


constexpr int APPLEWIN_WIDTH = 600;
constexpr int APPLEWIN_HEIGHT = 420;
constexpr int SIDEBAR_WIDTH = 200;

// Size of the client frame inside the window
int m_clientFrameWidth = 0;
int m_clientFrameHeight = 0;
float m_aspectRatio = 1.f;

// AppleWin video texture
D3D12_SUBRESOURCE_DATA g_textureData;
ComPtr<ID3D12Resource> g_textureUploadHeap;

SidebarManager m_sbM;

Game::Game() noexcept(false)
{
    g_textureData = {};
    m_sbM = SidebarManager();

    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

Game::~Game()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }
    GameLink::Destroy();
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

    m_gamePad = std::make_unique<GamePad>();
    m_keyboard = std::make_unique<Keyboard>();

    wchar_t buff[MAX_PATH];
    DX::FindMediaFile(buff, MAX_PATH, L"Background.jpg");
    m_bgImage = HA::LoadBGRAImage(buff, m_bgImageWidth, m_bgImageHeight);

    m_previousFrameCount = 0;
    m_previousGameLinkFrameSequence = 0;
    m_useGameLink = false;

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

#pragma region Activate GameLink

// Select which texture to show: The default background or the gamelink data
D3D12_RESOURCE_DESC Game::ChooseTexture()
{
    D3D12_RESOURCE_DESC txtDesc = {};
    txtDesc.MipLevels = txtDesc.DepthOrArraySize = 1;
    txtDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    txtDesc.SampleDesc.Count = 1;
    txtDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    // Check if the shared memory exists. If so, use it.
    char buf[500];
    if (m_useGameLink)
    {
        auto res = GameLink::Init();
        if (res)
        {
            auto fbI = GameLink::GetFrameBufferInfo();
            txtDesc.Width = fbI.width;
            txtDesc.Height = fbI.height;
            g_textureData.pData = fbI.frameBuffer;
            g_textureData.SlicePitch = fbI.bufferLength;
            sprintf_s(buf, "GameLink up with Width %d, Height %d\n", fbI.width, fbI.height);
            OutputDebugStringA(buf);
        }
    }
    // If GameLink is active, it could return a framebuffer of size (0,0).
    // This means it's in a waiting state for a program to be loaded
    if (!GameLink::IsActive() || (txtDesc.Width == 0))
    {
        txtDesc.Width = m_bgImageWidth;
        txtDesc.Height = m_bgImageHeight;
        g_textureData.pData = m_bgImage.data();
        g_textureData.SlicePitch = m_bgImage.size();
    }
    g_textureData.RowPitch = static_cast<LONG_PTR>(txtDesc.Width * sizeof(uint32_t));
    return txtDesc;
}

#pragma endregion

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

    // Game logic here

    auto pad = m_gamePad->GetState(0);
    if (pad.IsConnected())
    {
        if (pad.IsViewPressed())
        {
            // Do something when gamepad keys are pressed
        }
    }
    auto kb = m_keyboard->GetState();
    // TODO: Should we pass the keystrokes back to AppleWin here?
    if (kb.Escape)
    {
        // Do something when escape or other keys pressed
    }

    elapsedTime;

    PIXEndEvent();
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // Don't try to render anything before the first Update.
    uint32_t currFrameCount = m_timer.GetFrameCount();
    if (currFrameCount == 0)
    {
        return;
    }

    // Every m_framesDelay see if GameLink is active
    if ((currFrameCount - m_previousFrameCount) > m_framesDelay)
    {
        char buf[500];
        if (GameLink::IsActive())
        {
            UINT16 seq = GameLink::GetFrameSequence();
            if (seq == m_previousGameLinkFrameSequence)
            {
                if (seq > 0)
                {
                    // GameLink isn't doing anything, could be dead
                       // Revert to the original texture
                    sprintf_s(buf, "Destroying GameLink. Seq: %d - Prev: %d\n", seq, m_previousGameLinkFrameSequence);
                    OutputDebugStringA(buf);
                    GameLink::Destroy();
                    m_useGameLink = false;
                    ChooseTexture();
                }
                else
                {
                    // GameLink is waiting for a game, the texture size is (0,0)
                    sprintf_s(buf, "Gamelink waiting for a game. Seq: %d - Prev: %d\n", seq, m_previousGameLinkFrameSequence);
                    OutputDebugStringA(buf);
                }
            }
            else
            {
                if (m_previousGameLinkFrameSequence == 0)
                {
                    // A game was activated on AppleWin since our last check, let's set the texture the right size
                    ChooseTexture();
                }
                sprintf_s(buf, "Gamelink active. Seq: %d - Prev: %d\n", seq, m_previousGameLinkFrameSequence);
                OutputDebugStringA(buf);
                ChooseTexture();
            }
            m_previousGameLinkFrameSequence = seq;
        }
        else
        {
            // Look for an active gamelink
            sprintf_s(buf, "Looking for active GameLink. Cur: %d - Prev: %d\n", currFrameCount, m_previousFrameCount);
            OutputDebugStringA(buf);
            m_useGameLink = true;
            ChooseTexture();
        }
        m_previousFrameCount = currFrameCount;
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Add rendering code here.

    // Drawing video texture
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->ResourceBarrier(1, &barrier);
    UpdateSubresources(commandList, m_texture.Get(), g_textureUploadHeap.Get(), 0, 0, 1, &g_textureData);
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &barrier);

    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(m_pipelineState.Get());

    auto heap = m_srvHeap.Get();
    commandList->SetDescriptorHeaps(1, &heap);

    commandList->SetGraphicsRootDescriptorTable(0, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

    // Set necessary state.
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);

    // Draw quad.
    commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
    // End drawing video texture

    // Drawing text
    // TODO: Remove demo rendering of text
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
    // End drawing text

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
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::Black, 0, nullptr);
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
float Game::GetSidebarRatio() noexcept
{
    return ((float)SIDEBAR_WIDTH / (float)APPLEWIN_HEIGHT);
}

void Game::GetDefaultSize(int& width, int& height) noexcept
{
    width = (int)((float)APPLEWIN_WIDTH * (1.f + GetSidebarRatio()));
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

    /// <summary>
    /// Start of Font resource uploading to GPU
    /// </summary>
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

    //////////////////////////////////////////////////

    /// <summary>
    /// Start of AppleWin texture info uploading to GPU
    /// </summary>

        // Create descriptor heaps.
    {
        // Describe and create a shader resource view (SRV) heap for the texture.
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = 1;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        DX::ThrowIfFailed(
            device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(m_srvHeap.ReleaseAndGetAddressOf())));
    }

    // Create a root signature with one sampler and one texture
    {
        CD3DX12_DESCRIPTOR_RANGE descRange = {};
        descRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

        CD3DX12_ROOT_PARAMETER rp = {};
        rp.InitAsDescriptorTable(1, &descRange, D3D12_SHADER_VISIBILITY_PIXEL);

        // Use a static sampler that matches the defaults
        // https://msdn.microsoft.com/en-us/library/windows/desktop/dn913202(v=vs.85).aspx#static_sampler
        D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.MaxAnisotropy = 16;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        rootSignatureDesc.Init(1, &rp, 1, &samplerDesc,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        if (FAILED(hr))
        {
            if (error)
            {
                OutputDebugStringA(reinterpret_cast<const char*>(error->GetBufferPointer()));
            }
            throw DX::com_exception(hr);
        }

        DX::ThrowIfFailed(
            device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));
    }

    // Create the pipeline state, which includes loading shaders.
    auto vertexShaderBlob = DX::ReadData(L"VertexShader.cso");

    auto pixelShaderBlob = DX::ReadData(L"PixelShader.cso");

    static const D3D12_INPUT_ELEMENT_DESC s_inputElementDesc[2] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,  0 },
        { "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,  0 },
    };

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { s_inputElementDesc, _countof(s_inputElementDesc) };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { vertexShaderBlob.data(), vertexShaderBlob.size() };
    psoDesc.PS = { pixelShaderBlob.data(), pixelShaderBlob.size() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.DSVFormat = m_deviceResources->GetDepthBufferFormat();
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_deviceResources->GetBackBufferFormat();
    psoDesc.SampleDesc.Count = 1;
    DX::ThrowIfFailed(
        device->CreateGraphicsPipelineState(&psoDesc,
            IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf())));

    CD3DX12_HEAP_PROPERTIES heapUpload(D3D12_HEAP_TYPE_UPLOAD);

    // Create vertex buffer.

    {
        float sbR = GetSidebarRatio();
        static const Vertex s_vertexData[4] =
        {
            { { -1.0f, -1.0f, 0.5f, 1.0f }, { 0.f, 1.f } },
            { {  (1.f - sbR * 2.0f), -1.0f, 0.5f, 1.0f }, { 1.f, 1.f } },
            { {  (1.f - sbR * 2.0f),  1.0f, 0.5f, 1.0f }, { 1.f, 0.f } },
            { { -1.0f,  1.0f, 0.5f, 1.0f }, { 0.f, 0.f } },
        };

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(s_vertexData));

        DX::ThrowIfFailed(
            device->CreateCommittedResource(&heapUpload,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_vertexBuffer.ReleaseAndGetAddressOf())));

        // Copy the quad data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
        DX::ThrowIfFailed(
            m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, s_vertexData, sizeof(s_vertexData));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = sizeof(s_vertexData);
    }

    // Create index buffer.
    {
        static const uint16_t s_indexData[6] =
        {
            3,1,0,
            2,1,3,
        };

        // See note above
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(s_indexData));

        DX::ThrowIfFailed(
            device->CreateCommittedResource(&heapUpload,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_indexBuffer.ReleaseAndGetAddressOf())));

        // Copy the data to the index buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
        DX::ThrowIfFailed(
            m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, s_indexData, sizeof(s_indexData));
        m_indexBuffer->Unmap(0, nullptr);

        // Initialize the index buffer view.
        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        m_indexBufferView.SizeInBytes = sizeof(s_indexData);
    }

    // Create texture.
    auto commandList = m_deviceResources->GetCommandList();
    commandList->Reset(m_deviceResources->GetCommandAllocator(), nullptr);

    {
        D3D12_RESOURCE_DESC txtDesc = ChooseTexture();

        CD3DX12_HEAP_PROPERTIES heapDefault(D3D12_HEAP_TYPE_DEFAULT);

        DX::ThrowIfFailed(
            device->CreateCommittedResource(
                &heapDefault,
                D3D12_HEAP_FLAG_NONE,
                &txtDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(m_texture.ReleaseAndGetAddressOf())));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

        // Create the GPU upload buffer.
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        DX::ThrowIfFailed(
            device->CreateCommittedResource(
                &heapUpload,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(g_textureUploadHeap.GetAddressOf())));

        UpdateSubresources(commandList, m_texture.Get(), g_textureUploadHeap.Get(), 0, 0, 1, &g_textureData);

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &barrier);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = txtDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    DX::ThrowIfFailed(commandList->Close());
    m_deviceResources->GetCommandQueue()->ExecuteCommandLists(1, CommandListCast(&commandList));

    // Wait until assets have been uploaded to the GPU.
    m_deviceResources->WaitForGpu();

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
    m_texture.Reset();
    m_indexBuffer.Reset();
    m_vertexBuffer.Reset();
    m_pipelineState.Reset();
    m_rootSignature.Reset();
    m_srvHeap.Reset();
    m_font.reset();
    m_resourceDescriptors.reset();
    m_spriteBatch.reset();
    m_graphicsMemory.reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion
