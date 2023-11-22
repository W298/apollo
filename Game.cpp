//
// Game.cpp
//

#include "pch.h"
#include "Game.h"

#include <array>

#include "BufferHelpers.h"
#include "DDSTextureLoader.h"
#include "DirectXHelpers.h"
#include "GeometryGenerator.h"
#include "ReadData.h"
#include "ResourceUploadBatch.h"

extern void ExitGame() noexcept;

using namespace DirectX;
using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
{
    // Check Arguments
    {
        LPWSTR* szArglist;
        int nArgs;

        szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
        if (szArglist == nullptr || nArgs < 1)
        {
            OutputDebugStringW(L"CommandLineToArgvW failed\n");
            ExitGame();
        }

        m_subDivideCount = std::max(TESS_GROUP_QUAD_LEVEL, static_cast<UINT>(std::stoi(szArglist[1])));

        LocalFree(szArglist);
    }

    m_deviceResources = std::make_unique<DX::DeviceResources>(
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,    // backBufferFormat (DXGI_FORMAT)
        DXGI_FORMAT_D24_UNORM_S8_UINT,      // depthBufferFormat (DXGI_FORMAT)
        3,                                  // backBufferCount (UINT)
        D3D_FEATURE_LEVEL_11_0,             // minFeatureLevel (D3D_FEATURE_LEVEL)
        0                                   // flags (unsigned int)
    );

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

    const auto elapsedTime = static_cast<float>(timer.GetElapsedSeconds());

    // Handle Input
    {
        const auto keyboard = m_keyboard->GetState();
        if (keyboard.Escape)
        {
            ExitGame();
        }
        const auto mouse = m_mouse->GetState();

        m_orbitMode = keyboard.O ? true : keyboard.F ? false : m_orbitMode;

        if (m_orbitMode)
        {
            // Orbit mode
            if (mouse.leftButton)
            {
                m_camPosition = XMVector3TransformCoord(m_camPosition, XMMatrixRotationY(mouse.x * elapsedTime / 10.0f));
                m_camPosition = XMVector3TransformCoord(m_camPosition, XMMatrixRotationX(-mouse.y * elapsedTime / 10.0f));
            }

            m_camPosition = m_camPosition - XMVector3Normalize(m_camPosition) * (mouse.scrollWheelValue - m_scrollWheelValue) * elapsedTime * 10.0f;
            m_scrollWheelValue = static_cast<float>(mouse.scrollWheelValue);

            m_viewMatrix = XMMatrixLookAtLH(m_camPosition, XMVectorSet(0, 0, 0, 1), DEFAULT_UP_VECTOR);
        }
        else
        {
            // Flight mode
            const float verticalMove = (keyboard.W ? 1.0f : keyboard.S ? -1.0f : 0.0f) * elapsedTime * m_camMoveSpeed;
            const float horizontalMove = (keyboard.A ? -1.0f : keyboard.D ? 1.0f : 0.0f) * elapsedTime * m_camMoveSpeed;

            // Handle mouse input.
            m_camYaw += mouse.x * elapsedTime * m_camRotateSpeed;
            m_camPitch += mouse.y * elapsedTime * m_camRotateSpeed;

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

            // Manipulate shadow bias using scroll value.
            m_shadowBias += (mouse.scrollWheelValue - m_scrollWheelValue) * elapsedTime / 1000.0f;
            m_scrollWheelValue = static_cast<float>(mouse.scrollWheelValue);
        }

        // Do frustum culling
        {
            PIXBeginEvent(PIX_COLOR_DEFAULT, L"Frustum Culling");

            // Update frustum
            BoundingFrustum bf;
            auto det = XMMatrixDeterminant(m_viewMatrix);
            m_boundingFrustum.Transform(bf, XMMatrixInverse(&det, m_viewMatrix));

        	uint32_t totalCulledQuadCount = 0;
            for (int i = 0; i < 6; i++)
            {
                uint32_t culledQuadCount = m_faceTrees[i]->UpdateIndexData(bf, m_staticIndexData);
                totalCulledQuadCount += culledQuadCount;
            }

            // Check how many quads are culled
            /*OutputDebugStringW((
                L"Quad Culled : " + std::to_wstring(totalCulledQuadCount) + 
                L" / " + std::to_wstring(((float)totalCulledQuadCount / (m_staticIndexCount / 4.0f)) * 100) + 
                L" % Removed!  " + std::to_wstring(timer.GetFramesPerSecond()) + L"FPS" + L"\n").c_str()
            );*/

            // Debug cam position
            OutputDebugStringW(
                (L"Cam Position : " + std::to_wstring(m_camPosition.m128_f32[0]) + 
                    L", " + std::to_wstring(m_camPosition.m128_f32[1]) + L", " 
                    + std::to_wstring(m_camPosition.m128_f32[2]) + L"\n").c_str()
            );

            PIXEndEvent();
        }
    }

    // Light rotation update
    m_lightDirection = XMVector3TransformCoord(m_lightDirection, XMMatrixRotationY(elapsedTime / 12.0f));

    // Update Shadow Transform
    {
        XMVECTOR lightDir = m_lightDirection;
        XMVECTOR lightPos = -2.0f * m_sceneBounds.Radius * lightDir;
        XMVECTOR targetPos = XMLoadFloat3(&m_sceneBounds.Center);
        XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

        XMStoreFloat3(&m_lightPosition, lightPos);

        // Transform bounding sphere to light space.
        XMFLOAT3 sphereCenterLS;
        XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

        // Ortho frustum in light space encloses scene.
        float l = sphereCenterLS.x - m_sceneBounds.Radius;
        float b = sphereCenterLS.y - m_sceneBounds.Radius;
        float n = sphereCenterLS.z - m_sceneBounds.Radius;
        float r = sphereCenterLS.x + m_sceneBounds.Radius;
        float t = sphereCenterLS.y + m_sceneBounds.Radius;
        float f = sphereCenterLS.z + m_sceneBounds.Radius;

        m_lightNearZ = n;
        m_lightFarZ = f;

        XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

        // Transform NDC space [-1,+1]^2 to texture space [0,1]^2
        XMMATRIX T(
            0.5f, 0.0f, 0.0f, 0.0f,
            0.0f, -0.5f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.0f, 1.0f);

        XMMATRIX S = lightView * lightProj * T;
        XMStoreFloat4x4(&m_lightView, lightView);
        XMStoreFloat4x4(&m_lightProj, lightProj);
        XMStoreFloat4x4(&m_shadowTransform, S);
    }

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

    const auto commandList = m_deviceResources->GetCommandList();

    // if frame index is reset to zero it may temporarily be smaller than the last GPU signal
    if ((frameIdx > completedValue) && (frameIdx - completedValue > numBackBuffers))
    {
        // GPU not caught up, wait for at least one available frame
        DX::ThrowIfFailed(m_fence->SetEventOnCompletion(frameIdx - numBackBuffers, m_fenceEvent.Get()));
        WaitForSingleObjectEx(m_fenceEvent.Get(), INFINITE, FALSE);
    }

    // Update dynamic index buffer and upload to static index buffer.
    {
        ResourceUploadBatch upload(m_deviceResources->GetD3DDevice());

        upload.Begin();
        for (FaceTree* faceTree : m_faceTrees)
            faceTree->Upload(upload, m_graphicsMemory.get());
        const auto finish = upload.End(m_deviceResources->GetCommandQueue());
        finish.wait();
    }

    // Prepare the command list to render a new frame.
    m_deviceResources->Prepare();

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");

    // Index into the available constant buffers based on the number
    // of draw calls. We've allocated enough for a known number of
    // draw calls per frame times the number of back buffers
    unsigned int cbIndex = c_numDrawCalls * (frameIdx % numBackBuffers);

    // Set the root signature and pipeline state.
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // Set the descriptor heap containing the texture SRV.
    ID3D12DescriptorHeap* descHeaps[] = { m_srvHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(descHeaps), descHeaps);
    commandList->SetGraphicsRootDescriptorTable(0, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

    //--------------------------------------------------------------------------------------
	// PASS 1 - Shadow Map
	//--------------------------------------------------------------------------------------

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"PASS 1 Shadow Map");

    {
        // Update ShadowCB Data
        {
            ShadowCB cbShadow;

            const XMMATRIX lightWorld = XMLoadFloat4x4(&IDENTITY_MATRIX);
            const XMMATRIX lightView = XMLoadFloat4x4(&m_lightView);
            const XMMATRIX lightProj = XMLoadFloat4x4(&m_lightProj);

            cbShadow.lightWorldMatrix = XMMatrixTranspose(lightWorld);
            cbShadow.lightViewProjMatrix = XMMatrixTranspose(lightView * lightProj);
            cbShadow.cameraPosition = m_camPosition;

            cbShadow.quadWidth = m_quadWidth;
            cbShadow.unitCount = m_unitCount;

            memcpy(&m_cbMappedDataShadow[cbIndex].constants, &cbShadow, sizeof(ShadowCB));

            // Bind the constants to the shader.
            const auto baseGpuAddress = m_cbGpuAddressShadow + sizeof(PaddedShadowCB) * cbIndex;
            commandList->SetGraphicsRootConstantBufferView(2, baseGpuAddress);
        }

	    const auto viewport = m_shadowMap->Viewport();
	    const auto scissorRect = m_shadowMap->ScissorRect();
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissorRect);

        // Change to DEPTH_WRITE.
        TransitionResource(commandList, m_shadowMap->Resource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE);

        // ---> DEPTH_WRITE
        {
            // Clear the back buffer and depth buffer.
            commandList->ClearDepthStencilView(
                m_shadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

            // Set null render target because we are only going to draw to
            // depth buffer. Setting a null render target will disable color writes.
            // Note the active PSO also must specify a render target count of 0.
            const auto dsv = m_shadowMap->Dsv();
            commandList->OMSetRenderTargets(0, nullptr, false, &dsv);
            commandList->SetPipelineState(m_shadowPSO.Get());

            // Set necessary state.
            commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
            commandList->IASetVertexBuffers(0, 1, &m_vbv);

            for (const FaceTree* faceTree : m_faceTrees)
            {
                faceTree->Draw(commandList);
            }
        }
        // ---> GENERIC_READ

        // Change to GENERIC_READ, so we can read the texture in a shader.
        TransitionResource(commandList, m_shadowMap->Resource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ);
    }

    PIXEndEvent(commandList);

    //--------------------------------------------------------------------------------------
    // PASS 2 - Opaque
    //--------------------------------------------------------------------------------------

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"PASS 2 Opaque");

    {
        Clear();

        commandList->SetPipelineState(m_pipelineState.Get());

        // Update OpaqueCB Data
        {
            OpaqueCB cbOpaque;

        	cbOpaque.worldMatrix = XMMatrixTranspose(m_worldMatrix);
            cbOpaque.viewProjMatrix = XMMatrixTranspose(m_viewMatrix * m_projectionMatrix);
            XMStoreFloat4(&cbOpaque.cameraPosition, m_camPosition);
            XMStoreFloat4(&cbOpaque.lightDirection, m_lightDirection);
        	cbOpaque.lightColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

            cbOpaque.shadowTransform = XMMatrixTranspose(XMLoadFloat4x4(&m_shadowTransform));
        	cbOpaque.shadowBias = m_shadowBias;

            cbOpaque.quadWidth = m_quadWidth;
            cbOpaque.unitCount = m_unitCount;

            memcpy(&m_cbMappedData[cbIndex].constants, &cbOpaque, sizeof(OpaqueCB));

            // Bind the constants to the shader.
            const auto baseGpuAddress = m_cbGpuAddress + sizeof(PaddedOpaqueCB) * cbIndex;
            commandList->SetGraphicsRootConstantBufferView(1, baseGpuAddress);
        }

        // Set Topology and VB / IB
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
        commandList->IASetVertexBuffers(0, 1, &m_vbv);

        for (const FaceTree* faceTree : m_faceTrees)
        {
            faceTree->Draw(commandList);
        }
    }

    PIXEndEvent(commandList);

    //--------------------------------------------------------------------------------------
    // PASS 3 - Debug
    //--------------------------------------------------------------------------------------

    PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"PASS 3 Debug");

    {
        /*
        commandList->SetPipelineState(m_debugPSO.Get());

        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &m_debugVBV);
        commandList->IASetIndexBuffer(&m_debugIBV);

        commandList->DrawIndexedInstanced(m_debugIndexData.size(), 1, 0, 0, 0);
        */
    }

    PIXEndEvent(commandList);

    PIXEndEvent(commandList);

    // Show the new frame.
    PIXBeginEvent(m_deviceResources->GetCommandQueue(), PIX_COLOR_DEFAULT, L"Present");

	m_deviceResources->Present();
    m_graphicsMemory->Commit(m_deviceResources->GetCommandQueue());

    // GPU will signal an increasing value each frame
    m_deviceResources->GetCommandQueue()->Signal(m_fence.Get(), frameIdx);

    PIXEndEvent(m_deviceResources->GetCommandQueue());
}

// Helper method to clear the back buffers.
void Game::Clear()
{
	const auto commandList = m_deviceResources->GetCommandList();
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
    width = 2560;
    height = 1440;
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

    // Initialize Graphic Memory Instance
    m_graphicsMemory = std::make_unique<GraphicsMemory>(device);

    // Initialize Shadow Map Instance
    m_shadowMap = std::make_unique<ShadowMap>(m_deviceResources->GetD3DDevice(), 8192, 8192);

    // Initialize Bounds for Shadow Map
    m_sceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_sceneBounds.Radius = 160.0f;

    // Initialize Descriptor Sizes
    m_cbvsrvDescSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_dsvDescSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// Create root signature with root CBV, descriptor table (with SRV) and sampler
    {
        CD3DX12_DESCRIPTOR_RANGE srvTable;
        srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);

        CD3DX12_ROOT_PARAMETER rootParameters[3] = {};
        rootParameters[0].InitAsDescriptorTable(1, &srvTable, D3D12_SHADER_VISIBILITY_ALL);     // register (t0)
        rootParameters[1].InitAsConstantBufferView(0);                                          // register (c0)
        rootParameters[2].InitAsConstantBufferView(1);                                          // register (c1)

        const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
			0,                                                  // shaderRegister
			D3D12_FILTER_ANISOTROPIC,                           // filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,                    // addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,                    // addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,                    // addressW
			0.0f,                                               // mipLODBias
			16,                                                 // maxAnisotropy
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
			0.0f,                                               // minLOD
			D3D12_FLOAT32_MAX,                                  // maxLOD
			D3D12_SHADER_VISIBILITY_ALL
		);

        const CD3DX12_STATIC_SAMPLER_DESC shadow(
            1,                                                  // shaderRegister
            D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,   // filter
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,                  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,                  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,                  // addressW
            0.0f,                                               // mipLODBias
            16,                                                 // maxAnisotropy
            D3D12_COMPARISON_FUNC_LESS,
            D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK
        );

        const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
            2,                                                  // shaderRegister
            D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT,          // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,                    // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,                    // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,                    // addressW
            0.0f,                                               // mipLODBias
            16,                                                 // maxAnisotropy
            D3D12_COMPARISON_FUNC_LESS_EQUAL,
            D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
            0.0f,                                               // minLOD
            D3D12_FLOAT32_MAX,                                  // maxLOD
            D3D12_SHADER_VISIBILITY_ALL
        );

        std::array<const CD3DX12_STATIC_SAMPLER_DESC, 3> staticSamplers = { anisotropicWrap, shadow, pointWrap };

        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(
            _countof(rootParameters), rootParameters, 
            (UINT)staticSamplers.size(), staticSamplers.data(), 
            rootSignatureFlags
        );

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

    // Create the SRV Heap.
    {
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = 5;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        DX::ThrowIfFailed(
            device->CreateDescriptorHeap(
                &srvHeapDesc,
                IID_PPV_ARGS(m_srvHeap.ReleaseAndGetAddressOf())));
    }

    // Load color map (L) from file.
    {
        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();
        DX::ThrowIfFailed(
            CreateDDSTextureFromFile(device, resourceUpload, L"colormap_l.dds", m_colorLTexResource.ReleaseAndGetAddressOf()));

        auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
        uploadResourcesFinished.wait();
    }

    // Load color map (R) from file.
    {
        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();
        DX::ThrowIfFailed(
            CreateDDSTextureFromFile(device, resourceUpload, L"colormap_r.dds", m_colorRTexResource.ReleaseAndGetAddressOf()));

        auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
        uploadResourcesFinished.wait();
    }

    // Load displacement map (L) from file.
    {
        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();
        DX::ThrowIfFailed(
            CreateDDSTextureFromFile(device, resourceUpload, L"displacement_l.dds", m_heightLTexResource.ReleaseAndGetAddressOf()));

        auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
        uploadResourcesFinished.wait();
    }

    // Load displacement map (R) from file.
    {
        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();
        DX::ThrowIfFailed(
            CreateDDSTextureFromFile(device, resourceUpload, L"displacement_r.dds", m_heightRTexResource.ReleaseAndGetAddressOf()));

        auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
        uploadResourcesFinished.wait();
    }

    // Create SRVs for the textures.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart());

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        // color map (L)
        srvDesc.Format = m_colorLTexResource->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = m_colorLTexResource->GetDesc().MipLevels;
        device->CreateShaderResourceView(m_colorLTexResource.Get(), &srvDesc, descHandle);

        descHandle.Offset(1, m_cbvsrvDescSize);

        // color map (R)
        srvDesc.Format = m_colorRTexResource->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = m_colorRTexResource->GetDesc().MipLevels;
        device->CreateShaderResourceView(m_colorRTexResource.Get(), &srvDesc, descHandle);

        descHandle.Offset(1, m_cbvsrvDescSize);

        // height map (L)
        srvDesc.Format = m_heightLTexResource->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = m_heightLTexResource->GetDesc().MipLevels;
        device->CreateShaderResourceView(m_heightLTexResource.Get(),&srvDesc, descHandle);

        descHandle.Offset(1, m_cbvsrvDescSize);

        // height map (R)
        srvDesc.Format = m_heightRTexResource->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = m_heightRTexResource->GetDesc().MipLevels;
        device->CreateShaderResourceView(m_heightRTexResource.Get(), &srvDesc, descHandle);

        // shadow map
        m_shadowMap->BuildDescriptors(
            CD3DX12_CPU_DESCRIPTOR_HANDLE(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), 4, m_cbvsrvDescSize),
            CD3DX12_GPU_DESCRIPTOR_HANDLE(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), 4, m_cbvsrvDescSize),
            CD3DX12_CPU_DESCRIPTOR_HANDLE(m_deviceResources->GetDepthStencilView(), 1, m_dsvDescSize));
    }

    // Create the opaque constant buffer memory
    {
        CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        size_t const cbSize = c_numDrawCalls * m_deviceResources->GetBackBufferCount() * sizeof(PaddedOpaqueCB);

        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
        DX::ThrowIfFailed(
            device->CreateCommittedResource(
                &uploadHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_cbUploadHeap.ReleaseAndGetAddressOf())));

        // Map the CPU and GPU addresses.
        DX::ThrowIfFailed(m_cbUploadHeap->Map(0, nullptr, reinterpret_cast<void**>(&m_cbMappedData)));
        m_cbGpuAddress = m_cbUploadHeap->GetGPUVirtualAddress();
    }

    // Create the shadow constant buffer memory
    {
        CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        size_t const cbSize = c_numDrawCalls * m_deviceResources->GetBackBufferCount() * sizeof(PaddedShadowCB);

        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
        DX::ThrowIfFailed(
            device->CreateCommittedResource(
                &uploadHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_cbUploadHeapShadow.ReleaseAndGetAddressOf())));

        // Map the CPU and GPU addresses.
        DX::ThrowIfFailed(m_cbUploadHeapShadow->Map(0, nullptr, reinterpret_cast<void**>(&m_cbMappedDataShadow)));
        m_cbGpuAddressShadow = m_cbUploadHeapShadow->GetGPUVirtualAddress();
    }

    // Create the pipeline state, which includes loading shaders.
    {
        static constexpr D3D12_INPUT_ELEMENT_DESC s_inputElementDesc[] =
        {
            { "POSITION",   0,  DXGI_FORMAT_R32G32B32_FLOAT,    0,  0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "QUAD",       0,  DXGI_FORMAT_R32G32B32_FLOAT,    0,  12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // Load shaders
        auto vertexShaderBlob = DX::ReadData(L"VertexShader.cso");
        auto hullShaderBlob = DX::ReadData(L"HullShader.cso");
        auto domainShaderBlob = DX::ReadData(L"DomainShader.cso");
        auto pixelShaderBlob = DX::ReadData(L"PixelShader.cso");

        // Create Opaque PSO
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { s_inputElementDesc, _countof(s_inputElementDesc) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = { vertexShaderBlob.data(), vertexShaderBlob.size() };
        psoDesc.HS = { hullShaderBlob.data(), hullShaderBlob.size() };
        psoDesc.DS = { domainShaderBlob.data(), domainShaderBlob.size() };
        psoDesc.PS = { pixelShaderBlob.data(), pixelShaderBlob.size() };
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = m_deviceResources->GetBackBufferFormat();
        psoDesc.SampleDesc.Count = 1;
        DX::ThrowIfFailed(
            device->CreateGraphicsPipelineState(
                &psoDesc,
                IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf())));

        // Load shadow shaders
        auto shadowVSBlob = DX::ReadData(L"ShadowVS.cso");
        auto shadowHSBlob = DX::ReadData(L"ShadowHS.cso");
        auto shadowDSBlob = DX::ReadData(L"ShadowDS.cso");
        auto shadowPSBlob = DX::ReadData(L"ShadowPS.cso");

        // Create Shadow Map PSO
        auto shadowPSODesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC(psoDesc);
        shadowPSODesc.VS = { shadowVSBlob.data(), shadowVSBlob.size() };
        shadowPSODesc.HS = { shadowHSBlob.data(), shadowHSBlob.size() };
        shadowPSODesc.DS = { shadowDSBlob.data(), shadowDSBlob.size() };
        shadowPSODesc.PS = { shadowPSBlob.data(), shadowPSBlob.size() };
        shadowPSODesc.RasterizerState.DepthBias = 100000;
        shadowPSODesc.RasterizerState.DepthBiasClamp = 0.0f;
        shadowPSODesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
        shadowPSODesc.pRootSignature = m_rootSignature.Get();
        shadowPSODesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        shadowPSODesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
        shadowPSODesc.NumRenderTargets = 0;
        DX::ThrowIfFailed(
            device->CreateGraphicsPipelineState(
                &shadowPSODesc,
                IID_PPV_ARGS(m_shadowPSO.ReleaseAndGetAddressOf())));

        // Load debug shaders
        auto debugVSBlob = DX::ReadData(L"DebugVS.cso");
        auto debugPSBlob = DX::ReadData(L"DebugPS.cso");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPSODesc = {};
        debugPSODesc.InputLayout = { s_inputElementDesc, _countof(s_inputElementDesc) };
        debugPSODesc.pRootSignature = m_rootSignature.Get();
        debugPSODesc.VS = { debugVSBlob.data(), debugVSBlob.size() };
        debugPSODesc.PS = { debugPSBlob.data(), debugPSBlob.size() };
        debugPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
        debugPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        debugPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        debugPSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        debugPSODesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        debugPSODesc.SampleMask = UINT_MAX;
        debugPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        debugPSODesc.NumRenderTargets = 1;
        debugPSODesc.RTVFormats[0] = m_deviceResources->GetBackBufferFormat();
        debugPSODesc.SampleDesc.Count = 1;
        DX::ThrowIfFailed(
            device->CreateGraphicsPipelineState(
                &debugPSODesc,
                IID_PPV_ARGS(m_debugPSO.ReleaseAndGetAddressOf())));
    }

    // Compute sphere vertices and indices
    auto geoInfo = GeometryGenerator::CreateQuadBox(300.0f, 300.0f, 300.0f, m_subDivideCount, m_debugVertexData, m_debugIndexData);

    m_faceTrees = geoInfo->faceTrees;
    for (FaceTree* faceTree : m_faceTrees)
    {
        faceTree->Init(device);
    }

	const auto maxVertexData = std::vector<VertexTess>(geoInfo->vertices);
    m_staticIndexData = std::vector<uint32_t>(geoInfo->indices);
    m_staticIndexCount = m_staticIndexData.size();

    delete geoInfo;

    m_staticVBSize = sizeof(VertexTess) * maxVertexData.size();
    m_staticIBSize = sizeof(uint32_t) * m_staticIndexCount;

    // Create static vertex buffer and VBV
    {
        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();

        DX::ThrowIfFailed(
            CreateStaticBuffer(device, resourceUpload, maxVertexData, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_staticVB)
        );

        auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
        uploadResourcesFinished.wait();

        // Initialize the vertex buffer view.
        m_vbv.BufferLocation = m_staticVB->GetGPUVirtualAddress();
        m_vbv.StrideInBytes = sizeof(VertexTess);
        m_vbv.SizeInBytes = m_staticVBSize;
    }

    // Create static index buffer and IBV
    {
        CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(m_staticIBSize);

    	DX::ThrowIfFailed(device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(m_staticIB.GetAddressOf())
        ));

        // Initialize the index buffer view.
        m_ibv.BufferLocation = m_staticIB->GetGPUVirtualAddress();
        m_ibv.Format = DXGI_FORMAT_R32_UINT;
        m_ibv.SizeInBytes = m_staticIBSize;
    }

    // Create vertex buffer (debug)
    {
        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();

        DX::ThrowIfFailed(
            CreateStaticBuffer(device, resourceUpload, m_debugVertexData, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_debugVB)
        );

        auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
        uploadResourcesFinished.wait();

        // Initialize the vertex buffer view.
        m_debugVBV.BufferLocation = m_debugVB->GetGPUVirtualAddress();
        m_debugVBV.StrideInBytes = sizeof(VertexPosition);
        m_debugVBV.SizeInBytes = static_cast<UINT>(sizeof(VertexPosition) * m_debugVertexData.size());
    }

    // Create index buffer (debug)
    {
        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();

        DX::ThrowIfFailed(
            CreateStaticBuffer(device, resourceUpload, m_debugIndexData, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_debugIB)
        );

        auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());
        uploadResourcesFinished.wait();

        // Initialize the vertex buffer view.
        m_debugIBV.BufferLocation = m_debugIB->GetGPUVirtualAddress();
        m_debugIBV.Format = DXGI_FORMAT_R32_UINT;
        m_debugIBV.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * m_debugIndexData.size());
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
    m_camUp = DEFAULT_UP_VECTOR;
    m_camForward = DEFAULT_FORWARD_VECTOR;
    m_camRight = DEFAULT_RIGHT_VECTOR;
    m_camYaw = -6.2f;
    m_camPitch = 0.0f;
    m_camPosition = XMVectorSet(0.0f, 0.0f, -200.0f, 0.0f);
    m_camLookTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

    m_worldMatrix = XMMatrixIdentity();
    m_viewMatrix = XMMatrixLookAtLH(m_camPosition, m_camLookTarget, DEFAULT_UP_VECTOR);

	m_scrollWheelValue = 0;
    m_lightDirection = XMVectorSet(-1.0f, 0.0f, 0.0f, 1.0f);

    m_quadWidth = 300.0f / pow(2, TESS_GROUP_QUAD_LEVEL);
    m_unitCount = pow(2, m_subDivideCount - TESS_GROUP_QUAD_LEVEL);
}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    // Initialize the projection matrix
    auto size = m_deviceResources->GetOutputSize();
    m_projectionMatrix = XMMatrixPerspectiveFovLH(
        XM_PIDIV4, float(size.right) / float(size.bottom), 0.01f, 1000.0f);

    // Construct Frustum
    m_boundingFrustum = BoundingFrustum(m_projectionMatrix);

    // The frame index will be reset to zero when the window size changes
    // So we need to tell the GPU to signal our fence starting with zero
    uint64_t currentIdx = m_deviceResources->GetCurrentFrameIndex();
    m_deviceResources->GetCommandQueue()->Signal(m_fence.Get(), currentIdx);
}

void Game::OnDeviceLost()
{
    m_rootSignature.Reset();
    m_pipelineState.Reset();
    m_shadowPSO.Reset();

	m_staticVB.Reset();
    m_staticIB.Reset();

    m_debugVB.Reset();
    m_debugIB.Reset();

    m_cbUploadHeap.Reset();
    m_cbMappedData = nullptr;
    m_cbUploadHeapShadow.Reset();
    m_cbMappedDataShadow = nullptr;

    m_colorLTexResource.Reset();
    m_colorRTexResource.Reset();
    m_heightLTexResource.Reset();
    m_heightRTexResource.Reset();

    m_srvHeap.Reset();

    m_shadowMap.reset();

    for (const FaceTree* faceTree : m_faceTrees)
        delete faceTree;

    m_graphicsMemory.reset();

    m_fence.Reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}
#pragma endregion
