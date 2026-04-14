#include "DX11Demo/Renderer.h"
#include "DX11Demo/Camera.h"
#include "DX11Demo/CubeManager.h"
#include <string>

// グローバルレンダラーインスタンスの定義
Renderer g_renderer;

// ============================================================
// Init — D3D11の初期化を一括で行う
// ============================================================
bool Renderer::Init(HWND hwnd)
{
    if (!CreateDeviceAndSwapChain(hwnd))    return false;
    if (!CreateRenderTargetAndDepthBuffer()) return false;

    // ビューポートの設定
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(WINDOW_WIDTH);
    viewport.Height = static_cast<float>(WINDOW_HEIGHT);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_deviceContext->RSSetViewports(1, &viewport);

    if (!CompileAndCreateShaders())  return false;
    if (!CreateConstantBuffers())    return false;
    if (!CreateMeshBuffers())        return false;
    if (!LoadTextures())             return false;
    if (!CreateSamplerState())       return false;

    return true;
}

// ============================================================
// デバイスとスワップチェインの作成
// ============================================================
bool Renderer::CreateDeviceAndSwapChain(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferDesc.Width = WINDOW_WIDTH;
    scd.BufferDesc.Height = WINDOW_HEIGHT;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 1;
    scd.OutputWindow = hwnd;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL featureLevelOut;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        createDeviceFlags, featureLevels, _countof(featureLevels),
        D3D11_SDK_VERSION, &scd,
        m_swapChain.GetAddressOf(),
        m_device.GetAddressOf(),
        &featureLevelOut,
        m_deviceContext.GetAddressOf());

    if (FAILED(hr))
    {
        MessageBox(nullptr, L"D3D11CreateDeviceAndSwapChain に失敗", L"エラー", MB_OK);
        return false;
    }
    return true;
}

// ============================================================
// レンダーターゲットと深度バッファの作成
// ============================================================
bool Renderer::CreateRenderTargetAndDepthBuffer()
{
    // レンダーターゲットビュー
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (FAILED(hr)) return false;

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr,
        m_renderTargetView.GetAddressOf());
    if (FAILED(hr)) return false;

    // 深度バッファ
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = WINDOW_WIDTH;
    depthDesc.Height = WINDOW_HEIGHT;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ComPtr<ID3D11Texture2D> depthTexture;
    hr = m_device->CreateTexture2D(&depthDesc, nullptr, depthTexture.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = m_device->CreateDepthStencilView(depthTexture.Get(), nullptr,
        m_depthStencilView.GetAddressOf());
    if (FAILED(hr)) return false;

    return true;
}

// ============================================================
// シェーダーコンパイルユーティリティ
// ============================================================
bool Renderer::CompileShader(
    const wchar_t* filePath, const char* entryPoint,
    const char* profile, ComPtr<ID3DBlob>& blob)
{
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompileFromFile(
        filePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint, profile, compileFlags, 0,
        blob.GetAddressOf(), errorBlob.GetAddressOf());

    if (FAILED(hr))
    {
        if (errorBlob)
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }
    return true;
}

// ============================================================
// シェーダーのコンパイルと生成
// ============================================================
bool Renderer::CompileAndCreateShaders()
{
    // 頂点シェーダー
    ComPtr<ID3DBlob> vsBlob;
    if (!CompileShader(L"shaders/shaders.hlsl", "vs_main", "vs_5_0", vsBlob))
    {
        MessageBox(nullptr, L"頂点シェーダーのコンパイルに失敗", L"エラー", MB_OK);
        return false;
    }
    HRESULT hr = m_device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, m_vertexShader.GetAddressOf());
    if (FAILED(hr)) return false;

    // ピクセルシェーダー
    ComPtr<ID3DBlob> psBlob;
    if (!CompileShader(L"shaders/shaders.hlsl", "ps_main", "ps_5_0", psBlob))
    {
        MessageBox(nullptr, L"ピクセルシェーダーのコンパイルに失敗", L"エラー", MB_OK);
        return false;
    }
    hr = m_device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, m_pixelShader.GetAddressOf());
    if (FAILED(hr)) return false;

    // 入力レイアウト
    if (!CreateInputLayout(vsBlob)) return false;

    return true;
}

// ============================================================
// 入力レイアウトの作成
// ============================================================
bool Renderer::CreateInputLayout(ComPtr<ID3DBlob>& vsBlob)
{
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    HRESULT hr = m_device->CreateInputLayout(
        layoutDesc, _countof(layoutDesc),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        m_inputLayout.GetAddressOf());

    if (FAILED(hr))
    {
        MessageBox(nullptr, L"入力レイアウトの作成に失敗", L"エラー", MB_OK);
        return false;
    }
    return true;
}

// ============================================================
// 定数バッファの作成
// ============================================================
bool Renderer::CreateConstantBuffers()
{
    // WVP用
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.ByteWidth = sizeof(ConstantBuffer);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    HRESULT hr = m_device->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.GetAddressOf());
    if (FAILED(hr)) return false;

    // ライト用
    D3D11_BUFFER_DESC lbDesc = {};
    lbDesc.Usage = D3D11_USAGE_DEFAULT;
    lbDesc.ByteWidth = sizeof(LightBuffer);
    lbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = m_device->CreateBuffer(&lbDesc, nullptr, m_lightBuffer.GetAddressOf());
    if (FAILED(hr)) return false;

    return true;
}

// ============================================================
// メッシュ（頂点・インデックス）バッファの作成
// ============================================================
bool Renderer::CreateMeshBuffers()
{
    // ---- キューブ頂点データ（24頂点 = 6面 × 4頂点）----
    Vertex cubeVertices[] =
    {
        // 前面 (0,0,-1)
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(0,0,-1), XMFLOAT2(0, 0) },
        { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT3(0,0,-1), XMFLOAT2(1, 0) },
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(0,0,-1), XMFLOAT2(1, 1) },
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0,0,-1), XMFLOAT2(0, 1) },
        // 背面 (0,0,1)
        { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT3(0,0,1), XMFLOAT2(0, 0) },
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(0,0,1), XMFLOAT2(1, 0) },
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(0,0,1), XMFLOAT2(1, 1) },
        { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT3(0,0,1), XMFLOAT2(0, 1) },
        // 上面 (0,1,0)
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(0,1,0), XMFLOAT2(0, 0) },
        { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT3(0,1,0), XMFLOAT2(1, 0) },
        { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT3(0,1,0), XMFLOAT2(1, 1) },
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(0,1,0), XMFLOAT2(0, 1) },
        // 底面 (0,-1,0)
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0,-1,0), XMFLOAT2(0, 0) },
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(0,-1,0), XMFLOAT2(1, 0) },
        { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT3(0,-1,0), XMFLOAT2(1, 1) },
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(0,-1,0), XMFLOAT2(0, 1) },
        // 左面 (-1,0,0)
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(-1,0,0), XMFLOAT2(0, 0) },
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(-1,0,0), XMFLOAT2(1, 0) },
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(-1,0,0), XMFLOAT2(1, 1) },
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(-1,0,0), XMFLOAT2(0, 1) },
        // 右面 (1,0,0)
        { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT3(1,0,0), XMFLOAT2(0, 0) },
        { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT3(1,0,0), XMFLOAT2(1, 0) },
        { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT3(1,0,0), XMFLOAT2(1, 1) },
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(1,0,0), XMFLOAT2(0, 1) },
    };

    UINT cubeIndices[] =
    {
         0,  1,  2,   0,  2,  3,
         4,  5,  6,   4,  6,  7,
         8,  9, 10,   8, 10, 11,
        12, 13, 14,  12, 14, 15,
        16, 17, 18,  16, 18, 19,
        20, 21, 22,  20, 22, 23,
    };

    // ---- 床頂点データ ----
    Vertex floorVertices[] =
    {
        { XMFLOAT3(-10, -0.5f, -10), XMFLOAT3(0,1,0), XMFLOAT2(0,  0) },
        { XMFLOAT3(10, -0.5f, -10), XMFLOAT3(0,1,0), XMFLOAT2(10,  0) },
        { XMFLOAT3(10, -0.5f,  10), XMFLOAT3(0,1,0), XMFLOAT2(10, 10) },
        { XMFLOAT3(-10, -0.5f,  10), XMFLOAT3(0,1,0), XMFLOAT2(0, 10) },
    };

    UINT floorIndices[] = { 0, 2, 1, 0, 3, 2 };

    // ---- バッファ作成ヘルパー ----
    auto createBuffer = [&](auto* data, UINT byteWidth, UINT bindFlags, ComPtr<ID3D11Buffer>& buffer)
        {
            D3D11_BUFFER_DESC desc = {};
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.ByteWidth = byteWidth;
            desc.BindFlags = bindFlags;

            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = data;

            return m_device->CreateBuffer(&desc, &initData, buffer.GetAddressOf());
        };

    HRESULT hr;
    hr = createBuffer(cubeVertices, sizeof(cubeVertices), D3D11_BIND_VERTEX_BUFFER, m_vertexBuffer);
    if (FAILED(hr)) return false;
    hr = createBuffer(cubeIndices, sizeof(cubeIndices), D3D11_BIND_INDEX_BUFFER, m_indexBuffer);
    if (FAILED(hr)) return false;
    hr = createBuffer(floorVertices, sizeof(floorVertices), D3D11_BIND_VERTEX_BUFFER, m_floorVertexBuffer);
    if (FAILED(hr)) return false;
    hr = createBuffer(floorIndices, sizeof(floorIndices), D3D11_BIND_INDEX_BUFFER, m_floorIndexBuffer);
    if (FAILED(hr)) return false;

    return true;
}

// ============================================================
// テクスチャの読み込み
// ============================================================
bool Renderer::LoadTextures()
{
    HRESULT hr;

    hr = DirectX::CreateWICTextureFromFile(
        m_device.Get(), m_deviceContext.Get(),
        L"textures/crate.png", nullptr, m_textureView.GetAddressOf());
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"テクスチャ(crate)の読み込みに失敗", L"エラー", MB_OK);
        return false;
    }

    hr = DirectX::CreateWICTextureFromFile(
        m_device.Get(), m_deviceContext.Get(),
        L"textures/floor.png", nullptr, m_floorTextureView.GetAddressOf());
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"テクスチャ(floor)の読み込みに失敗", L"エラー", MB_OK);
        return false;
    }

    return true;
}

// ============================================================
// サンプラーステートの作成
// ============================================================
bool Renderer::CreateSamplerState()
{
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = m_device->CreateSamplerState(&sampDesc, m_sampler.GetAddressOf());
    return SUCCEEDED(hr);
}

// ============================================================
// Render — 毎フレームの描画処理
// ============================================================
void Renderer::Render(float deltaTime)
{
    // ---- 画面クリア ----
    m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

    float clearColor[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
    m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
    m_deviceContext->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    // ---- ビュー・プロジェクション行列の取得 ----
    XMMATRIX view = g_camera.GetViewMatrix();
    XMMATRIX projection = g_camera.GetProjectionMatrix();

    // ---- パイプライン共通設定 ----
    m_deviceContext->IASetInputLayout(m_inputLayout.Get());
    m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_deviceContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_deviceContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_deviceContext->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    m_deviceContext->PSSetSamplers(0, 1, m_sampler.GetAddressOf());

    // ---- ライトバッファの更新 ----
    LightBuffer lb;
    lb.lightDirection = XMFLOAT3(0.5f, -1.0f, 0.3f);
    lb.padding1 = 0;
    lb.lightColor = XMFLOAT3(1.0f, 1.0f, 0.95f);
    lb.padding2 = 0;
    lb.cameraPosition = g_camera.GetPosition();
    lb.shininess = 32.0f;
    m_deviceContext->UpdateSubresource(m_lightBuffer.Get(), 0, nullptr, &lb, 0, 0);
    m_deviceContext->PSSetConstantBuffers(1, 1, m_lightBuffer.GetAddressOf());

    // ---- キューブの描画 ----
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ConstantBuffer cb;

    for (int i = 0; i < g_cubeManager.size(); i++)
    {
        Cube cube = g_cubeManager.getCube(i);
        XMMATRIX cubeWorld = XMMatrixScaling(cube.scale.x, cube.scale.y, cube.scale.z)
            * XMMatrixRotationY(cube.angle)
            * XMMatrixTranslation(
                cube.position.x,
                cube.position.y,
                cube.position.z 
            );
        cb.wvp = XMMatrixTranspose(cubeWorld * view * projection);
        cb.world = XMMatrixTranspose(cubeWorld);
        m_deviceContext->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &cb, 0, 0);
        m_deviceContext->PSSetShaderResources(0, 1, m_textureView.GetAddressOf());
        m_deviceContext->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
        m_deviceContext->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        m_deviceContext->DrawIndexed(36, 0, 0);
    }

    // ---- 床の描画 ----
    XMMATRIX floorWorld = XMMatrixIdentity();
    cb.wvp = XMMatrixTranspose(floorWorld * view * projection);
    cb.world = XMMatrixTranspose(floorWorld);
    m_deviceContext->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &cb, 0, 0);
    m_deviceContext->PSSetShaderResources(0, 1, m_floorTextureView.GetAddressOf());
    m_deviceContext->IASetVertexBuffers(0, 1, m_floorVertexBuffer.GetAddressOf(), &stride, &offset);
    m_deviceContext->IASetIndexBuffer(m_floorIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_deviceContext->DrawIndexed(6, 0, 0);

    // ---- Present ----
    m_swapChain->Present(1, 0);
}