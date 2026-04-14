#pragma once
#include "DX11Demo/Common.h"
#include "DX11Demo/Types.h"
#include "DX11Demo/Cube.h"

// ============================================================
// Renderer — Direct3D 11 の初期化・リソース管理・描画
// ============================================================
// D3D11デバイス、スワップチェイン、シェーダー、バッファ、テクスチャなど
// 描画に必要なすべてのGPUリソースを管理する。
class Renderer
{
public:
    // D3D11の初期化（デバイス、スワップチェイン、シェーダー、バッファ等を一括作成）
    bool Init(HWND hwnd);

    // 毎フレームの描画処理
    void Render(float deltaTime);

private:
    // ---- 初期化のサブ関数 ----
    bool CreateDeviceAndSwapChain(HWND hwnd);
    bool CreateRenderTargetAndDepthBuffer();
    bool CompileAndCreateShaders();
    bool CreateInputLayout(ComPtr<ID3DBlob>& vsBlob);
    bool CreateConstantBuffers();
    bool CreateMeshBuffers();
    bool LoadTextures();
    bool CreateSamplerState();

    // ---- シェーダーコンパイルユーティリティ ----
    static bool CompileShader(const wchar_t* filePath, const char* entryPoint,
        const char* profile, ComPtr<ID3DBlob>& blob);


    // ---- D3D11 コアオブジェクト ----
    ComPtr<ID3D11Device>           m_device;
    ComPtr<ID3D11DeviceContext>    m_deviceContext;
    ComPtr<IDXGISwapChain>         m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;

    // ---- シェーダー ----
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader>  m_pixelShader;
    ComPtr<ID3D11InputLayout>  m_inputLayout;

    // ---- バッファ ----
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    ComPtr<ID3D11Buffer> m_floorVertexBuffer;
    ComPtr<ID3D11Buffer> m_floorIndexBuffer;
    ComPtr<ID3D11Buffer> m_constantBuffer;
    ComPtr<ID3D11Buffer> m_lightBuffer;

    // ---- テクスチャ ----
    ComPtr<ID3D11ShaderResourceView> m_textureView;
    ComPtr<ID3D11ShaderResourceView> m_floorTextureView;
    ComPtr<ID3D11SamplerState>       m_sampler;
};

// グローバルレンダラーインスタンス
extern Renderer g_renderer;