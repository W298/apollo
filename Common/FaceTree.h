#pragma once

#include <SimpleMath.h>
#include <GraphicsMemory.h>

#include "ResourceUploadBatch.h"
#include "VertexTypes.h"

class QuadNode
{
public:
	QuadNode(char level, uint32_t indexCount, uint32_t index[4], uint32_t baseAddress, float width);
	~QuadNode();

	void CreateChildren(
		const char limit,
		const std::vector<DirectX::VertexPosition>& vertices,
		const std::vector<uint32_t>& indices, 
		std::vector<DirectX::VertexPosition>& debugVertexData, 
		std::vector<uint32_t>& debugIndexData);

	void CalcCenter(
		const std::vector<DirectX::VertexPosition>& vertices, 
		std::vector<DirectX::VertexPosition>& debugVertexData, 
		std::vector<uint32_t>& debugIndexData);

	void Render(
		IN DirectX::BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices, 
		OUT std::vector<uint32_t>& retVec, OUT uint32_t& culledQuadCount) const;

	uint32_t GetIndexCount() const { return m_indexCount; }

private:
	char									m_level;
	uint32_t								m_indexCount;
	uint32_t								m_cornerIndex[4];
	uint32_t								m_baseAddress;
	DirectX::XMFLOAT3						m_centerPosition;
	DirectX::BoundingOrientedBox			m_obb;
	float									m_width;
	QuadNode*								m_children[4] = { nullptr, nullptr, nullptr, nullptr };
};

class FaceTree
{
public:
	FaceTree(QuadNode* rootNode, UINT32 faceIndexCount) : m_rootNode(rootNode), m_faceIndexCount(faceIndexCount) {}
	~FaceTree()
	{
		delete m_rootNode;
		m_staticIB.Reset();
		m_renderIB.Reset();
	}

	QuadNode*								GetRootNode() const { return m_rootNode; }

	void Init(ID3D12Device* device)
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

	uint32_t UpdateIndexData(IN DirectX::BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices)
	{
		m_renderIndexData.clear();

		uint32_t culledQuadCount = 0;
		m_rootNode->Render(frustum, indices, m_renderIndexData, culledQuadCount);

		m_renderIndexCount = m_renderIndexData.size();
		m_renderIBSize = sizeof(uint32_t) * m_renderIndexCount;

		return culledQuadCount;
	}

	void Upload(DirectX::ResourceUploadBatch& upload, DirectX::GraphicsMemory* m_graphicsMemory)
	{
		m_renderIB = m_graphicsMemory->Allocate(std::max(1u, m_renderIBSize));
		memcpy(m_renderIB.Memory(), m_renderIndexData.data(), m_renderIBSize);
		upload.Upload(m_staticIB.Get(), m_renderIB);
		upload.Transition(m_staticIB.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	}

	void Draw(ID3D12GraphicsCommandList* commandList) const
	{
		commandList->IASetIndexBuffer(&m_ibv);
		commandList->DrawIndexedInstanced(m_renderIndexCount, 1, 0, 0, 0);
	}

private:
	QuadNode*								m_rootNode;
	uint32_t								m_faceIndexCount;

	std::vector<uint32_t>					m_renderIndexData;

	uint32_t								m_staticIndexCount = 0;
	uint32_t								m_staticIBSize = 0;

	uint32_t								m_renderIndexCount;
	uint32_t								m_renderIBSize;

	D3D12_INDEX_BUFFER_VIEW					m_ibv;
	Microsoft::WRL::ComPtr<ID3D12Resource>  m_staticIB;
	DirectX::SharedGraphicsResource         m_renderIB;
};