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

	void Render(
		IN DirectX::BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices,
		OUT std::vector<uint32_t>& retVec1, OUT std::vector<uint32_t>& retVec2, 
		OUT uint32_t& culledQuadCount) const;

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
	~FaceTree() { delete m_rootNode; }

	QuadNode*								GetRootNode() const { return m_rootNode; }

	void Init(ID3D12Device* device)
	{
		for (int i = 0; i < 2; i++)
		{
			m_staticIndexHalfCount = m_rootNode->GetIndexCount() / 2u;
			m_staticIBHalfSize = sizeof(uint32_t) * m_staticIndexHalfCount;

			CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
			auto desc = CD3DX12_RESOURCE_DESC::Buffer(m_staticIBHalfSize);

			DX::ThrowIfFailed(device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(m_staticIB[i].GetAddressOf())
			));

			// Initialize the index buffer view.
			m_ibv[i].BufferLocation = m_staticIB[i]->GetGPUVirtualAddress();
			m_ibv[i].Format = DXGI_FORMAT_R32_UINT;
			m_ibv[i].SizeInBytes = m_staticIBHalfSize;
		}
	}

	uint32_t UpdateIndexData(IN DirectX::BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices)
	{
		m_renderIndexData[0].clear();
		m_renderIndexData[1].clear();

		uint32_t culledQuadCount = 0;
		m_rootNode->Render(frustum, indices, m_renderIndexData[0], m_renderIndexData[1], culledQuadCount);

		m_renderIndexCount[0] = m_renderIndexData[0].size();
		m_renderIndexCount[1] = m_renderIndexData[1].size();

		m_renderIBSize[0] = sizeof(uint32_t) * m_renderIndexCount[0];
		m_renderIBSize[1] = sizeof(uint32_t) * m_renderIndexCount[1];

		return culledQuadCount;
	}

	void Draw(ID3D12GraphicsCommandList* commandList) const
	{
		commandList->IASetIndexBuffer(&m_ibv[0]);
		commandList->DrawIndexedInstanced(m_renderIndexCount[0], 1, 0, 0, 0);

		commandList->IASetIndexBuffer(&m_ibv[1]);
		commandList->DrawIndexedInstanced(m_renderIndexCount[1], 1, 0, 0, 0);
	}
public:
	QuadNode*								m_rootNode;
	uint32_t								m_faceIndexCount;

	std::vector<uint32_t>					m_renderIndexData[2];

	uint32_t								m_staticIndexHalfCount = 0;
	uint32_t								m_staticIBHalfSize = 0;

	uint32_t								m_renderIndexCount[2];
	uint32_t								m_renderIBSize[2];

	D3D12_INDEX_BUFFER_VIEW					m_ibv[2];
	Microsoft::WRL::ComPtr<ID3D12Resource>  m_staticIB[2];
	DirectX::SharedGraphicsResource         m_renderIB[2];
};