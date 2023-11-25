#include "pch.h"
#include "FaceTree.h"

#include "BufferHelpers.h"

FaceTree::FaceTree(QuadNode* rootNode, UINT32 faceIndexCount)
{
	m_rootNode = rootNode;
	m_faceIndexCount = faceIndexCount;
	m_renderIndexData = std::vector<uint32_t>(m_faceIndexCount);
}

FaceTree::~FaceTree()
{
	m_staticIB.Reset();
	m_renderIB.Reset();
	delete m_rootNode;
}

void FaceTree::Init(ID3D12Device* device, ID3D12CommandQueue* commandQueue)
{
	m_staticIndexCount = m_rootNode->GetIndexCount();
	m_staticIBSize = sizeof(uint32_t) * m_staticIndexCount;

	DirectX::ResourceUploadBatch upload(device);
	upload.Begin();

	DX::ThrowIfFailed(
		CreateStaticBuffer(device, upload, m_renderIndexData, D3D12_RESOURCE_STATE_INDEX_BUFFER, m_staticIB.ReleaseAndGetAddressOf())
	);

	auto finish = upload.End(commandQueue);
	finish.wait();

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