# apollo
![title](https://github.com/W298/apollo/assets/25034289/95b49e3b-2ba5-4942-8750-5a3c66ceeb57)
<br/>

A large-scale lunar terrain renderer in DirectX 12 using terrain information from [NASA's Scientific Visualization Studio.](https://svs.gsfc.nasa.gov/cgi-bin/details.cgi?aid=4720)

## Textures

You have to download the textures from the [Release](https://github.com/W298/apollo/releases) and place Textures directory in your root directory for build properly.

## Techniques

- View frustum culling with QuadTree
  - 1 index buffer per QuadTree, execute 1 Draw Call on each QuadTrees
  - Each frame, Check view frustum contains OBB of QuadNode
  - Each frame, Update index buffer (default heap) with CopyBufferRegion
- Distance based tessellation factor calculation
- Matching QuadNode border tessellation factors
  - By estimating adjacent tessellation factors of QuadNode
  - For preventing crack
- Matching cube border teseellation factors
- Shadow mapping with PCF (Percentage-Closer Filtering)
