#pragma once

#include <GraphicsMemory.h>

#include "QuadNode.h"
#include "ResourceUploadBatch.h"

class FaceTree
{
public:
	FaceTree(QuadNode* rootNode, UINT32 faceIndexCount);
	~FaceTree();

	QuadNode*								GetRootNode() const { return m_rootNode; }

	void Init(ID3D12Device* device);
	uint32_t UpdateIndexData(IN DirectX::BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices);
	void Upload(DirectX::ResourceUploadBatch& upload, DirectX::GraphicsMemory* graphicsMemory);
	void Draw(ID3D12GraphicsCommandList* commandList) const;

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