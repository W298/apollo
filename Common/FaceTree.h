#pragma once

#include "QuadNode.h"

class FaceTree
{
public:
	FaceTree(QuadNode* rootNode, UINT32 faceIndexCount);
	~FaceTree();

	QuadNode*								GetRootNode() const { return m_rootNode; }

	void Init(ID3D12Device* device);
	uint32_t UpdateIndexData(IN DirectX::BoundingFrustum& frustum, IN const std::vector<uint32_t>& indices);
	void Upload(ID3D12GraphicsCommandList* commandList);
	void Draw(ID3D12GraphicsCommandList* commandList) const;

private:
	QuadNode*								m_rootNode;
	uint32_t								m_faceIndexCount;

	std::vector<uint32_t>					m_renderIndexData;

	uint32_t								m_staticIndexCount = 0;
	uint32_t								m_staticIBSize = 0;

	uint32_t								m_renderIndexCount = 0;
	uint32_t								m_renderIBSize = 0;

	D3D12_INDEX_BUFFER_VIEW					m_ibv;
	Microsoft::WRL::ComPtr<ID3D12Resource>  m_staticIB;
	Microsoft::WRL::ComPtr<ID3D12Resource>  m_uploadIB;
};