#pragma once

#include "FaceTree.h"
#include "ShadowMap.h"
#include "StepTimer.h"

class Apollo
{
public:

    Apollo() noexcept;
    ~Apollo();

    Apollo(Apollo&&) = default;
    Apollo& operator= (Apollo&&) = default;

    Apollo(Apollo const&) = delete;
    Apollo& operator= (Apollo const&) = delete;

    // Initialization
    void InitializeD3DResources(HWND window, int width, int height, UINT subDivideCount, UINT shadowMapSize, BOOL fullScreenMode);

    // Basic game loop
    void Tick();

    // Input handle
    void OnKeyDown(UINT8 key);
    void OnKeyUp(UINT8 key);
    void OnMouseWheel(float delta);
    void OnMouseMove(int x, int y);

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowSizeChanged(int width, int height);

private:

    struct OpaqueCB
    {
        DirectX::XMMATRIX   worldMatrix;
        DirectX::XMMATRIX   viewProjMatrix;
        DirectX::XMFLOAT4   cameraPosition;
        DirectX::XMFLOAT4   lightDirection;
        DirectX::XMFLOAT4   lightColor;
        DirectX::XMMATRIX   shadowTransform;
        DirectX::XMFLOAT4   parameters;
    };

    struct ShadowCB
    {
        DirectX::XMMATRIX   lightWorldMatrix;
        DirectX::XMMATRIX   lightViewProjMatrix;
        DirectX::XMVECTOR   cameraPosition;
        DirectX::XMFLOAT4   parameters;
        uint8_t             padding[88];
    };

    void Update(DX::StepTimer const& timer);
    void Render();

    void CreateDeviceResources();
    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();
    void CreateCommandListDependentResources();

    void WaitForGpu() noexcept;
    void MoveToNextFrame();
    void GetAdapter(IDXGIAdapter1** ppAdapter) const;

    void OnDeviceLost();

    // Helper functions
    void CreateTextureResource(const wchar_t* fileName, ID3D12Resource** texture, ID3D12Resource** uploadHeap, UINT index) const;

    // Constants
    const DirectX::XMVECTORF32                          DEFAULT_UP_VECTOR       = { 0.f, 1.f, 0.f, 0.f };
    const DirectX::XMVECTORF32                          DEFAULT_FORWARD_VECTOR  = { 0.f, 0.f, 1.f, 0.f };
    const DirectX::XMVECTORF32                          DEFAULT_RIGHT_VECTOR    = { 1.f, 0.f, 0.f, 0.f };
    const DirectX::XMFLOAT4X4                           IDENTITY_MATRIX         = { 1.f, 0.f, 0.f, 0.f,
																					0.f, 1.f, 0.f, 0.f,
																					0.f, 0.f, 1.f, 0.f,
																					0.f, 0.f, 0.f, 1.f };

    // Input
    std::unordered_map<UINT8, bool>                     m_keyTracker;
    bool												m_isFlightMode;

	// Application state
    HWND                                                m_window;
    int                                                 m_outputWidth;
    int                                                 m_outputHeight;
    float											    m_aspectRatio;
    bool												m_fullScreenMode;
    static constexpr DXGI_FORMAT                        c_backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    static constexpr DXGI_FORMAT                        c_rtvFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    static constexpr DXGI_FORMAT                        c_depthBufferFormat = DXGI_FORMAT_D32_FLOAT;
    static constexpr UINT                               c_swapBufferCount = 3;

    // Back buffer index
    UINT                                                m_backBufferIndex;

    // Descriptor sizes
    UINT                                                m_rtvDescriptorSize;
    UINT												m_dsvDescriptorSize;
    UINT												m_cbvSrvDescriptorSize;

    // Options
    D3D_FEATURE_LEVEL                                   m_featureLevel;

    // Device resources
	Microsoft::WRL::ComPtr<IDXGIFactory4>               m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device>                m_d3dDevice;

    // Fence objects
    Microsoft::WRL::ComPtr<ID3D12Fence>                 m_fence;
    UINT64                                              m_fenceValues[c_swapBufferCount];
    Microsoft::WRL::Wrappers::Event                     m_fenceEvent;

    // Command objects
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>          m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      m_commandAllocators[c_swapBufferCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>   m_commandList;

    // Descriptor heaps
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_rtvDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_dsvDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_srvDescriptorHeap;

    // Root signature and pipeline state objects
    Microsoft::WRL::ComPtr<ID3D12RootSignature>         m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_opaquePSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_noShadowPSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_wireframePSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>         m_shadowPSO;

    // CB
    Microsoft::WRL::ComPtr<ID3D12Resource>              m_cbOpaqueUploadHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource>              m_cbShadowUploadHeap;
    OpaqueCB*                                           m_cbOpaqueMappedData;
    ShadowCB*                                           m_cbShadowMappedData;
    D3D12_GPU_VIRTUAL_ADDRESS						    m_cbOpaqueGpuAddress;
    D3D12_GPU_VIRTUAL_ADDRESS						    m_cbShadowGpuAddress;

    // Resources
    Microsoft::WRL::ComPtr<IDXGISwapChain3>             m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource>              m_renderTargets[c_swapBufferCount];
    Microsoft::WRL::ComPtr<ID3D12Resource>              m_depthStencil;

    // Viewport and scissor rect
    D3D12_VIEWPORT                                      m_viewport;
    D3D12_RECT                                          m_scissorRect;

    // Textures
    Microsoft::WRL::ComPtr<ID3D12Resource>              m_colorLTexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>              m_colorRTexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>              m_heightLTexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource>              m_heightRTexResource;

    // Static IB Data
    std::vector<uint32_t>							    m_totalIndexData;
    size_t											    m_totalIBSize;
    uint32_t										    m_totalIndexCount;

    // Static VB
    Microsoft::WRL::ComPtr<ID3D12Resource>              m_staticVB;
    D3D12_VERTEX_BUFFER_VIEW                            m_staticVBV;
    size_t											    m_staticVBSize;
    uint32_t										    m_staticVertexCount;

    // QuadBox
    UINT        			                            m_subDivideCount;
    uint32_t										    m_culledQuadCount;

    // QuadTree instances
    std::vector<FaceTree*>                              m_faceTrees;

    // Shadow
    std::unique_ptr<ShadowMap>  			            m_shadowMap;
    UINT												m_shadowMapSize;
    DirectX::BoundingSphere                             m_sceneBounds;

    // Game state
    DX::StepTimer                                       m_timer;

    // Rendering options
    bool												m_renderShadow;
    bool												m_lightRotation;
    bool												m_wireframe;

    // WVP matrices
    DirectX::XMMATRIX                                   m_worldMatrix;
    DirectX::XMMATRIX                                   m_viewMatrix;
    DirectX::XMMATRIX                                   m_projectionMatrix;

    // Camera states
    DirectX::XMVECTOR                                   m_camPosition;
    DirectX::XMVECTOR                                   m_camLookTarget;
    DirectX::XMMATRIX							        m_camRotationMatrix;
    DirectX::XMVECTOR                                   m_camUp;
    DirectX::XMVECTOR                                   m_camRight;
    DirectX::XMVECTOR                                   m_camForward;
    float                                               m_camYaw;
    float										        m_camPitch;
    bool                                                m_orbitMode;
    float                                               m_camMoveSpeed;
    float										        m_camRotateSpeed;

    // Light states
    DirectX::XMVECTOR                                   m_lightDirection;

    // Shadow Map states
    DirectX::XMFLOAT4X4                                 m_shadowTransform;
    float                                               m_lightNearZ;
    float                                               m_lightFarZ;
    DirectX::XMFLOAT3                                   m_lightPosition;
    DirectX::XMFLOAT4X4                                 m_lightView;
    DirectX::XMFLOAT4X4                                 m_lightProj;

    // Tessellation states
    float										        m_quadWidth;
    UINT										        m_unitCount;
    int										            m_tessMin;
    int										            m_tessMax;
};
