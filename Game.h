//
// Game.h
//

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"

struct Vertex
{
    DirectX::XMFLOAT4 position;
    DirectX::XMFLOAT4 color;
};

struct ConstantBuffer
{
    DirectX::XMFLOAT4 colorMultiplier;
};

// A basic game implementation that creates a D3D12 device and
// provides a game loop.
class Game final : public DX::IDeviceNotify
{
public:

    Game() noexcept(false);
    ~Game();

    Game(Game&&) = default;
    Game& operator= (Game&&) = default;

    Game(Game const&) = delete;
    Game& operator= (Game const&) = delete;

    // Initialization and management
    void Initialize(HWND window, int width, int height);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
    void OnDisplayChange();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize(int& width, int& height) const noexcept;

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>            m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                                   m_timer;

    // If using the DirectX Tool Kit for DX12, uncomment this line:
    std::unique_ptr<DirectX::GraphicsMemory>        m_graphicsMemory;

    // Direct3D 12 objects
    Microsoft::WRL::ComPtr<ID3D12RootSignature>     m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>     m_pipelineState;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW                        m_vertexBufferView;

    static const UINT                               c_rootParameterCB = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cbUploadHeap;
    ConstantBuffer*                                 m_cbMappedData;
    D3D12_GPU_VIRTUAL_ADDRESS                       m_cbGpuAddress;

    DirectX::XMFLOAT4                               m_colorMultiplier;

    DirectX::GraphicsResource                       m_cbResource;
};
