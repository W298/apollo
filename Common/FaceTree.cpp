#include "pch.h"
#include "FaceTree.h"

FaceTree::FaceTree(QuadNode* rootNode, UINT32 faceIndexCount) :
	m_rootNode(rootNode),
	m_faceIndexCount(faceIndexCount) {}

FaceTree::~FaceTree()
{
	delete m_rootNode;
	m_staticIB.Reset();
	m_renderIB.Reset();
}

void FaceTree::Init(ID3D12Device* device)
{
	m_staticIndexCount = m_rootNode->GetIndexCount();
	m_staticIBSize = sizeof(uint32_t) * m_staticIndexCount;

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

uint32_t FaceTree::UpdateIndexData(IN DirectX::BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices)
{
	m_renderIndexData.clear();

	uint32_t culledQuadCount = 0;
	m_rootNode->Render(frustum, indices, m_renderIndexData, culledQuadCount);

	m_renderIndexCount = m_renderIndexData.size();
	m_renderIBSize = sizeof(uint32_t) * m_renderIndexCount;

	return culledQuadCount;
}

void FaceTree::Upload(DirectX::ResourceUploadBatch& upload, DirectX::GraphicsMemory* graphicsMemory)
{
	m_renderIB = graphicsMemory->Allocate(std::max(1u, m_renderIBSize));
	memcpy(m_renderIB.Memory(), m_renderIndexData.data(), m_renderIBSize);
	upload.Upload(m_staticIB.Get(), m_renderIB);
	upload.Transition(m_staticIB.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
}

void FaceTree::Draw(ID3D12GraphicsCommandList* commandList) const
{
	commandList->IASetIndexBuffer(&m_ibv);
	commandList->DrawIndexedInstanced(m_renderIndexCount, 1, 0, 0, 0);
}