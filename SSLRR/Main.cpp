//https://www.desmos.com/calculator/tpzmmpusro

#include <Windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;
constexpr float PI = 3.141592654f;
constexpr float DEG2RAD = PI/180.0F;
constexpr float RAD2DEG = 180.0F/PI;

int a = 45;	// in game value: angle
unsigned short p = 30;	// in game value: power
short w = 0;	// in game value: wind

constexpr float r = 15;	// tank radius
constexpr float aimCircle = 578 / 2.5; //aim circle radius
constexpr float g = -1;	// gravity
float i = 0;	// cos(a)
float j = 1;	// sin(a)

constexpr int maxTrajPoints = 100;
D2D1_POINT_2F trajPoints[maxTrajPoints];
int trajIndex = 0;

POINT curPos;	// mouse position
POINT curAim;	// cursor for aiming
bool aiming = false;	// if click+drag aiming
POINT tankOrigin = { 100,100 };	// center of tank
bool centering = false;	// if centering on tank

constexpr int SSwid = 1350;	// game wid
constexpr int SShgt = 760;	// game hgt
RECT SSrc; // game client size
RECT RRrc; // ruler client size
constexpr int rightPadding = 120;	// right strip wid
constexpr int bottomPadding = 50;	// bottom strip hgt

HWND SShwnd;	// game hwnd
HWND RRhwnd;	// ruler hwnd
HDC SSdc;		// game device context

ID2D1DCRenderTarget* d2RenderTarget;
ID2D1StrokeStyle* dashedStroke;
ID2D1SolidColorBrush* stripBrush;
ID2D1SolidColorBrush* redBrush;
ID2D1SolidColorBrush* greenBrush;
ID2D1SolidColorBrush* yellowBrush;
ID2D1SolidColorBrush* purpleBrush;
ID2D1SolidColorBrush* blueBrush;
ID2D1SolidColorBrush* pinkBrush;

bool offStandby = true;
LRESULT CALLBACK MsgCallback(HWND hwnd, UINT msg, WPARAM param, LPARAM lparam);
INPUT ip;
void simKey(int vk, bool press);
void setupD2D();

int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, PSTR cmdLine, INT cmdCount) {

	ip.type = INPUT_KEYBOARD;
	ip.ki.wScan = 0;
	ip.ki.time = 0;
	ip.ki.dwExtraInfo = 0;

	SShwnd = FindWindow(nullptr, L"ShellShock Live");
	if (!SShwnd) {
		cout << "Couldn't find game window." << endl;
		return 1;
	}

	ShowWindow(SShwnd, SW_RESTORE);
	MoveWindow(SShwnd, 0, 0, SSwid, SShgt, false);

	SSdc = GetDC(SShwnd);
	GetClientRect(SShwnd, &SSrc);

	WNDCLASS wc{};
	wc.hInstance = currentInstance;
	wc.lpszClassName = L"SSLRR";
	wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
	//wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpfnWndProc = MsgCallback;
	if (!RegisterClass(&wc)) return 1;

	RRhwnd = CreateWindow(L"SSLRR", L"SSLRR", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 1500, 800,
		nullptr, nullptr, nullptr, nullptr);
	if (!RRhwnd) return 1;
	MoveWindow(RRhwnd, 0, 0, SSwid + rightPadding, SShgt + bottomPadding, true);
	GetClientRect(RRhwnd, &RRrc);

	setupD2D();

	MSG msg{};
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

void render(HDC &hdc, PAINTSTRUCT &ps) {
	float angle = round(atan2(curAim.y - tankOrigin.y, curAim.x - tankOrigin.x)*RAD2DEG);

	a = angle + 90;
	i = cos(a * DEG2RAD);
	j = sin(a * DEG2RAD);
	int t = 0;
	trajIndex = 0;
	while (trajIndex < maxTrajPoints) {
		trajPoints[trajIndex].x = tankOrigin.x + j * r + j * p * t + w * t * t;
		trajPoints[trajIndex].y = tankOrigin.y - i * r - i * p * t - g * t * t;
		t += 1;
		trajIndex += 1;
	}

	// GDI POINT to d2 point
	D2D1_POINT_2F curAimPT = { curAim.x,curAim.y };
	D2D1_POINT_2F tankOriginPT = { tankOrigin.x,tankOrigin.y };
	D2D1_POINT_2F gameAimPT = { tankOrigin.x + aimCircle * cos(angle * DEG2RAD) ,tankOrigin.y + aimCircle * sin(angle * DEG2RAD) };
	D2D1_RECT_F bottomStrip = D2D1::RectF(0, ps.rcPaint.bottom - bottomPadding, ps.rcPaint.right, ps.rcPaint.bottom);
	D2D1_RECT_F rightStrip = D2D1::RectF(ps.rcPaint.right - rightPadding, 0, ps.rcPaint.right, ps.rcPaint.bottom - bottomPadding);
	D2D1_ELLIPSE tankRadius = { tankOriginPT,r,r };
	D2D1_ELLIPSE aimRadius = { tankOriginPT,aimCircle,aimCircle };
	D2D1_ELLIPSE cursorAim = { curAimPT,3,3 };

	d2RenderTarget->BindDC(hdc, &RRrc);
	d2RenderTarget->BeginDraw();

	// bottom and right strips
	d2RenderTarget->FillRectangle(&bottomStrip, stripBrush);
	d2RenderTarget->FillRectangle(&rightStrip, stripBrush);

	for (int i = 0; i < maxTrajPoints-1; ++i) {
		d2RenderTarget->DrawLine(trajPoints[i], trajPoints[i + 1], redBrush);
	}

	// tank barrel radius, aim radius
	d2RenderTarget->DrawEllipse(tankRadius, greenBrush);
	d2RenderTarget->DrawEllipse(aimRadius, yellowBrush);

	// aim line and point and arc
	d2RenderTarget->FillEllipse(cursorAim, purpleBrush);
	d2RenderTarget->DrawLine(tankOriginPT, curAimPT, blueBrush,1.0f,dashedStroke);
	d2RenderTarget->DrawLine(tankOriginPT, gameAimPT, pinkBrush);
	//d2RenderTarget->DrawGeometry()

	//D2D1_RECT_F testcur = D2D1::RectF(curAim.x - 10, curAim.y - 10, curAim.x + 10, curAim.y + 10);
	//D2D1_RECT_F testcur2 = D2D1::RectF(curPos.x - 10, curPos.y - 10, curPos.x + 10, curPos.y + 10);
	//d2RenderTarget->FillRectangle(&testcur, redBrush);
	//d2RenderTarget->FillRectangle(&testcur2, redBrush);

	d2RenderTarget->EndDraw();
}

//            a    d    w    s
const int keyCnt = 62;
const int vks[keyCnt] = { VK_BACK, VK_RETURN, VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_ESCAPE,
							VK_SPACE, VK_END, VK_HOME, VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN, VK_DELETE,
							0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, // 1-9
							0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, // A-M
							0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, // N-Z
							VK_OEM_1, VK_OEM_PLUS, VK_OEM_COMMA, VK_OEM_MINUS, VK_OEM_PERIOD,
							VK_OEM_2, VK_OEM_4, VK_OEM_5, VK_OEM_6, VK_OEM_7 };
bool isDown[keyCnt] = { 0 };
bool isSim[keyCnt] = { 0 };
bool keyIsDown = false;
bool keyWasUp = true;
char debounce;
bool processKeys() {
	keyIsDown = false;

	for (int i = 0; i < keyCnt; ++i) {
		isDown[i] = GetAsyncKeyState(vks[i]) & 0x8000;
		if (isDown[i]) keyIsDown = true;
		else if (isSim[i]) {
			isSim[i] = false;
			simKey(vks[i], false); // release sim (for edge cases)
		}
	}

	if (keyIsDown) {
		if (keyWasUp) { // first keys
			if (GetForegroundWindow() != RRhwnd) return true;
			SetForegroundWindow(SShwnd);
			keyWasUp = false;
			for (int i = 0; i < keyCnt; ++i) {
				if (isDown[i]) {
					isSim[i] = true;
					simKey(vks[i], true);
				}
			}
		}
		debounce = 3; // reset debounce
		return true;
	}
	else {
		if (debounce > 0) { // dont switch yet
			debounce -= 1;
			return true;
		}
		else if (!keyWasUp) { // switch back
			keyWasUp = true;
			offStandby = true; //update screen asap
			SetForegroundWindow(RRhwnd);
			return false;
		}
		return false;
	}
}

LRESULT CALLBACK MsgCallback(HWND hwnd, UINT msg, WPARAM param, LPARAM lparam) {
	switch (msg) {
	case WM_CREATE:
		SetTimer(hwnd, 1, 16, NULL); // 16 ms
		return 0;

	case WM_DESTROY:
		KillTimer(hwnd, 1);
		PostQuitMessage(0);
		return 0;

	case WM_TIMER:
		if (!processKeys()) { // if ruler window shown
			if (centering) {
				GetCursorPos(&tankOrigin);
				ScreenToClient(hwnd, &tankOrigin);
				offStandby = true; // high update rate while centering
			}

			else if (aiming) {
				GetCursorPos(&curAim);
				ScreenToClient(hwnd, &curAim);
				offStandby = true; // high update rate while aiming
			}

			//render
			if (offStandby) { // update screen every other time
				InvalidateRect(hwnd, nullptr, false);
				offStandby = false;
			}
			else offStandby = true;
		}
		return 0;

	case WM_PAINT:
		HDC hdc;
		PAINTSTRUCT ps;
		hdc = BeginPaint(hwnd, &ps);

		BitBlt(hdc, 0, 0, SSrc.right - SSrc.left, SSrc.bottom - SSrc.top, SSdc, 0, 0, SRCCOPY);
		render(hdc, ps);

		EndPaint(hwnd, &ps);

		return 0;

	case WM_LBUTTONDOWN:
		SendMessage(SShwnd, WM_LBUTTONDOWN, param, lparam);
		aiming = true;
		return 0;

	case WM_LBUTTONUP:
		SendMessage(SShwnd, WM_LBUTTONUP, param, lparam);
		aiming = false;
		return 0;

	case WM_KEYDOWN:
		if (param == VK_TAB) centering = true;
		return 0;

	case WM_KEYUP:
		if (param == VK_TAB) centering = false;
		return 0;

	default:
		return DefWindowProc(hwnd, msg, param, lparam);
	}
}

void simKey(int vk, bool press) {
	if (press) {
		ip.ki.wVk = vk;
		ip.ki.dwFlags = 0;
		SendInput(1, &ip, sizeof(INPUT));
	}
	else {
		ip.ki.wVk = vk;
		ip.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &ip, sizeof(INPUT));
	}
}


float dashes[2] = { r,r };
void setupD2D() {
	ID2D1Factory* d2Factory;
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2Factory);

	auto props = D2D1::RenderTargetProperties();
	props.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);

	d2Factory->CreateDCRenderTarget(&props, &d2RenderTarget);

	d2Factory->CreateStrokeStyle(D2D1::StrokeStyleProperties(
		D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_FLAT,
		D2D1_LINE_JOIN_MITER,10.0F, D2D1_DASH_STYLE_CUSTOM,0.0f), dashes, 2, &dashedStroke);

	d2RenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::DarkGray),
		&stripBrush
	);
	d2RenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::IndianRed),
		&redBrush
	);
	d2RenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::MediumPurple),
		&purpleBrush
	);
	d2RenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::ForestGreen),
		&greenBrush
	);
	d2RenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::Yellow),
		&yellowBrush
	);
	yellowBrush->SetOpacity(0.5);
	d2RenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::DodgerBlue),
		&blueBrush
	);
	d2RenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::DeepPink),
		&pinkBrush
	);

	d2Factory->Release();
}