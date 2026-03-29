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

// ウィンドウのサイズ（後でDirectXのバックバッファサイズと合わせる）
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

// ウィンドウプロシージャの前方宣言
// OSからのメッセージ（キー入力、マウス、閉じるボタン等）を処理するコールバック関数
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

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

            // Render（今は空）
            // TODO: DirectXによる描画（Day 3〜4で実装）

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
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hwnd);
        }
        return 0;

    // ウィンドウが破棄された → アプリケーション終了をOSに通知
    case WM_DESTROY:
        PostQuitMessage(0); // メッセージキューにWM_QUITを投入 → GetMessage()が0を返す
        return 0;
    }

    // 自分で処理しないメッセージはOSのデフォルト処理に委譲
    return DefWindowProc(hwnd, msg, wParam, lParam);
}