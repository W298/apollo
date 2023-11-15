//
// Game.h
//

#pragma once

#include <DirectXCollision.h>

#include "DeviceResources.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "ShadowMap.h"
#include "StepTimer.h"
#include "VertexTypes.h"

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
    struct OpaqueCB
    {
        DirectX::XMMATRIX worldMatrix;
        DirectX::XMMATRIX viewProjMatrix;
        DirectX::XMFLOAT4 cameraPosition;
        DirectX::XMFLOAT4 lightDirection;
        DirectX::XMFLOAT4 lightColor;
        DirectX::XMMATRIX shadowTransform;
        float             shadowBias;
    };

    union PaddedOpaqueCB
    {
        OpaqueCB constants;
        uint8_t bytes[2 * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
    };

    struct ShadowCB
    {
        DirectX::XMMATRIX lightWorldMatrix;
        DirectX::XMMATRIX lightViewProjMatrix;
        DirectX::XMVECTOR cameraPosition;
    };

    union PaddedShadowCB
    {
    	ShadowCB constants;
		uint8_t bytes[2 * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
	};

    // Check the exact size to make sure it will align properly
    static_assert(sizeof(PaddedOpaqueCB) == 
        2 * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, "PaddedOpaqueCB is not aligned properly");
    static_assert(sizeof(PaddedShadowCB) ==
        2 * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, "PaddedShadowCB is not aligned properly");

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Constants
    const DirectX::XMVECTORF32                      DEFAULT_UP_VECTOR = { 0.f, 1.f, 0.f, 0.f };
    const DirectX::XMVECTORF32                      DEFAULT_FORWARD_VECTOR = { 0.f, 0.f, 1.f, 0.f };
    const DirectX::XMVECTORF32                      DEFAULT_RIGHT_VECTOR = { 1.f, 0.f, 0.f, 0.f };
    const DirectX::XMFLOAT4X4                       IDENTITY_MATRIX = { 1.f, 0.f, 0.f, 0.f,
    																	0.f, 1.f, 0.f, 0.f,
    																	0.f, 0.f, 1.f, 0.f,
    																	0.f, 0.f, 0.f, 1.f };

    // Device resources.
    std::unique_ptr<DX::DeviceResources>            m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                                   m_timer;

    // Input devices.
    std::unique_ptr<DirectX::Keyboard>              m_keyboard;
    std::unique_ptr<DirectX::Mouse>                 m_mouse;

    // Number of draw calls
    static const unsigned int                       c_numDrawCalls = 2;

    // Root Signature and PSO
    Microsoft::WRL::ComPtr<ID3D12RootSignature>     m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>     m_pipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>     m_shadowPSO;

    // VB and IB
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW                        m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW                         m_indexBufferView;
    unsigned int                                    m_indexCount;

    // CB
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_cbUploadHeap;
    PaddedOpaqueCB*                                 m_cbMappedData;
    D3D12_GPU_VIRTUAL_ADDRESS                       m_cbGpuAddress;
	Microsoft::WRL::ComPtr<ID3D12Resource>          m_cbUploadHeapShadow;
    PaddedShadowCB*                                 m_cbMappedDataShadow;
    D3D12_GPU_VIRTUAL_ADDRESS                       m_cbGpuAddressShadow;

    // Texture Resources
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_colorLTexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_colorRTexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_heightLTexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>          m_heightRTexResource;

    // SRV Descriptor Heap
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_srvHeap;

    // Descriptor Sizes
    UINT											m_cbvsrvDescSize;
    UINT											m_dsvDescSize;

    // Camera
    float                                           m_camMoveSpeed = 60.0f;

    // Shadow
    std::unique_ptr<ShadowMap>  			        m_shadowMap;
    DirectX::BoundingSphere                         m_sceneBounds;
    float										    m_shadowBias = 0.003f;

    // A synchronization fence and an event. These members will be used
    // to synchronize the CPU with the GPU so that there will be no
    // contention for the constant buffers. 
    Microsoft::WRL::ComPtr<ID3D12Fence>             m_fence;
    Microsoft::WRL::Wrappers::Event                 m_fenceEvent;

    //--------------------------------------------------------------------------------------
	// Constant data
	//--------------------------------------------------------------------------------------
    // WVP matrices
    DirectX::XMMATRIX                               m_worldMatrix;
    DirectX::XMMATRIX                               m_viewMatrix;
    DirectX::XMMATRIX                               m_projectionMatrix;

    // Camera states
    DirectX::XMVECTOR                               m_camPosition;
    DirectX::XMVECTOR                               m_camLookTarget;
    DirectX::XMMATRIX							    m_camRotationMatrix;
    DirectX::XMVECTOR                               m_camUp;
    DirectX::XMVECTOR                               m_camRight;
    DirectX::XMVECTOR                               m_camForward;
    float                                           m_camYaw;
    float										    m_camPitch;
    float										    m_scrollWheelValue;
    bool                                            m_orbitMode = false;

    // Light states
    DirectX::XMVECTOR                               m_lightDirection;

    // Shadow Map states
    DirectX::XMFLOAT4X4                             m_shadowTransform = IDENTITY_MATRIX;
    float                                           m_lightNearZ = 0.0f;
    float                                           m_lightFarZ = 0.0f;
    DirectX::XMFLOAT3                               m_lightPosition;
    DirectX::XMFLOAT4X4                             m_lightView = IDENTITY_MATRIX;
    DirectX::XMFLOAT4X4                             m_lightProj = IDENTITY_MATRIX;
};
