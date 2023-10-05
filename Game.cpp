//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

#include "CommonStates.h"
#include "ReadData.h"
#include "DirectXTK12/Src/Geometry.h"

extern void ExitGame() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
    // TODO: Provide parameters for swapchain format, depth/stencil format, and backbuffer count.
    //   Add DX::DeviceResources::c_AllowTearing to opt-in to variable rate displays.
    //   Add DX::DeviceResources::c_EnableHDR for HDR10 display.
    //   Add DX::DeviceResources::c_ReverseDepth to optimize depth buffer clears for 0 instead of 1.
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
    // Initialize input devices.
    m_keyboard = std::make_unique<Keyboard>();
    m_mouse = std::make_unique<Mouse>();
    m_mouse->SetWindow(window);
    m_mouse->SetMode(Mouse::MODE_RELATIVE);

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    m_fenceEvent.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));
    if (!m_fenceEvent.IsValid())
    {
	    throw std::exception("CreateEvent");
    }

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

    // Update color multiplier data.
    static float rIncrement = elapsedTime / 100.f;
    static float gIncrement = elapsedTime / 200.f;
    static float bIncrement = elapsedTime / 300.f;

    m_colorMultiplier.x += rIncrement;
    m_colorMultiplier.y += gIncrement;
    m_colorMultiplier.z += bIncrement;

    if (m_colorMultiplier.x >= 1.0 || m_colorMultiplier.x <= 0.0)
    {
        m_colorMultiplier.x = m_colorMultiplier.x >= 1.0 ? 1.0 : 0.0;
        rIncrement = -rIncrement;
    }
    if (m_colorMultiplier.y >= 1.0 || m_colorMultiplier.y <= 0.0)
    {
        m_colorMultiplier.y = m_colorMultiplier.y >= 1.0 ? 1.0 : 0.0;
        gIncrement = -gIncrement;
    }
    if (m_colorMultiplier.z >= 1.0 || m_colorMultiplier.z <= 0.0)
    {
        m_colorMultiplier.z = m_colorMultiplier.z >= 1.0 ? 1.0 : 0.0;
        bIncrement = -bIncrement;
    }

    // Handle keyboard input.
    float verticalMove = 0.0f;
    float horizontalMove = 0.0f;

    const auto keyboard = m_keyboard->GetState();
    if (keyboard.Escape)
    {
        ExitGame();
    }
    else if (keyboard.W)
    {
        verticalMove = 3.0f * elapsedTime;
    }
    else if (keyboard.S)
    {
        verticalMove = -3.0f * elapsedTime;
    }
    else if (keyboard.A)
    {
        horizontalMove = -3.0f * elapsedTime;
    }
    else if (keyboard.D)
    {
        horizontalMove = 3.0f * elapsedTime;
    }

    // Handle mouse input.
    const auto mouse = m_mouse->GetState();
    m_camYaw += mouse.x * elapsedTime / 10.0f;
    m_camPitch += mouse.y * elapsedTime / 10.0f;

    // Set view matrix based on camera position and orientation.
    m_camRotationMatrix = XMMatrixRotationRollPitchYaw(m_camPitch, m_camYaw, 0.0f);
    m_camLookTarget = XMVector3TransformCoord(DEFAULT_FORWARD_VECTOR, m_camRotationMatrix);
    m_camLookTarget = XMVector3Normalize(m_camLookTarget);

    m_camRight = XMVector3TransformCoord(DEFAULT_RIGHT_VECTOR, m_camRotationMatrix);
    m_camUp = XMVector3TransformCoord(DEFAULT_UP_VECTOR, m_camRotationMatrix);
    m_camForward = XMVector3TransformCoord(DEFAULT_FORWARD_VECTOR, m_camRotationMatrix);

    m_camPosition += horizontalMove * m_camRight;
    m_camPosition += verticalMove * m_camForward;

    m_camLookTarget = m_camPosition + m_camLookTarget;

    m_viewMatrix = XMMatrixLookAtLH(m_camPosition, m_camLookTarget, m_camUp);

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

    // Check to see if the GPU is keeping up
    int frameIdx = m_deviceResources->GetCurrentFrameIndex();
    int numBackBuffers = m_deviceResources->GetBackBufferCount();
    uint64_t completedValue = m_fence->GetCompletedValue();

    // if frame index is reset to zero it may temporarily be smaller than the last GPU signal
    if ((frameIdx > completedValue) && (frameIdx - completedValue > numBackBuffers))
    {
        // GPU not caught up, wait for at least one available frame
        DX::ThrowIfFailed(m_fence->SetEventOnCompletion(frameIdx - numBackBuffers, m_fenceEvent.Get()));
        WaitForSingleObjectEx(m_fenceEvent.Get(), INFINITE, FALSE);
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();
    Clear();

    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // TODO: Add your rendering code here.

    // Index into the available constant buffers based on the number
    // of draw calls. We've allocated enough for a known number of
    // draw calls per frame times the number of back buffers
    unsigned int cbIndex = c_numDrawCalls * (frameIdx % numBackBuffers);

    // Set the root signature and pipeline state for the command list.
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(m_pipelineState.Get());

    // Set the constant data.
    ConstantBuffer cbData;
    cbData.colorMultiplier = XMLoadFloat4(&m_colorMultiplier);
    cbData.worldMatrix = XMMatrixTranspose(m_worldMatrix);
    cbData.viewMatrix = XMMatrixTranspose(m_viewMatrix);
    cbData.projectionMatrix = XMMatrixTranspose(m_projectionMatrix);

    memcpy(&m_cbMappedData[cbIndex].constants, &cbData, sizeof(ConstantBuffer));

    // Bind the constants to the shader.
    auto baseGpuAddress = m_cbGpuAddress + sizeof(PaddedConstantBuffer) * cbIndex;
    commandList->SetGraphicsRootConstantBufferView(c_rootParameterCB, baseGpuAddress);

    // Set necessary state.
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);

    // Draw triangle.
    commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
    baseGpuAddress += sizeof(PaddedConstantBuffer);
    ++cbIndex;

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(m_deviceResources->GetCommandQueue(), PIX_COLOR_DEFAULT, L"Present");
    m_deviceResources->Present();

    // If using the DirectX Tool Kit for DX12, uncomment this line:
    // m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());

    // GPU will signal an increasing value each frame
    m_deviceResources->GetCommandQueue()->Signal(m_fence.Get(), frameIdx);

    PIXEndEvent(m_deviceResources->GetCommandQueue());
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();
    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");

    // Clear the views.
    auto const rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto const dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::Black, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto const viewport = m_deviceResources->GetScreenViewport();
    auto const scissorRect = m_deviceResources->GetScissorRect();
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
    auto const r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnDisplayChange()
{
    m_deviceResources->UpdateColorSpace();
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();

    // TODO: Game window is being resized.
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 1280;
    height = 720;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();

    // Check Shader Model 6 support
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_0 };
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)))
        || (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0))
    {
#ifdef _DEBUG
        OutputDebugStringA("ERROR: Shader Model 6.0 is not supported!\n");
#endif
        throw std::runtime_error("Shader Model 6.0 is not supported!");
    }

    // If using the DirectX Tool Kit for DX12, uncomment this line:
    // m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    // TODO: Initialize device dependent objects here (independent of window size).

	// Create root signature with one constant buffer view
    {
        CD3DX12_ROOT_PARAMETER rootParameters[1];
        rootParameters[0].InitAsConstantBufferView(c_rootParameterCB, 0);

        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        DX::ThrowIfFailed(
            D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        DX::ThrowIfFailed(
            device->CreateRootSignature(
                0, 
                signature->GetBufferPointer(), 
                signature->GetBufferSize(),
                IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));
    }

    // Create the constant buffer memory and map the CPU and GPU addresses.
    {
        CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        size_t const cbSize = c_numDrawCalls * m_deviceResources->GetBackBufferCount() * sizeof(PaddedConstantBuffer);

        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
        DX::ThrowIfFailed(
            device->CreateCommittedResource(
                &uploadHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_cbUploadHeap.ReleaseAndGetAddressOf())));

        DX::ThrowIfFailed(m_cbUploadHeap->Map(0, nullptr, reinterpret_cast<void**>(&m_cbMappedData)));

        m_cbGpuAddress = m_cbUploadHeap->GetGPUVirtualAddress();
    }

    // Create the pipeline state, which includes loading shaders.
    {
        auto vertexShaderBlob = DX::ReadData(L"VertexShader.cso");
        auto pixelShaderBlob = DX::ReadData(L"PixelShader.cso");

        static const D3D12_INPUT_ELEMENT_DESC s_inputElementDesc[2] =
        {
            { "POSITION",   0,  DXGI_FORMAT_R32G32B32_FLOAT,    0,  0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",      0,  DXGI_FORMAT_R32G32B32_FLOAT,    0,  12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
            device->CreateGraphicsPipelineState(
                &psoDesc,
                IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf())));
    }

    // Create vertex buffer.
    {
        static const Vertex s_vertexData[] =
        {
			{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
            { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
            { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },
            { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) },

            { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
            { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
            { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },
            { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f) },

            { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f) },

            { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) },

            { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
            { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
            { XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },
            { XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f) },

            { XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
            { XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
            { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
            { XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        };

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(s_vertexData));

        DX::ThrowIfFailed(
            device->CreateCommittedResource(&heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_vertexBuffer.ReleaseAndGetAddressOf())));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
        DX::ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, s_vertexData, sizeof(s_vertexData));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = sizeof(s_vertexData);
    }

    // Create index buffer.
    {
        static const uint16_t s_indexData[] =
        {
            3,1,0,
            2,1,3,

            6,4,5,
            7,4,6,

            11,9,8,
            10,9,11,

            14,12,13,
            15,12,14,

            19,17,16,
            18,17,19,

            22,20,21,
            23,20,22
        };

        const D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        const CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(s_indexData));

    	DX::ThrowIfFailed(
            device->CreateCommittedResource(&heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_indexBuffer.ReleaseAndGetAddressOf())));

        // Copy the data to the index buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
        DX::ThrowIfFailed(m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, s_indexData, sizeof(s_indexData));
        m_indexBuffer->Unmap(0, nullptr);

        // Initialize the index buffer view.
        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        m_indexBufferView.SizeInBytes = sizeof(s_indexData);
    }

    // Wait until assets have been uploaded to the GPU.
    m_deviceResources->WaitForGpu();

    // Create a fence for synchronizing between the CPU and the GPU
    DX::ThrowIfFailed(
        device->CreateFence(
            m_deviceResources->GetCurrentFrameIndex(), 
            D3D12_FENCE_FLAG_NONE, 
            IID_PPV_ARGS(m_fence.ReleaseAndGetAddressOf())));

    // Initialize values
    m_colorMultiplier = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

    // Initialize camera values
    m_camUp = DEFAULT_UP_VECTOR;
    m_camForward = DEFAULT_FORWARD_VECTOR;
    m_camRight = DEFAULT_RIGHT_VECTOR;
    m_camYaw = -3.0f;
    m_camPitch = 0.37f;

    // Initialize the world matrix
    m_worldMatrix = XMMatrixIdentity();

    // Initialize the view matrix
    m_camPosition = XMVectorSet(0.0f, 2.0f, 5.0f, 0.0f);
    m_camLookTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    m_viewMatrix = XMMatrixLookAtLH(m_camPosition, m_camLookTarget, DEFAULT_UP_VECTOR);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    // TODO: Initialize windows-size dependent objects here.

    // Initialize the projection matrix
    auto size = m_deviceResources->GetOutputSize();
    m_projectionMatrix = XMMatrixPerspectiveFovLH(
        XM_PIDIV4, float(size.right) / float(size.bottom), 0.01f, 100.0f);

    // The frame index will be reset to zero when the window size changes
    // So we need to tell the GPU to signal our fence starting with zero
    uint64_t currentIdx = m_deviceResources->GetCurrentFrameIndex();
    m_deviceResources->GetCommandQueue()->Signal(m_fence.Get(), currentIdx);
}

void Game::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.
    m_rootSignature.Reset();
    m_pipelineState.Reset();
    m_vertexBuffer.Reset();

    m_cbUploadHeap.Reset();
    m_cbMappedData = nullptr;
    m_cbGpuAddress = 0;

    m_fence.Reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}
#pragma endregion
