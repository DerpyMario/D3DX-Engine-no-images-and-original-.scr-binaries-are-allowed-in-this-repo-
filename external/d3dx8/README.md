# d3dx8 (clean-room subset)

The Direct3D 8 screen savers of Windows XP / Windows Server 2003 link
Microsoft's static D3DX 8 utility library (`public\sdk\lib\i386\d3dx8.lib`).
The DirectX 8 SDK that shipped it is no longer distributed, and MinGW-w64
has no D3DX 8 either, so this directory provides the part of the D3DX 8
interface that the reconstructed savers call:

| Header | What it provides |
|---|---|
| `d3dx8math.h`, `d3dx8math.inl` | `D3DXVECTOR2/3/4`, `D3DXMATRIX(A16)`, `D3DXQUATERNION`, `D3DXPLANE`, `D3DXCOLOR`; vector and matrix functions; `ID3DXMatrixStack` |
| `d3dx8core.h` | `ID3DXBuffer` |
| `d3dx8mesh.h` | `ID3DXMesh`, `D3DXCreateMeshFVF`, `D3DXCreateText` |
| `d3dx8tex.h` | `D3DXCreateTextureFromFile*`, `...FromResource*`, `...FromFileInMemory*` |

It is **not** Microsoft code.  The type layouts, function names and
signatures follow the published D3DX 8 interface (they have to, for the
savers to compile unchanged); the implementations are written from the
documented behaviour (for example the matrix formulas of the DirectX 8
documentation).  Results can differ from Microsoft's library in the last
bits of floating-point values, and only the features the savers use are
implemented.

This directory is not part of the original source tree.
