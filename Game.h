//
// Game.h
//

#pragma once

#include "DeviceResources.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "StepTimer.h"
#include "VertexTypes.h"

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
    struct ConstantBuffer
    {
        DirectX::XMMATRIX worldMatrix;
        DirectX::XMMATRIX viewMatrix;
        DirectX::XMMATRIX projectionMatrix;
        DirectX::XMFLOAT4 cameraPosition;
    };

    union PaddedConstantBuffer
    {
        ConstantBuffer constants;
        uint8_t bytes[2 * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
    };

    // Check the exact size of the PaddedConstantBuffer to make sure it will align properly
    static_assert(sizeof(PaddedConstantBuffer) == 
        2 * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, "PaddedConstantBuffer is not aligned properly");

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    const DirectX::XMVECTORF32 DEFAULT_UP_VECTOR = { 0.f, 1.f, 0.f, 0.f };
    const DirectX::XMVECTORF32 DEFAULT_FORWARD_VECTOR = { 0.f, 0.f, 1.f, 0.f };
    const DirectX::XMVECTORF32 DEFAULT_RIGHT_VECTOR = { 1.f, 0.f, 0.f, 0.f };

    // Device resources.
    std::unique_ptr<DX::DeviceResources>            m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                                   m_timer;

    // Input devices.
    std::unique_ptr<DirectX::Keyboard>              m_keyboard;
    std::unique_ptr<DirectX::Mouse>                 m_mouse;

    // Direct3D 12 objects
    Microsoft::WRL::ComPtr<ID3D12RootSignature>     m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>     m_pipelineState;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW                        m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW                         m_indexBufferView;
    unsigned int                                    m_indexCount;

    // Constant buffer objects
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cbUploadHeap;
    PaddedConstantBuffer*                           m_cbMappedData;
    D3D12_GPU_VIRTUAL_ADDRESS                       m_cbGpuAddress = 0;

    // Texture objects
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_colorLTexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_colorRTexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_heightLTexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_heightRTexResource;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_srvHeap;

    UINT											m_cbvsrvDescSize = 0;

    // Number of draw calls
    static const unsigned int                       c_numDrawCalls = 1;

    // A synchronization fence and an event. These members will be used
    // to synchronize the CPU with the GPU so that there will be no
    // contention for the constant buffers. 
    Microsoft::WRL::ComPtr<ID3D12Fence>             m_fence;
    Microsoft::WRL::Wrappers::Event                 m_fenceEvent;

    // These computed values will be loaded into a ConstantBuffer
    // during Render
    DirectX::XMMATRIX                               m_worldMatrix;
    DirectX::XMMATRIX                               m_viewMatrix;
    DirectX::XMMATRIX                               m_projectionMatrix;

    // Camera state
    DirectX::XMVECTOR                               m_camPosition;
    DirectX::XMVECTOR                               m_camLookTarget;
    DirectX::XMMATRIX							    m_camRotationMatrix;
    DirectX::XMVECTOR                               m_camUp;
    DirectX::XMVECTOR                               m_camRight;
    DirectX::XMVECTOR                               m_camForward;
    float                                           m_camYaw;
    float										    m_camPitch;

    // If using the DirectX Tool Kit for DX12, uncomment this line:
    // std::unique_ptr<DirectX::GraphicsMemory>     m_graphicsMemory;
};
