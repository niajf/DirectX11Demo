# DirectX 11 3D Tech Demo

C++ と DirectX 11 を用いて、レンダリングパイプラインをゼロから構築した3D技術デモです。
ゲームエンジンに頼らず、デバイス初期化・シェーダー・行列変換・テクスチャ・ライティングなどの
グラフィックスの基盤を自前で実装することで、GPUレンダリングパイプラインを理解すること目的としています。

![Demo movie](docs/demo.gif)

## 🛠 技術スタック (Tech Stack)

| 項目 | 内容 |
|---|---|
| **Graphics API** | DirectX 11 (Direct3D 11, DXGI) |
| **Shader** | HLSL (Shader Model 5.0) |
| **Language** | C++17 |
| **Math** | DirectXMath |
| **Texture** | WICTextureLoader (DirectXTK) |
| **Build** | Visual Studio 2022 |
| **Platform** | Windows |

## 🎮 デモ概要 (Demo Overview)

- **カメラ操作:** WASD で移動、マウスで視点操作（左クリックでキャプチャ、ESC で解除）
- **オブジェクト配置:** 右クリックで床の上にキューブを配置
- **スケール変更:** ↑↓キーで配置するキューブのサイズを変更
- **ライティング:** フォンシェーディングによるリアルタイム照明

## 🚀 技術的なこだわり (Technical Highlights)

### 1. レンダリングパイプラインの手動構築 (Manual Pipeline Setup)

ゲームエンジンが内部で行っている処理を、Win32 API と Direct3D 11 API を直接操作して構築しています。

- `D3D11CreateDeviceAndSwapChain` によるデバイス・スワップチェインの作成
- レンダーターゲットビュー・深度ステンシルビューの生成と管理
- ビューポート設定、バックバッファの Present によるダブルバッファリング

### 2. HLSL によるフォンシェーディング (Phong Shading in HLSL)

頂点シェーダーとピクセルシェーダーを HLSL で記述し、アンビエント・ディフューズ・スペキュラの3成分によるフォンシェーディングを実装しました。

```hlsl
// shaders.hlsl — ブリン・フォンモデルによるスペキュラ計算
float3 V = normalize(cameraPosition - input.worldPos);
float3 H = normalize(L + V);
float spec = pow(max(dot(N, H), 0.0f), shininess);
float3 specular = lightColor * spec * 0.5f;

float3 finalColor = ambient + diffuse + specular;
```

- 法線のワールド空間変換、ラスタライザー補間後の再正規化を実施
- ライト方向・色・光沢度をピクセルシェーダーの定数バッファ（`register(b1)`）でCPUからGPUへ毎フレーム伝達

### 3. WVP 行列変換と定数バッファ (Matrix Transform & Constant Buffers)

ローカル空間 → ワールド空間 → ビュー空間 → クリップ空間の座標変換を、DirectXMath の行列演算と HLSL の `mul()` で実現しています。

```cpp
// Renderer.cpp — ワールド行列を変えて同じメッシュを複数回描画
XMMATRIX cubeWorld = XMMatrixScaling(s.x, s.y, s.z)
                   * XMMatrixRotationY(angle)
                   * XMMatrixTranslation(p.x, p.y, p.z);
cb.wvp   = XMMatrixTranspose(cubeWorld * view * projection);
cb.world = XMMatrixTranspose(cubeWorld);
```

- DirectXMath（行優先）と HLSL（列優先）の差異を `XMMatrixTranspose` で吸収
- 定数バッファの 16 バイトアライメント制約に対応したパディング設計

### 4. スクリーン→ワールド レイキャスト (Screen-to-World Raycasting)

マウスクリック位置から3D空間にレイを飛ばし、床平面（Y = -0.5）との交点にキューブを配置する機能を実装しました。

```cpp
// Raycast.cpp — XMVector3Unproject でスクリーン座標をワールド空間に逆変換
XMVECTOR nWorldPos = XMVector3Unproject(nViewPos, ...);  // ニアクリップ面上の点
XMVECTOR fWorldPos = XMVector3Unproject(fViewPos, ...);  // ファークリップ面上の点
origin    = nWorldPos;
direction = XMVector3Normalize(XMVectorSubtract(fWorldPos, nWorldPos));
```

- 描画時の座標変換（ローカル → クリップ → スクリーン）の逆過程を実行
- レイと水平面のパラメトリック交差判定（`t = (-0.5 - origin.y) / direction.y`）で配置位置を算出

### 5. フリーカメラ (FPS-Style Free Camera)

ヨー・ピッチの2軸でカメラの向きを管理し、球面座標から前方向ベクトルを計算。WASD 移動はカメラの向きに追従し、マウスカーソルを毎フレーム画面中央にリセットすることで無限回転を実現しています。

```cpp
// Camera.cpp — ヨー/ピッチから前方向ベクトルを計算
forward.x = cosf(m_pitch) * sinf(m_yaw);
forward.y = -sinf(m_pitch);
forward.z = cosf(m_pitch) * cosf(m_yaw);
```

- ピッチ角を ±89° にクランプしてジンバルロックを回避
- `deltaTime` による移動量のフレームレート非依存化

### 6. テクスチャマッピング (Texture Mapping)

WICTextureLoader でテクスチャ画像を読み込み、シェーダーリソースビュー（SRV）とサンプラーステートを介してピクセルシェーダーで UV サンプリングを実行しています。

- 描画オブジェクトごとに `PSSetShaderResources` でテクスチャを切り替え
- `D3D11_TEXTURE_ADDRESS_WRAP` による床テクスチャのタイリング

## 📂 ディレクトリ構成 (Project Structure)

```
DX11Demo/
├── include/DX11Demo/
│   ├── Common.h           # 共有インクルード・定数定義
│   ├── Types.h            # Vertex, ConstantBuffer, LightBuffer 構造体
│   ├── Camera.h           # フリーカメラクラス
│   ├── Renderer.h         # D3D11 リソース管理・描画
│   ├── Cube.h             # キューブデータ構造
│   ├── CubeManager.h      # キューブの生成・管理
│   ├── Raycast.h          # スクリーン→ワールド座標変換
│   └── External/
│       └── tiny_obj_loader.h
├── src/
│   ├── main.cpp           # WinMain, WndProc, ゲームループ
│   ├── Renderer.cpp       # パイプライン初期化・描画処理
│   ├── Camera.cpp         # カメラ更新・ビュー/プロジェクション行列
│   ├── CubeManager.cpp    # キューブ配置・スケール管理
│   └── Raycast.cpp        # レイキャスト計算
├── shaders/
│   └── shaders.hlsl       # 頂点/ピクセルシェーダー（フォンシェーディング）
└── textures/              # テクスチャ画像 (PNG/JPG)
```

## 📦 ビルド方法 (Build Instructions)

### 前提条件

- Windows 10 以降
- Visual Studio 2022（C++ デスクトップ開発ワークロード）
- DirectXTK NuGet パッケージ（プロジェクトに設定済み）

### 手順

```bash
# リポジトリのクローン
git clone https://github.com/niajf/DirectX11Demo.git

# Visual Studio でソリューションを開く
# DX11Demo.slnx をダブルクリック

# ビルド: Ctrl + Shift + B
# 実行:   F5
```

## 🎯 実装機能一覧 (Features)

- **D3D11 初期化:** デバイス・スワップチェイン・レンダーターゲット・深度バッファの手動構築
- **HLSL シェーダー:** 頂点シェーダー（WVP 変換）+ ピクセルシェーダー（フォンシェーディング）
- **定数バッファ:** WVP 行列用（`b0`）+ ライト情報用（`b1`）の2スロット構成
- **テクスチャマッピング:** WICTextureLoader による画像読み込み、UV サンプリング、タイリング
- **フリーカメラ:** WASD 移動 + マウス視点操作、`SetCursorPos` による無限回転
- **オブジェクト配置:** `XMVector3Unproject` によるレイキャスト、床面との交差判定
- **スケール変更:** キー入力による配置キューブのサイズ調整
- **ダブルバッファリング:** VSync 対応の `Present` によるティアリング防止
