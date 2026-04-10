// ============================================================
// shaders.hlsl — 定数バッファ追加版
// ============================================================

// 定数バッファの定義
// register(b0) は「定数バッファスロット0番」を意味する。
// C++側の VSSetConstantBuffers(0, ...) の「0」と対応する。
cbuffer ConstantBuffer : register(b0)
{
    matrix wvp;		// 4x4行列(HLSLのmatrix型 = flaot4x4)
    matrix world;	// ワールド行列
};

// ライト用鄭州バッファの定義
cbuffer LightBuffer : register(b1)
{
    float3 lightDirection;
    float padding1;
    float3 lightColor;
    float padding2;
    float3 cameraPosition;
    float shininess;
};

// テクスチャとサンプラー
Texture2D diffuseTexture : register(t0); // テクスチャスロット0番
SamplerState linearSampler : register(s0); // サンプラースロット0番

// ---- 頂点シェーダーの入力構造体 ----
// C++側の頂点構造体と対応する。
// セマンティクス（: POSITION, : COLOR）は入力レイアウトと一致させる必要がある。
struct VSInput
{
	float3 position : POSITION; // 頂点の位置(x, y, x)
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

// ---- 頂点シェーダーの出力 / ピクセルシェーダーの入力 ----
// 頂点シェーダーの出力は、ラスタライザーで補間された後、
// ピクセルシェーダーの入力として渡される。
struct PSInput
{
    float4 position : SV_POSITION; // 変換後の頂点位置(SV = System Value, 必須)
    float3 worldNormal : NORMAL; // ワールド空間の法線
    float3 worldPos : TEXCOORD1; // ワールド空間の位置
    float2 texCoord : TEXCOORD0;
};

// ============================================================
// 頂点シェーダー (vs_main)
// ============================================================
// 各頂点に対して1回実行される。
// 頂点シェーダー — 行列変換を追加
//
// SV_POSITION はシステムセマンティクスで、「この値は画面上の位置です」を意味する。
// ラスタライザーがこの値を使って三角形をピクセルに分解する。
PSInput vs_main(VSInput input)
{
	PSInput output;
	
	// mul() は行列の掛け算。
    // ローカル座標にWVP行列を掛けて、クリップ空間の座標に変換する。
    // これにより3D空間上の位置が画面上の位置に変換される。
    output.position = mul(float4(input.position, 1.0f), wvp);
    output.worldNormal = mul(input.normal, (float3x3) world);      //法線をワールド空間に変換
    output.worldPos = mul(float4(input.position, 1.0f), world).xyz;
    output.texCoord = input.texCoord;                               // テクスチャ座標はそのまま出力
	return output;
}

// ============================================================
// ピクセルシェーダー (ps_main)
// ============================================================
// 三角形内部の各ピクセルに対して1回実行される。
// 入力の color は、3頂点の色がラスタライザーによって補間された値。
//
// SV_TARGET は「この値をレンダーターゲットに書き込め」を意味する。
float4 ps_main(PSInput input) : SV_TARGET
{   
    // テクスチャからピクセルの色を取得する
    // Sample() はUV座標を受け取り、テクスチャ上の対応するピクセルの色を返す
    float3 texColor = diffuseTexture.Sample(linearSampler, input.texCoord).rgb;
    
    // 法線を正規化(ラスタライザーの補間で長さが1でなはくなるため)
    float3 N = normalize(input.worldNormal);
    
    // ライトの方向を正規化(ライトの来る方向の逆)
    float3 L = normalize(-lightDirection);
    
    // ---- アンビエント ----
    float3 ambient = texColor * 0.15f;
    
    // ---- ディフューズ ----
    float diff = max(dot(N, L), 0.0f); // 法線とライトの方向の内積
    float3 diffuse = texColor * lightColor * diff;
    
    // ---- スペキュラ ----
    float3 V = normalize(cameraPosition - input.worldPos); // 視線方向
    float3 H = normalize(L + V); // ハーフベクトル
    float spec = pow(max(dot(N, H), 0.0f), shininess);
    float3 specular = lightColor * spec * 0.5f;

    float3 finalColor = ambient + diffuse + specular;
   
    return float4(finalColor, 1.0f); // アルファは1.0で不透明)
}