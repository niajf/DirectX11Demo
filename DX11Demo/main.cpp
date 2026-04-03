// ============================================================
// main.cpp — Win32ウィンドウの作成
// ============================================================

// Windows APIのヘッダー
// WIN32_LEAN_AND_MEANを定義すると、使用頻度の低いAPIが除外されビルドが速くなる
#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

// ----- DirectX11 -----
#include <d3d11.h>          // D3D11のコアAPI
#include <dxgi.h >          // DXGI(ディスプレイ関連)
#include <d3dcompiler.h>   // シェーダーコンパイラ
#include <wrl/client.h>     // ComPtr
#include <DirectXMath.h> 

// ライブラリのリンク
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// ウィンドウのサイズ（後でDirectXのバックバッファサイズと合わせる）
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

// ---- DirectX11 オブジェクト-----
ComPtr<ID3D11Device> g_device;                      // リソース生成担当
ComPtr<ID3D11DeviceContext> g_deviceContext;        // 描画コマンド担当
ComPtr<IDXGISwapChain> g_swapChain;                 // ダブルバッファリング
ComPtr<ID3D11RenderTargetView> g_renderTargetView;  // 描画先の指定

// ---- シェーダー ----
ComPtr<ID3D11VertexShader> g_vertexShader;
ComPtr<ID3D11PixelShader> g_pixelShader;
ComPtr<ID3D11InputLayout> g_inputLayout;

// ---- 頂点バッファ ----
ComPtr<ID3D11Buffer> g_vertexBuffer;
ComPtr<ID3D11Buffer> g_floorVertexBuffer;

// ---- 定数バッファ ----
ComPtr<ID3D11Buffer> g_constantBuffer;

// ----　インデックスバッファ ----
ComPtr<ID3D11Buffer> g_indexBuffer;
ComPtr<ID3D11Buffer> g_floorIndexBuffer;

// ---- 深度バッファ ----
ComPtr<ID3D11DepthStencilView> g_depthStencilView;

// ============================================================
// カメラの状態
// ============================================================
XMFLOAT3 g_camPosition = { 0.0f, 1.5f, -3.0f }; // カメラの位置
float g_camYaw = 0.0f;                          // 左右の回転（ラジアン）
float g_camPitch = 0.0f;                        // 上下の回転（ラジアン）
float g_camSpeed = 3.0f;                        // 移動速度（単位/秒）
float g_mouseSensitivity = 0.002f;              // マウス感度

// ---- キー入力 ----
bool g_keyW = false;
bool g_keyA = false;
bool g_keyS = false;
bool g_keyD = false;

// ---- マウス入力 ----
bool g_mouseCaptured = false;   // マウスがキャプチャされているか
POINT g_lastMousePos = {};      // 前フレームのマウス位置

// ウィンドウプロシージャの前方宣言
// OSからのメッセージ（キー入力、マウス、閉じるボタン等）を処理するコールバック関数
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================
// 頂点構造体
// ============================================================
// GPU側のHLSL構造体（VSInput）と完全に対応する。
// メモリ上のレイアウトが一致していなければならない。
//
// XMFLOAT3 = float x 3 (12バイト) — HLSLの float3 に対応
// XMFLOAT4 = float x 4 (16バイト) — HLSLの float4 に対応
struct Vertex
{
    XMFLOAT3 position;  // 位置（x, y, z）
    XMFLOAT4 color;     // 色（r, g, b, a）
};

// ============================================================
// 定数バッファ構造体
// ============================================================
// HLSL側の cbuffer と完全にメモリレイアウトが一致する必要がある。
//
// 重要: 定数バッファは16バイトアライメントが必須。
// XMMATRIX は 64バイト（float 4x4 = 16 float = 64バイト）で、
// 16バイトの倍数なので問題ない。
// もし float や int を追加する場合は、パディングに注意すること。
struct alignas(16) ConstantBuffer
{
    XMMATRIX wvp;   // ワールド * ビュー * プロジェクション行列
};

// ============================================================
// CompileShader — HLSLファイルをコンパイルする汎用関数
// ============================================================
// filePath:   HLSLファイルのパス（例: L"shaders.hlsl"）
// entryPoint: エントリポイント関数名（例: "vs_main"）
// profile:    シェーダーモデル（例: "vs_5_0" または "ps_5_0"）
// blob:       [出力] コンパイル結果のバイナリデータ
// ============================================================
bool CompileShader(
    const wchar_t* filePath,
    const char* entryPoint,
    const char* profile,
    ComPtr<ID3DBlob>& blob)
{
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
# ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG;
    compileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompileFromFile(
        filePath,                           // HLSLファイルのパス
        nullptr,                            // マクロ定義（なし）
        D3D_COMPILE_STANDARD_FILE_INCLUDE,  // #includeの標準ハンドラ
        entryPoint,                         //  エントリポイント関数名
        profile,                            // シェーダーモデル
        compileFlags,                       // コンパイルフラグ
        0,                                  // エフェクトフラグ(0)
        blob.GetAddressOf(),                // [出力] コンパイル結果
        errorBlob.GetAddressOf()            // [出力] エラーメッセージ
    );

    // コンパイルエラーがあれば出力ウィンドウに表示
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA(
                static_cast<const char*>(errorBlob->GetBufferPointer())
            );
        }
        return false;
    }

    return true;
}


// ============================================================
// InitD3D — Direct3D 11の初期化
// ============================================================
// 成功したら true、失敗したら false を返す。
// この関数で以下の4つを作成する:
//   1. ID3D11Device           (g_device)
//   2. ID3D11DeviceContext     (g_deviceContext)
//   3. IDXGISwapChain          (g_swapChain)
//   4. ID3D11RenderTargetView  (g_renderTargetView)
// ============================================================
bool  InitD3D(HWND hwnd)
{
   // --------------------------------------------------------
   // 1. スワップチェインの設定を記述する
   // --------------------------------------------------------
   // DXGI_SWAP_CHAIN_DESC は「どんなスワップチェインが欲しいか」の注文書。
   // まずゼロ初期化してから、必要なフィールドだけ埋める。
   DXGI_SWAP_CHAIN_DESC scd = {};

   // バックバッファの幅と高さ（ウィンドウのクライアント領域と一致させる）
   scd.BufferDesc.Width = WINDOW_WIDTH;
   scd.BufferDesc.Height = WINDOW_HEIGHT;

   // リフレッシュレート（60Hz）
   // Numerator / Denominator = 60/1 = 60fps
   scd.BufferDesc.RefreshRate.Numerator = 60;
   scd.BufferDesc.RefreshRate.Denominator = 1;

   // ピクセルフォーマット
   // R8G8B8A8_UNORM = 赤緑青+アルファ各8ビット、0.0〜1.0の範囲に正規化
   // 最も一般的なフォーマット
   scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;


   // マルチサンプリング（アンチエイリアス）の設定
   // Count=1, Quality=0 はマルチサンプリングなし（最もシンプル）
   scd.SampleDesc.Count = 1;
   scd.SampleDesc.Quality = 0;

   // バックバッファの用途: レンダーターゲット（描画先）として使う
   scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

   // バックバッファの数（ダブルバッファリングなので1）
   // 「フロント1枚 + バック1枚」で合計2枚。ここで指定するのはバック側の枚数。
   scd.BufferCount = 1;

   // 描画先のウィンドウハンドル
   scd.OutputWindow = hwnd;

   // ウィンドウモード（TRUE）/ フルスクリーン（FALSE）
   scd.Windowed = TRUE;

   // スワップ時の動作: DISCARD = 前のバッファの内容を保持しない（最も効率的）
   scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

   // --------------------------------------------------------
   // 2. デバイスとスワップチェインを一括作成
   // --------------------------------------------------------
   // D3D11CreateDeviceAndSwapChain は名前の通り、
   // Device, DeviceContext, SwapChain の3つを一度に作ってくれる便利関数。
   //
   // 引数が多いが、重要なのは以下:
   //   - D3D_DRIVER_TYPE_HARDWARE: GPUを使う（ソフトウェアレンダリングではない）
   //   - D3D11_CREATE_DEVICE_DEBUG: デバッグレイヤーを有効にする（開発中は必須）
   //   - D3D11_SDK_VERSION: 常にこの定数を渡す

   UINT createDeviceFlags = 0;
#ifdef DEBUG
   // デバッグビルド時のみデバッグレイヤーを有効にする。
   // APIの誤用をVisual Studioの出力ウィンドウに警告してくれる。
   // リリースビルドでは無効にすること（パフォーマンスに影響する）。
   createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // DEBUG

   // 対応するフィーチャーレベル（DirectXのバージョン）を指定
   // 11_0 を最優先にし、対応していなければ10_1, 10_0にフォールバック
   D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
   };

   D3D_FEATURE_LEVEL featureLevelOut;   // 実際に選ばれたフィーチャーレベルが返る

   HRESULT hr = D3D11CreateDeviceAndSwapChain(
       nullptr,                         // アダプター: nullptrでデフォルトGPU
       D3D_DRIVER_TYPE_HARDWARE,        //ハードウェア（GPU)を使用
       nullptr,                         // ソフトウェアラスタライザー（使わない）
       createDeviceFlags,               // デバッグフラッグ
       featureLevels,                   // フィーチャーレベルの配列
       _countof(featureLevels),         // 配列の要素数
       D3D11_SDK_VERSION,               // 常にこの値
       &scd,                            // スワップチェインの設定
       g_swapChain.GetAddressOf(),      // [出力] スワップチェイン
       g_device.GetAddressOf(),         // [出力] デバイス
       &featureLevelOut,                // [出力] 選ばれたフィーチャーレベル
       g_deviceContext.GetAddressOf()   // [出力] デバイスコンテキスト
   );

   // HRESULTはWindowsのエラーコード。FAILED()マクロで失敗判定
   if (FAILED(hr))
   {
       MessageBox(nullptr, L"D3D11CreateDeviceAndSwapChainに失敗しました", L"エラー", MB_OK);
       return false;
   }

   // --------------------------------------------------------
   // 3. レンダーターゲットビューの作成
   // --------------------------------------------------------
   // スワップチェインが持つバックバッファ（テクスチャ）を取得し、
   // それを「描画先」として使うためのビューを作る。
   //
   // 手順:
   //   (a) SwapChain::GetBuffer() でバックバッファのテクスチャを取得
   //   (b) Device::CreateRenderTargetView() でビューを作成

   // (a) バックバッファテクスチャを取得
   ComPtr<ID3D11Texture2D> backBuffer;
   hr = g_swapChain->GetBuffer(
       0,                                                       // バッファのインデックス(0 = 最初のバックバッファ)
       __uuidof(ID3D11Texture2D),                               // 取得したいインターフェースの型
       reinterpret_cast<void**>(backBuffer.GetAddressOf())      // [出力] テクスチャ
   );

   if (FAILED(hr))
   {
       MessageBox(nullptr, L"バックバッファの取得に失敗しました", L"エラー", MB_OK);
       return false;
   }

   // (b) レンダーターゲットビューを作成
   hr = g_device->CreateRenderTargetView(
       backBuffer.Get(),                    // 対象のテクスチャ（バックバッファ）
       nullptr,                             // ビューの詳細設定（nullptrでデフォルト）
       g_renderTargetView.GetAddressOf()    // [出力] レンダーターゲットビュー
   );
   // backBufferはこのスコープを抜けると自動でReleaseする
   // レンダーターゲットビューがバックバッファへの参照を保持しているので問題はない
    
   if (FAILED(hr))
   {
       MessageBox(nullptr, L"レンダーターゲットビューの作成に失敗しました", L"エラー", MB_OK);
       return false;
   }

   // --------------------------------------------------------
   // 深度バッファの作成
   // --------------------------------------------------------

   //(a) 深度テクスチャの作成
   D3D11_TEXTURE2D_DESC depthDesc = {};
   depthDesc.Width = WINDOW_WIDTH;
   depthDesc.Height = WINDOW_HEIGHT;
   depthDesc.MipLevels = 1;
   depthDesc.ArraySize = 1;
   depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 24ビット深度 + 8ビットステンシル
   depthDesc.SampleDesc.Count = 1;
   depthDesc.SampleDesc.Quality = 0;
   depthDesc.Usage = D3D11_USAGE_DEFAULT;
   depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

   ComPtr<ID3D11Texture2D> depthTexture;
   hr = g_device->CreateTexture2D(&depthDesc, nullptr, depthTexture.GetAddressOf());
   if (FAILED(hr)) return false;

   //(b) 深度ステンシルビューの作成
   hr = g_device->CreateDepthStencilView(
       depthTexture.Get(), nullptr, g_depthStencilView.GetAddressOf());
   if (FAILED(hr)) return false;

   // --------------------------------------------------------
   // 4. ビューポートの設定
   // --------------------------------------------------------
   // ビューポートは「描画結果をウィンドウのどの範囲に表示するか」を定義する。
   // 通常はウィンドウ全体に描画する。
   D3D11_VIEWPORT viewport = {};
   viewport.TopLeftX = 0.0f;
   viewport.TopLeftY = 0.0f;
   viewport.Width = static_cast<float>(WINDOW_WIDTH);
   viewport.Height = static_cast<float>(WINDOW_HEIGHT);
   viewport.MinDepth = 0.0f;  // 深度バッファの最小値（通常0.0）
   viewport.MaxDepth = 1.0f;  // 深度バッファの最大値（通常1.0）

   // ビューポートの設定はDeviceContext（描画コマンド担当）に対して行う
   g_deviceContext->RSSetViewports(1, &viewport);

   // --------------------------------------------------------
   // 5. シェーダーのコンパイルと生成
   // --------------------------------------------------------

   // (a) 頂点シェーダーのコンパイル
   ComPtr<ID3DBlob> vsBlob;
   if (!CompileShader(L"shaders.hlsl", "vs_main", "vs_5_0", vsBlob))
   {
       MessageBox(nullptr, L"頂点シェーダーのコンパイルに失敗", L"エラー", MB_OK);
       return false;
   }

   // コンパイル結果から頂点シェーダーオブジェクトを生成
   hr = g_device->CreateVertexShader(
       vsBlob->GetBufferPointer(),  // コンパイル済みバイトコード
       vsBlob->GetBufferSize(),     // バイトコードのサイズ
       nullptr,                     // クラスリンケージ（使わない）
       g_vertexShader.GetAddressOf()
   );
   if (FAILED(hr)) return false;

   // (b) ピクセルシェーダーのコンパイル
   ComPtr<ID3DBlob> psBlob;
   if (!CompileShader(L"shaders.hlsl", "ps_main", "ps_5_0", psBlob))
   {
       MessageBox(nullptr, L"ピクセルシェーダーのコンパイルに失敗", L"エラー", MB_OK);
       return false;
   }

   // コンパイル結果からピクセルシェーダーオブジェクトを生成
   hr = g_device->CreatePixelShader(
       psBlob->GetBufferPointer(),
       psBlob->GetBufferSize(),
       nullptr,
       g_pixelShader.GetAddressOf()
   );
   if (FAILED(hr)) return false;

   // --------------------------------------------------------
   // 入力レイアウトの作成
   // --------------------------------------------------------
   // D3D11_INPUT_ELEMENT_DESC の配列で、頂点の各フィールドを記述する。
   //
   // 各フィールドの意味:
   //   SemanticName:      HLSLのセマンティクス名（"POSITION", "COLOR"）
   //   SemanticIndex:     同名セマンティクスが複数ある場合のインデックス（通常0）
   //   Format:            データ型（DXGI_FORMAT_R32G32B32_FLOAT = float3）
   //   InputSlot:         頂点バッファのスロット番号（通常0）
   //   AlignedByteOffset: 構造体の先頭からのバイトオフセット
   //                      D3D11_APPEND_ALIGNED_ELEMENT で自動計算もできる
   //   InputSlotClass:    頂点ごと（PER_VERTEX_DATA）かインスタンスごとか
   //   InstanceDataStepRate: インスタンシング用（0 = 使わない）
   D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
   {
       {
           "POSITION",                      // SemanticName — HLSL側の : POSITION に対応
           0,                               // SemanticIndex
           DXGI_FORMAT_R32G32B32_FLOAT,     // Format — XMFLOAT3 = float x 3
           0,                               // InputSlot
           0,                               // Format — XMFLOAT3 = float x 3
           D3D11_INPUT_PER_VERTEX_DATA,     // InputSlotClass
           0                                // InstanceDataStepRate
       },
       {
           "COLOR",                         // SemanticName — HLSL側の : COLOR に対応  
           0,                               // SemanticIndex
           DXGI_FORMAT_R32G32B32A32_FLOAT,  // Format — XMFLOAT4 = float x 4
           0,                               // InputSlot
           12,                              // AlignedByteOffset — XMFLOAT3のあと = 12バイト
                                            // D3D11_APPEND_ALIGNED_ELEMENT を使うと自動で計算してくれる
           D3D11_INPUT_PER_VERTEX_DATA,     // InputSlotClass
           0                                // InstanceDataStepRate
       },
   };

   // 入力レイアウトの作成
   // 頂点シェーダーのコンパイル結果（vsBlob）が必要 — シェーダーの入力と
   // レイアウト定義が一致していることを検証するため
   hr = g_device->CreateInputLayout(
       layoutDesc,                      // レイアウト記述の配列
       _countof(layoutDesc),            // 要素数
       vsBlob->GetBufferPointer(),      // 頂点シェーダーのバイトコード
       vsBlob->GetBufferSize(),         // バイトコードのサイズ
       g_inputLayout.GetAddressOf()     // [出力」入力レイアウト
   );
   if (FAILED(hr))
   {
       MessageBox(nullptr, L"入力レイアウトの作成に失敗", L"エラー", MB_OK);
       return false;
   }

   // --------------------------------------------------------
   // 定数バッファの作成
   // --------------------------------------------------------
   // 頂点バッファと同じ CreateBuffer() で作るが、
   // BindFlags が D3D11_BIND_CONSTANT_BUFFER になる。
   // 初期データは渡さない（毎フレーム UpdateSubresource で更新するため）。
   D3D11_BUFFER_DESC cbDesc = {};
   cbDesc.Usage = D3D11_USAGE_DEFAULT;
   cbDesc.ByteWidth = sizeof(ConstantBuffer);
   cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

   hr = g_device->CreateBuffer(&cbDesc, nullptr, g_constantBuffer.GetAddressOf());
   if (FAILED(hr))
   {
       MessageBox(nullptr, L"定数バッファの作成に失敗", L"エラー", MB_OK);
       return false;
   }


   // --------------------------------------------------------
   // 6. 頂点データの定義と頂点バッファの作成
   // --------------------------------------------------------

   // 三角形の3頂点を定義する。
   //
   // 座標系について:
   //   今回は行列変換をしない（頂点シェーダーがそのまま出力する）ので、
   //   「クリップ空間」の座標を直接指定する。
   //   クリップ空間は以下の範囲:
   //     X: -1.0（左端）〜 +1.0（右端）
   //     Y: -1.0（下端）〜 +1.0（上端）
   //     Z:  0.0（最も手前）〜 +1.0（最も奥）
   //
   // 頂点の順番（ワインディングオーダー）:
   //   DirectXは**時計回り**が表面（フロントフェイス）。
   //   反時計回りだと裏面と見なされ、カリングで描画されない。

   // ============================================================
   // キューブの頂点データ（8頂点）
   // ============================================================
   //
   //       4 -------- 5
   //      /|         /|
   //     / |        / |
   //    0 -------- 1  |
   //    |  7 ------|- 6
   //    | /        | /
   //    |/         |/
   //    3 -------- 2
   //

   Vertex vertices[] =
   {
       //        位置 (x,    y,    z)      色 (r,   g,   b,   a)
       { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) }, // 0: 左上手前 赤
       { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) }, // 1: 右上手前 緑
       { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }, // 2: 右下手前 青
       { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) }, // 3: 左下手前 黄
       { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) }, // 4: 左上奥   マゼンタ
       { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) }, // 5: 右上奥   シアン
       { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) }, // 6: 右下奥   白
       { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f) }, // 7: 左下奥   灰
   };

   Vertex floorVertices[] =
   {
       { XMFLOAT3(-10.0f, -0.5f, 10.0f), XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f) },
       { XMFLOAT3(10.0f, -0.5f, 10.0f), XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f) },
       { XMFLOAT3(10.0f, -0.5f, -10.0f), XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f) },
       { XMFLOAT3(-10.0f, -0.5f, -10.0f), XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f) },
   };

   // 頂点バッファの設定
   // D3D11_BUFFER_DESC は「こういうバッファが欲しい」という注文書。
   D3D11_BUFFER_DESC bufferDesc = {};
   bufferDesc.Usage = D3D11_USAGE_DEFAULT;              // GPUから読み書き可能
   bufferDesc.ByteWidth = sizeof(vertices);            // バッファのサイズ
   bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;     // 頂点バッファとして使う
   bufferDesc.CPUAccessFlags = 0;                       // CPUからはアクセスしない

   // D3D11_SUBRESOURCE_DATA はバッファに最初に入れるデータの指定。
   // pSysMem に頂点配列のポインタを渡す。
   D3D11_SUBRESOURCE_DATA initData = {};
   initData.pSysMem = vertices;

   // バッファを作成（Device が担当）
   hr = g_device->CreateBuffer(&bufferDesc, &initData, g_vertexBuffer.GetAddressOf());
   if (FAILED(hr))
   {
       MessageBox(nullptr, L"頂点バッファの作成に失敗", L"エラー", MB_OK);
       return false;
   }

   // 床頂点バッファの設定
   bufferDesc.ByteWidth = sizeof(floorVertices);
   initData.pSysMem = floorVertices;
   hr = g_device->CreateBuffer(&bufferDesc, &initData, g_floorVertexBuffer.GetAddressOf());
   if (FAILED(hr))
   {
       MessageBox(nullptr, L"床頂点バッファの作成に失敗", L"エラー", MB_OK);
       return false;
   }

   // ============================================================
   // インデックスデータ（12三角形 = 36インデックス）
   // ============================================================
   // 各面を2つの三角形に分割する。
   // 時計回り（正面から見て）がフロントフェイス。
   UINT indices[] =
   {
       // 前面
       0, 1, 2,
       0, 2, 3,
       // 背面
       5, 4, 7,
       5, 7, 6,
       // 上面
       4, 5, 1,
       4, 1, 0,
       // 底面
       3, 2, 6,
       3, 6, 7,
       // 左面
       4, 0, 3,
       4, 3, 7,
       // 右面
       1, 5, 6,
       1, 6, 2,
   };

   UINT floorIndices[] = { 0, 1, 2, 0, 2, 3 };

   // インデックスバッファの作成
   D3D11_BUFFER_DESC ibDesc = {};
   ibDesc.Usage = D3D11_USAGE_DEFAULT;
   ibDesc.ByteWidth = sizeof(indices);
   ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

   D3D11_SUBRESOURCE_DATA ibData = {};
   ibData.pSysMem = indices;

   hr = g_device->CreateBuffer(&ibDesc, &ibData, g_indexBuffer.GetAddressOf());
   if (FAILED(hr)) return false;

   // 床インデックスバッファの作成
   ibDesc.ByteWidth = sizeof(floorIndices);
   ibData.pSysMem = floorIndices;
   hr = g_device->CreateBuffer(&ibDesc, &ibData, g_floorIndexBuffer.GetAddressOf());
   if (FAILED(hr)) return false;


   return true;
}

// ============================================================
// UpdateCamera — 毎フレームのカメラ状態更新
// ============================================================
void UpdateCamera(float deltaTime)
{
    // --------------------------------------------------------
    // マウス入力 → ヨーとピッチの更新
    // --------------------------------------------------------
    if (g_mouseCaptured)
    {
		POINT currentMousePos;
		GetCursorPos(&currentMousePos);

        // 前フレームからの移動量
        float deltaX = static_cast<float>(currentMousePos.x - g_lastMousePos.x);
        float deltaY = static_cast<float>(currentMousePos.y - g_lastMousePos.y);

        // 移動量に感度を掛けてヨーとピッチを更新
		g_camYaw += deltaX * g_mouseSensitivity;
		g_camPitch += deltaY * g_mouseSensitivity;

        // ピッチを制限する（真上・真下を超えないように）
        // ±89度に制限。90度ちょうどだとforward.yが±1になり、
        // 外積でrightベクトルが計算できなくなる（ジンバルロック）
		constexpr float maxPitch = XMConvertToRadians(89.0f);
        if (g_camPitch > maxPitch) g_camPitch = maxPitch;
		if (g_camPitch < -maxPitch) g_camPitch = -maxPitch;

        // マウスカーソルを画面中央に戻す
        // こうすることで、マウスが画面端に到達して動けなくなる問題を防ぐ
        RECT rect;
		GetWindowRect(GetForegroundWindow(), &rect);
		int centerX = (rect.left + rect.right) / 2;
		int centerY = (rect.top + rect.bottom) / 2;
		SetCursorPos(centerX, centerY);

        // 「画面中央に戻した位置」を次フレームの基準にする
		g_lastMousePos.x = centerX;
		g_lastMousePos.y = centerY;
    }
    
    // --------------------------------------------------------
    // 1. ヨーとピッチから前方向ベクトルを計算
    // --------------------------------------------------------
    // 球面座標 → デカルト座標の変換
    XMFLOAT3 forward;
	forward.x = cosf(g_camPitch) * sinf(g_camYaw);
	forward.y = -sinf(g_camPitch);
	forward.z = cosf(g_camPitch) * cosf(g_camYaw);

    // XMFLOAT3 → XMVECTOR に変換（DirectXMathの演算にはXMVECTORが必要）
	XMVECTOR forwardVec = XMVector3Normalize(XMLoadFloat3(&forward));

    // --------------------------------------------------------
    // 2. 右方向ベクトルを計算（外積）
    // --------------------------------------------------------
    // worldUp(0,1,0) と forward の外積で right が求まる
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(worldUp, forwardVec));

    // --------------------------------------------------------
    // 3. WASD入力に基づいてカメラ位置を移動
    // --------------------------------------------------------
	float speed = g_camSpeed * deltaTime; // フレームレートに依存しない移動量
	XMVECTOR posVec = XMLoadFloat3(&g_camPosition);

    if (g_keyW) posVec = XMVectorAdd(posVec, XMVectorScale(forwardVec, speed));
    if (g_keyS) posVec = XMVectorAdd(posVec, XMVectorScale(forwardVec, -speed));
    if (g_keyD) posVec = XMVectorAdd(posVec, XMVectorScale(rightVec, speed));
    if (g_keyA) posVec = XMVectorAdd(posVec, XMVectorScale(rightVec, -speed));

    // XMVECTOR → XMFLOAT3 に戻す
	XMStoreFloat3(&g_camPosition, posVec);
}

// ============================================================
// Render — 毎フレームの描画処理
// ============================================================
void Render(float deltaTime)
{
    // --------------------------------------------------------
    // 1. レンダーターゲットの設定
    // --------------------------------------------------------
    // 「これから描画する先はこのレンダーターゲットですよ」とDeviceContextに伝える。
    // 毎フレーム設定するのが安全（他の処理で変更される可能性があるため）。
    //
    // 第1引数: レンダーターゲットの数（通常は1）
    // 第2引数: レンダーターゲットビューの配列（今は1つだけ）
    // 第3引数: 深度ステンシルビュー（今はまだ使わないのでnullptr）
    g_deviceContext->OMSetRenderTargets(
        1,
        g_renderTargetView.GetAddressOf(),
		g_depthStencilView.Get()
    );

    // --------------------------------------------------------
    // 2. 画面のクリア（塗りつぶし）
    // --------------------------------------------------------
    // ClearRenderTargetView は指定した色でレンダーターゲット全体を塗りつぶす。
    // RGBA（赤、緑、青、アルファ）の4つのfloat値で色を指定する。
    // 各値は 0.0f 〜 1.0f の範囲
    float clearColor[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
    g_deviceContext->ClearRenderTargetView(g_renderTargetView.Get(), clearColor);


    // 深度バッファのクリア（毎フレーム1.0でリセット）
    // 1.0 は最も遠い深度値。描画されたピクセルはこれより小さい値を持つ。
    g_deviceContext->ClearDepthStencilView(
        g_depthStencilView.Get(),
        D3D11_CLEAR_DEPTH,
        1.0f,  // 深度の初期値
        0      // ステンシルの初期値
    );

    // --------------------------------------------------------
    // WVP行列の計算
    // --------------------------------------------------------

    // 回転角度（毎フレーム増加させる）
    static float angle = 0.0f;

    angle += deltaTime * 1.0f;

    // ワールド行列: Y軸で回転
    // 1つ目のキューブ（左側）
    //XMMATRIX world = XMMatrixRotationY(angle);

    // ビュー行列: カメラの設定
    // カメラの前方向を計算
    XMFLOAT3 forward;
    forward.x = cosf(g_camPitch) * sinf(g_camYaw);
    forward.y = -sinf(g_camPitch);
    forward.z = cosf(g_camPitch) * cosf(g_camYaw);

    XMVECTOR eye = XMLoadFloat3(&g_camPosition);        // カメラの位置
	XMVECTOR forwardVec = XMLoadFloat3(&forward);       // 前方向ベクトル
    XMVECTOR target = XMVectorAdd(eye, forwardVec);     // 注視点
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);  // 上方向
	XMMATRIX view = XMMatrixLookAtLH(eye, target, up);

    // プロジェクション行列: 投資投影
	constexpr float fov = XMConvertToRadians(60.0f);                                                  // 視野角60度
	float aspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);    // アスペクト比
    float nearZ = 0.1f;                                                                     // ニアクリップ面
    float farZ = 100.0f;                                                                    // ファークリップ面
    XMMATRIX projection = XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);

	// WVP行列 = ワールド x ビュー x プロジェクション
    //XMMATRIX wvp = world * view * projection;

    // --------------------------------------------------------
    // 定数バッファの更新
    // --------------------------------------------------------
    // 重要: DirectXMathは行優先(row-major)、HLSLはデフォルトで列優先(column-major)。
    // mul() が正しく動作するように、行列を転置してから渡す
	ConstantBuffer cb;
	//cb.wvp = XMMatrixTranspose(wvp);

    // UpdateSubresource でGPU上の定数バッファを更新する
	//g_deviceContext->UpdateSubresource(g_constantBuffer.Get(), 0, nullptr, &cb, 0, 0);

    // 定数バッファを頂点シェーダーのスロット0に設定
    // register(b0) に対応する
    g_deviceContext->VSSetConstantBuffers(0, 1, g_constantBuffer.GetAddressOf());


    // --------------------------------------------------------
    // パイプラインの設定（描画前に毎回行う）
    // --------------------------------------------------------

    // (1)入力アセンブラの設定
    // 「頂点データの読み方はこうだよ」とGPUに伝える

    // 入力レイアウトの設定
	g_deviceContext->IASetInputLayout(g_inputLayout.Get());

    // 頂点バッファをInput Assemblerにバインド
	UINT stride = sizeof(Vertex);   // 頂点1つ分のサイズ
    UINT offset = 0;                // 読み開始位置のオフセット
    g_deviceContext->IASetVertexBuffers(
        0,                              // スロット番号（入力レイアウトのInputSlotに対応）
        1,                              // バインドするバッファの数
        g_vertexBuffer.GetAddressOf(),  // バッファの配列
        &stride,                        // ストライドの配列
        &offset                        // オフセットの配列
    );

    g_deviceContext->IASetIndexBuffer(g_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    // プリミティブトポロジーの設定
    // 「頂点を3つずつグループにして三角形として描け」という指示
	g_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //(2) シェーダーの設定
    g_deviceContext->VSSetShader(g_vertexShader.Get(), nullptr, 0);
    g_deviceContext->PSSetShader(g_pixelShader.Get(), nullptr, 0);

    // --------------------------------------------------------
    // 描画コマンドの発行
    // --------------------------------------------------------
    // DrawIndexed に変更（Draw → DrawIndexed）
    // 第1引数: インデックスの総数（36 = 12三角形 × 3頂点）
    // 第2引数: 開始インデックスのオフセット
    // 第3引数: 頂点のベースオフセット
    //g_deviceContext->DrawIndexed(36, 0, 0);

    XMFLOAT3 cubePositions[] = {
    { 0.0f, 0.0f,  0.0f },
    { 3.0f, 0.0f,  0.0f },
    {-3.0f, 0.0f,  2.0f },
    { 0.0f, 0.0f,  5.0f },
    { 2.0f, 1.0f, -2.0f },
    };

    for (int i = 0; i < 5; i++)
    {
        XMMATRIX cubeWorld = XMMatrixRotationY(angle * (i + 1) * 0.3f)
            * XMMatrixTranslation(cubePositions[i].x,
                cubePositions[i].y,
                cubePositions[i].z);
        cb.wvp = XMMatrixTranspose(cubeWorld * view * projection);
        g_deviceContext->UpdateSubresource(g_constantBuffer.Get(), 0, nullptr, &cb, 0, 0);
        g_deviceContext->DrawIndexed(36, 0, 0);
    }


    XMMATRIX floorWorld = XMMatrixIdentity();   // 回転なし
    XMMATRIX floorWvp = floorWorld * view * projection;
    cb.wvp = XMMatrixTranspose(floorWvp);
    g_deviceContext->UpdateSubresource(g_constantBuffer.Get(), 0, nullptr, &cb, 0, 0);
    g_deviceContext->VSSetConstantBuffers(0, 1, g_constantBuffer.GetAddressOf());
    g_deviceContext->IASetVertexBuffers(0, 1, g_floorVertexBuffer.GetAddressOf(), &stride, &offset);
    g_deviceContext->IASetIndexBuffer(g_floorIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    g_deviceContext->DrawIndexed(6, 0, 0);

    // --------------------------------------------------------
    // 3. バックバッファをフロントバッファに切り替える（画面に表示）
    // --------------------------------------------------------
    // Present() を呼ぶことで、バックバッファの内容が実際に画面に表示される。
    //
    // 第1引数: 垂直同期（VSync）の設定
    //   0 = VSync無効（可能な限り高速に描画、ティアリングの可能性あり）
    //   1 = VSync有効（モニターのリフレッシュレートに同期、通常60fps上限）
    //   今は1（VSync有効）にする。開発中はティアリングを防げるので見た目が安定する。
    //
    // 第2引数: プレゼントフラグ（通常は0）
    g_swapChain->Present(1, 0);
}

// ============================================================
// WinMain — Win32アプリケーションのエントリポイント
// ============================================================
// 通常のC++プログラムは main() が起点だが、
// Win32プログラムでは WinMain() がOSから呼ばれる。
//
// 引数の意味:
//   hInstance     — このアプリケーションのインスタンスハンドル（OSがアプリを識別する番号）
//   hPrevInstance — 常にNULL（Win16時代の名残で、現在は使われない）
//   lpCmdLine    — コマンドライン引数の文字列
//   nCmdShow     — ウィンドウの初期表示方法（最大化、最小化など）
// ============================================================
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    // 使わない引数を明示的にマーク（コンパイラ警告の抑制）
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // --------------------------------------------------------
    // 1. ウィンドウクラスの登録
    // --------------------------------------------------------
    // WNDCLASSEX構造体にウィンドウの「設計図」を記述し、OSに登録する。
    // ウィンドウクラスとは「このアプリのウィンドウはこういう見た目・動作です」
    // という定義のこと。C++のクラスとは無関係。
    WNDCLASSEX wc = {};                             // ゼロ初期化
    wc.cbSize        = sizeof(WNDCLASSEX);          // この構造体のサイズ（OSが内部で使う）
    wc.style         = CS_HREDRAW | CS_VREDRAW;     // ウィンドウサイズ変更時に再描画
    wc.lpfnWndProc   = WndProc;                     // メッセージ処理関数のポインタ
    wc.hInstance     = hInstance;                    // アプリケーションのインスタンス
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW); // マウスカーソルの形状
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);  // 背景色（OSのデフォルト色）
    wc.lpszClassName = L"DX11DemoWindowClass";       // ウィンドウクラスの識別名

    // OSに登録。失敗したら終了
    if (!RegisterClassEx(&wc))
    {
        MessageBox(nullptr, L"ウィンドウクラスの登録に失敗しました", L"エラー", MB_OK);
        return -1;
    }

    // --------------------------------------------------------
    // 2. ウィンドウのサイズ計算
    // --------------------------------------------------------
    // ウィンドウには「タイトルバー」と「枠線」がある。
    // WINDOW_WIDTH x WINDOW_HEIGHT は「描画領域（クライアント領域）」のサイズにしたい。
    // AdjustWindowRect で枠線の分を加算した実際のウィンドウサイズを計算する。
    //
    // これをしないと、描画領域がWINDOW_WIDTHより小さくなってしまう。
    // DirectXのバックバッファサイズと描画領域のサイズが一致しないと、
    // 引き伸ばしやレターボックスの原因になる。
    RECT windowRect = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    int adjustedWidth  = windowRect.right - windowRect.left;
    int adjustedHeight = windowRect.bottom - windowRect.top;

    // --------------------------------------------------------
    // 3. ウィンドウの作成
    // --------------------------------------------------------
    HWND hwnd = CreateWindowEx(
        0,                          // 拡張ウィンドウスタイル（今回は不要）
        L"DX11DemoWindowClass",     // Step 1で登録したクラス名
        L"DirectX 11 Demo",        // タイトルバーに表示されるテキスト
        WS_OVERLAPPEDWINDOW,        // ウィンドウスタイル（タイトルバー、枠線、最大化ボタン等）
        CW_USEDEFAULT,              // X座標（OSにお任せ）
        CW_USEDEFAULT,              // Y座標（OSにお任せ）
        adjustedWidth,              // 枠線込みのウィンドウ幅
        adjustedHeight,             // 枠線込みのウィンドウ高さ
        nullptr,                    // 親ウィンドウ（なし）
        nullptr,                    // メニュー（なし）
        hInstance,                  // アプリケーションインスタンス
        nullptr                     // 追加データ（なし）
    );

    if (!hwnd)
    {
        MessageBox(nullptr, L"ウィンドウの作成に失敗しました", L"エラー", MB_OK);
        return -1;
    }

    // --------------------------------------------------------
    // 4. ウィンドウの表示
    // --------------------------------------------------------
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // --------------------------------------------------------
    // DirectX11の初期化
    // --------------------------------------------------------
    if (!InitD3D(hwnd))
    {
        return -1;
    }

    // --------------------------------------------------------
    // タイマーの初期化
    // --------------------------------------------------------
    // QueryPerformanceCounter は CPU の高精度タイマーを読む関数。
    // Frequency（1秒あたりのカウント数）を取得し、
    // フレーム間のカウント差を Frequency で割ると経過秒数が求まる
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER previousTime;
    QueryPerformanceCounter(&previousTime);

    float deltaTime = 0.0f;

    // --------------------------------------------------------
    // 5. ゲームループ
    // --------------------------------------------------------
    // PeekMessage()を使ったリアルタイムループ
    // メッセージがあれば処理し、なければゲームの更新と描画を行う。
    // ゲームはこの構造が基本になる。
    MSG msg = {};
    bool isRunning = true;
    float fpsTimer = 0.0f;
    int   frameCount = 0;

    while (isRunning)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                isRunning = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (isRunning)
        {
            // deltaTime計算
            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);
            float deltaTime = static_cast<float>(currentTime.QuadPart - previousTime.QuadPart)
                            / static_cast<float>(frequency.QuadPart);
            previousTime = currentTime;

            // Update（今は空）
            // TODO: ゲームロジックの更新

            // Render
            UpdateCamera(deltaTime);
            Render(deltaTime);

            // FPS表示
            fpsTimer += deltaTime;
            frameCount++;
            if (fpsTimer >= 1.0f)
            {
                wchar_t fpsText[64];
                swprintf_s(fpsText, L"DX11 Demo - FPS: %d\n", frameCount);
                OutputDebugString(fpsText);

                // タイトルバーにもFPSを表示（便利）
                SetWindowText(hwnd, fpsText);

                fpsTimer = 0.0f;
                frameCount = 0;
            }
        }
    }

    return static_cast<int>(msg.wParam);
}

// ============================================================
// WndProc — ウィンドウプロシージャ（メッセージ処理）
// ============================================================
// OSがウィンドウにメッセージを送るたびに、この関数が呼ばれる。
//
// 引数:
//   hwnd   — メッセージの宛先ウィンドウ
//   msg    — メッセージの種類（WM_KEYDOWN, WM_DESTROY など）
//   wParam — メッセージの追加情報1（キーコードなど）
//   lParam — メッセージの追加情報2（スキャンコードなど）
//
// 戻り値:
//   処理した場合は 0、処理しなかった場合は DefWindowProc() に委譲
// ============================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    // ESCキーが押された → ウィンドウを閉じる
    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
			if (g_mouseCaptured)
            {
				// マウスキャプチャを解放
				g_mouseCaptured = false;
                ShowCursor(TRUE);
                ClipCursor(nullptr);
            }
            else
            {
                DestroyWindow(hwnd); // ウィンドウを破棄 → WM_DESTROYが送られる
            }
			break;
        case 'W':
			g_keyW = true;
            break;
        case 'A':
            g_keyA = true;
            break;
        case 'S':
            g_keyS = true;
            break;
        case 'D':
            g_keyD = true;
            break;
        }
        return 0;
    
    case WM_KEYUP:
        switch (wParam)
        {
        case 'W':
            g_keyW = false;
            break;
        case 'A':
            g_keyA = false;
            break;
        case 'S':
            g_keyS = false;
            break;
        case 'D':
            g_keyD = false;
            break;
        }
		return 0;

    // ---- 左クリックでマウスキャプチャ開始 ----
    case WM_LBUTTONDOWN:
        if (!g_mouseCaptured)
        {
            g_mouseCaptured = true;
			ShowCursor(FALSE); // カーソルを非表示

			// カーソルの移動をウィンドウ内に制限
            RECT clipRect;
			GetClientRect(hwnd, &clipRect);                                         // クライアント領域の矩形を取得
			MapWindowPoints(hwnd, nullptr, reinterpret_cast<POINT*>(&clipRect), 2); // クライアント座標をスクリーン座標に変換
			ClipCursor(&clipRect);                                                  // カーソルの移動を矩形内に制限


            // 現在位置を記録
			GetCursorPos(&g_lastMousePos);
        }
        return 0;

    // ウィンドウが破棄された → アプリケーション終了をOSに通知
    case WM_DESTROY:
		ClipCursor(nullptr);    // 念のためカーソルの制限を解除
		ShowCursor(TRUE);       // 念のためカーソルを表示
        PostQuitMessage(0);     // メッセージキューにWM_QUITを投入 → GetMessage()が0を返す
        return 0;
    }

    // 自分で処理しないメッセージはOSのデフォルト処理に委譲
    return DefWindowProc(hwnd, msg, wParam, lParam);
}