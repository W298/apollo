#include "pch.h"
#include "FaceTree.h"

FaceTree::FaceTree(QuadNode* rootNode, UINT32 faceIndexCount)
{
	m_rootNode = rootNode;
	m_faceIndexCount = faceIndexCount;
	m_renderIndexData = std::vector<uint32_t>(m_faceIndexCount);
}

FaceTree::~FaceTree()
{
	m_staticIB.Reset();
	m_uploadIB.Reset();
	delete m_rootNode;
}

void FaceTree::Init(ID3D12Device* device)
{
	m_staticIndexCount = m_rootNode->GetIndexCount();
	m_staticIBSize = sizeof(uint32_t) * m_staticIndexCount;

    // Create default heap.
    CD3DX12_HEAP_PROPERTIES defaultHeapProp(D3D12_HEAP_TYPE_DEFAULT);
    auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(m_staticIBSize);
    DX::ThrowIfFailed(
        device->CreateCommittedResource(
            &defaultHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(m_staticIB.ReleaseAndGetAddressOf())));

    // Initialize the index buffer view.
    m_ibv.BufferLocation = m_staticIB->GetGPUVirtualAddress();
    m_ibv.Format = DXGI_FORMAT_R32_UINT;
    m_ibv.SizeInBytes = m_staticIBSize;

    // Create upload heap.
    CD3DX12_HEAP_PROPERTIES uploadHeapProp(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(m_staticIBSize);
    DX::ThrowIfFailed(
        device->CreateCommittedResource(
            &uploadHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &uploadHeapDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_uploadIB.ReleaseAndGetAddressOf())));
}

uint32_t FaceTree::UpdateIndexData(IN DirectX::BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices)
{
	m_renderIndexData.clear();

	uint32_t culledQuadCount = 0;
	m_rootNode->Render(frustum, indices, m_renderIndexData, culledQuadCount);

	m_renderIndexCount = m_renderIndexData.size();
	m_renderIBSize = sizeof(uint32_t) * m_renderIndexCount;

	return culledQuadCount;
}

void FaceTree::Upload(ID3D12GraphicsCommandList* commandList)
{
    // Define sub-resource data.
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = m_renderIndexData.data();
    subResourceData.RowPitch = m_renderIBSize;
    subResourceData.SlicePitch = m_renderIBSize;

    // Copy the index data to the default heap.
    UpdateSubresources(commandList, m_staticIB.Get(), m_uploadIB.Get(), 0, 0, 1, &subResourceData);

    // Translate vertex buffer state.
    const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_staticIB.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	commandList->ResourceBarrier(1, &barrier);
}

void FaceTree::Draw(ID3D12GraphicsCommandList* commandList) const
{
	commandList->IASetIndexBuffer(&m_ibv);
	commandList->DrawIndexedInstanced(m_renderIndexCount, 1, 0, 0, 0);
}