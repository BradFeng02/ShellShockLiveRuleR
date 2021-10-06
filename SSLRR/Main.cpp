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

short a = 45;	// rounded value: angle (is -90 to 269 clockwise from left)
unsigned short p = 50;	// rounded value: power
short w = 0;	// wind

constexpr float r = 13.5;	// tank radius
constexpr float aimCircle = 578 / 2.5; //aim circle radius // 578
float g = -2.8783;	// gravity
float i = 0;	// cos(a)
float j = 1;	// sin(a)

constexpr int maxTrajPoints = 75;
int usedTrajCnt = 0;
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
constexpr int rightPadding = 160;	// right strip wid
constexpr int bottomPadding = 50;	// bottom strip hgt
constexpr int strSize = 18;
WCHAR dispStr[strSize];

HWND SShwnd;	// game hwnd
HWND RRhwnd;	// ruler hwnd
HDC SSdc;		// game device context
HDC CPdc;			// clone of SSdc
HBITMAP CPbmp;		// bitmap of game
void* CPbmpPixels;	// pointer to pixels
cv::Mat CPmat;		// mat of game

ID2D1DCRenderTarget* d2RenderTarget;
IDWriteTextFormat* textFormat;
ID2D1StrokeStyle* dashedStroke;
ID2D1SolidColorBrush* stripBrush;
ID2D1SolidColorBrush* whiteBrush;
ID2D1SolidColorBrush* redBrush;
ID2D1SolidColorBrush* greenBrush;
ID2D1SolidColorBrush* yellowBrush;
ID2D1SolidColorBrush* purpleBrush;
ID2D1SolidColorBrush* blueBrush;
ID2D1SolidColorBrush* pinkBrush;

bool offStandby = true;
LRESULT CALLBACK MsgCallback(HWND hwnd, UINT msg, WPARAM param, LPARAM lparam);
INPUT ip; // simulate keyboard input
INPUT mMove; // sim mouse input
void simKey(int vk, bool press);
void setupD2D();

int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, PSTR cmdLine, INT cmdCount) {
	ip.type = INPUT_KEYBOARD;
	ip.ki.wScan = 0;
	ip.ki.time = 0;
	ip.ki.dwExtraInfo = 0;

	mMove.type = INPUT_MOUSE;
	mMove.mi.mouseData = 0;
	mMove.mi.time = 0;
	mMove.mi.dwExtraInfo = 0;

	SShwnd = FindWindow(nullptr, L"ShellShock Live");
	if (!SShwnd) {
		cout << "Couldn't find game window." << endl;
		return 1;
	}

	ShowWindow(SShwnd, SW_RESTORE);
	MoveWindow(SShwnd, 0, 0, SSwid, SShgt, false);

	SSdc = GetDC(SShwnd);
	GetClientRect(SShwnd, &SSrc);
	CPdc = CreateCompatibleDC(SSdc);

	// for ssdc to opencv mat
	BITMAPINFO bi;
	ZeroMemory(&bi, sizeof(BITMAPINFO));
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = SSrc.right - SSrc.left;
	bi.bmiHeader.biHeight = -(SSrc.bottom - SSrc.top);  //neg indicating a top-down DIB, so (0,0) is at top left
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	CPbmp = CreateDIBSection(SSdc, &bi, DIB_RGB_COLORS, &CPbmpPixels, NULL, 0);
	SelectObject(CPdc, CPbmp);
	CPmat = Mat(SSrc.bottom - SSrc.top, SSrc.right - SSrc.left, CV_8UC4, CPbmpPixels, 0);

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
	float aimDist = sqrtf(pow(curAim.y - tankOrigin.y, 2) + pow(curAim.x - tankOrigin.x, 2));
	float angle = roundf(atan2f(curAim.y - tankOrigin.y, curAim.x - tankOrigin.x) * RAD2DEG);

	if (aiming) {	
		p = min(roundf(aimDist * 100.0f / aimCircle), 100.0f);
		a = angle + 90;
	}
	else {
		angle = a - 90;
	}
	
	i = cosf(a * DEG2RAD);
	j = sinf(a * DEG2RAD);
	float t = 0;
	trajIndex = 0;
	D2D1_POINT_2F *b, *c;
	while (trajIndex < maxTrajPoints) {
		// calc next pos
		float ax = tankOrigin.x + j * r + j * (p + ((g > 0) ? 1 : 0)) * t + w * t * t; // 1 less power
		float ay = tankOrigin.y - i * r - i * (p + ((g > 0) ? 1 : 0)) * t - g * t * t; // for underground

		// simplify
		if (trajIndex > 1) {
			b = &(trajPoints[trajIndex - 1]);
			c = &(trajPoints[trajIndex - 2]);
			float ang1 = atan2f(ay - b->y, ax - b->x);
			float ang2 = atan2f(b->y - c->y, b->x - c->x);
			if (abs(ang1 - ang2) < .01f) { // combine if less than .01 rad diff.
				trajIndex -= 1;
			}
		}

		trajPoints[trajIndex].x = ax;
		trajPoints[trajIndex].y = ay;

		// enough drawn
		if (ax < 0 || ax > ps.rcPaint.right || ay < -2000 || ay > 2000) {
			usedTrajCnt = trajIndex;
			break;
		}

		t += 0.5;
		trajIndex += 1;
	}

	// GDI POINT to d2 point
	D2D1_POINT_2F curAimPT = { curAim.x,curAim.y };
	D2D1_POINT_2F tankOriginPT = { tankOrigin.x,tankOrigin.y };
	D2D1_POINT_2F gameAimPT = { tankOrigin.x + aimCircle * cosf(angle * DEG2RAD) ,tankOrigin.y + aimCircle * sinf(angle * DEG2RAD) };
	// crosshair to help alignment
	D2D1_POINT_2F xhair11 = { tankOrigin.x,tankOrigin.y - aimCircle + 15 }; // top vertical xhair
	D2D1_POINT_2F xhair12 = { tankOrigin.x,tankOrigin.y - aimCircle + 30 };
	D2D1_POINT_2F xhair21 = { tankOrigin.x + aimCircle - 15,tankOrigin.y }; // right horiz xhair
	D2D1_POINT_2F xhair22 = { tankOrigin.x + aimCircle - 30,tankOrigin.y };
	D2D1_POINT_2F xhair31 = { tankOrigin.x,tankOrigin.y + aimCircle - 15 }; // bot vertical xhair
	D2D1_POINT_2F xhair32 = { tankOrigin.x,tankOrigin.y + aimCircle - 30 };
	D2D1_POINT_2F xhair41 = { tankOrigin.x - aimCircle + 15,tankOrigin.y }; // left horiz xhair
	D2D1_POINT_2F xhair42 = { tankOrigin.x - aimCircle + 30,tankOrigin.y };
	D2D1_POINT_2F xhair51 = { tankOrigin.x - 163.5 + 10,tankOrigin.y - 163.5 + 10 }; // top left diag xhair
	D2D1_POINT_2F xhair52 = { tankOrigin.x - 163.5 + 20,tankOrigin.y - 163.5 + 20 };
	D2D1_POINT_2F xhair61 = { tankOrigin.x + 163.5 - 10,tankOrigin.y - 163.5 + 10 }; // top right diag xhair
	D2D1_POINT_2F xhair62 = { tankOrigin.x + 163.5 - 20,tankOrigin.y - 163.5 + 20 };
	D2D1_POINT_2F xhair71 = { tankOrigin.x + 163.5 - 10,tankOrigin.y + 163.5 - 10 }; // bot right diag xhair
	D2D1_POINT_2F xhair72 = { tankOrigin.x + 163.5 - 20,tankOrigin.y + 163.5 - 20 };
	D2D1_POINT_2F xhair81 = { tankOrigin.x - 163.5 + 10,tankOrigin.y + 163.5 - 10 }; // bot left diag xhair
	D2D1_POINT_2F xhair82 = { tankOrigin.x - 163.5 + 20,tankOrigin.y + 163.5 - 20 };
	D2D1_RECT_F bottomStrip = D2D1::RectF(0, ps.rcPaint.bottom - bottomPadding, ps.rcPaint.right-rightPadding, ps.rcPaint.bottom);
	D2D1_RECT_F rightStrip = D2D1::RectF(ps.rcPaint.right - rightPadding, 0, ps.rcPaint.right, ps.rcPaint.bottom);
	D2D1_ELLIPSE tankRadius = { tankOriginPT,r,r };
	D2D1_ELLIPSE aimRadius = { tankOriginPT,aimCircle,aimCircle };
	D2D1_ELLIPSE cursorAim = { curAimPT,7,7 };

	short dispAngle;
	if (a <= 0) dispAngle = a + 90;
	else if (a > 0 && a <= 180) dispAngle = 90 - a;
	else dispAngle = a - 270;
	swprintf(dispStr, strSize, L"P: %-3d     A: %-3d", p, dispAngle);

	d2RenderTarget->BindDC(hdc, &RRrc);
	d2RenderTarget->BeginDraw();

	// bottom and right strips
	d2RenderTarget->FillRectangle(&bottomStrip, stripBrush);
	d2RenderTarget->FillRectangle(&rightStrip, stripBrush);
	d2RenderTarget->DrawText(dispStr, strSize, textFormat, bottomStrip, whiteBrush);
	if(centering) d2RenderTarget->DrawText(L"` to center\n\n\n\\| to auto-find center\n\narrows to adjust\n\n________\n\TAB to toggle", 78, textFormat, rightStrip, whiteBrush);
	else d2RenderTarget->DrawText(L"` to invert gravity\n\n\\| to auto-aim\n\n\narrows to aim\n\n________\n\TAB to toggle", 75, textFormat, rightStrip, whiteBrush);

	for (int i = 0; i < usedTrajCnt; ++i) {
		// D2D1_ELLIPSE debugDot = { trajPoints[i],1,1 };
		// d2RenderTarget->DrawEllipse(debugDot, stripBrush);
		d2RenderTarget->DrawLine(trajPoints[i], trajPoints[i + 1], redBrush);
	}

	// tank barrel radius, aim radius, crosshair
	d2RenderTarget->DrawEllipse(tankRadius, greenBrush);
	d2RenderTarget->DrawEllipse(aimRadius, yellowBrush);
	d2RenderTarget->DrawLine(xhair11, xhair12, yellowBrush);
	d2RenderTarget->DrawLine(xhair21, xhair22, yellowBrush);
	d2RenderTarget->DrawLine(xhair31, xhair32, yellowBrush);
	d2RenderTarget->DrawLine(xhair41, xhair42, yellowBrush);
	d2RenderTarget->DrawLine(xhair51, xhair52, yellowBrush);
	d2RenderTarget->DrawLine(xhair61, xhair62, yellowBrush);
	d2RenderTarget->DrawLine(xhair71, xhair72, yellowBrush);
	d2RenderTarget->DrawLine(xhair81, xhair82, yellowBrush);

	// aim lines and point
	d2RenderTarget->DrawLine(tankOriginPT, gameAimPT, pinkBrush);
	if (aiming) {
		d2RenderTarget->DrawLine(tankOriginPT, curAimPT, blueBrush, 1.0f, dashedStroke);
		d2RenderTarget->FillEllipse(cursorAim, purpleBrush);
	}

	//D2D1_RECT_F testcur = D2D1::RectF(curAim.x - 10, curAim.y - 10, curAim.x + 10, curAim.y + 10);
	//D2D1_RECT_F testcur2 = D2D1::RectF(curPos.x - 10, curPos.y - 10, curPos.x + 10, curPos.y + 10);
	//d2RenderTarget->FillRectangle(&testcur, redBrush);
	//d2RenderTarget->FillRectangle(&testcur2, redBrush);

	d2RenderTarget->EndDraw();
}

//            a    d    w    s
const int keyCnt = 57; //62
const int vks[keyCnt] = { VK_BACK, VK_RETURN, VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_ESCAPE,
							VK_SPACE, VK_END, VK_HOME, /*VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN,*/ VK_DELETE,
							0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, // 1-9
							0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, // A-M
							0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, // N-Z
							VK_OEM_1, VK_OEM_PLUS, VK_OEM_COMMA, VK_OEM_MINUS, VK_OEM_PERIOD,
							VK_OEM_2, VK_OEM_4, /*VK_OEM_5,*/ VK_OEM_6, VK_OEM_7};
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

void autoAim() {
	RECT screenRC;
	GetWindowRect(GetDesktopWindow(), &screenRC);
	float transx = 65535 / (float)screenRC.right;
	float transy = 65535 / (float)screenRC.bottom;
	POINT toClick = { tankOrigin.x ,tankOrigin.y };
	ClientToScreen(RRhwnd, &toClick);
	float clickDist = aimCircle * 2.0f;
	float slope = tan((a - 90) * DEG2RAD);
	// check collision with screene edges when moving to angle
	long edgex, edgey;
	if (a <= 90) edgey = screenRC.top; // top
	else  edgey = screenRC.bottom; // bot
	if (a >= 0 && a < 180)  edgex = screenRC.right;  // right
	else  edgex = screenRC.left; //left
	
	long dy = edgey - toClick.y;
	long dx = edgex - toClick.x;
	clickDist = min(clickDist, sqrtf(powf(dy / slope, 2) + powf(dy, 2)));
	clickDist = min(clickDist, sqrtf(powf(dx * slope, 2) + powf(dx, 2)));

	// begin inputs
	SetForegroundWindow(SShwnd);

	this_thread::sleep_for(chrono::milliseconds(25));

	// move mouse to origin
	mMove.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
	mMove.mi.dx = (float)toClick.x * transx;
	mMove.mi.dy = (float)toClick.y * transy;
	SendInput(1, &mMove, sizeof(INPUT));

	this_thread::sleep_for(chrono::milliseconds(50));

	// click origin
	mMove.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	SendInput(1, &mMove, sizeof(INPUT));

	this_thread::sleep_for(chrono::milliseconds(50));

	// move to angle
	mMove.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
	mMove.mi.dx += clickDist * cosf((a - 90) * DEG2RAD) * transx;
	mMove.mi.dy += clickDist * sinf((a - 90) * DEG2RAD) * transy;
	SendInput(1, &mMove, sizeof(INPUT));

	this_thread::sleep_for(chrono::milliseconds(50));

	// click up
	mMove.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	SendInput(1, &mMove, sizeof(INPUT));

	// adjust power
	for (unsigned short i = 100; i > p; --i) {
		this_thread::sleep_for(chrono::milliseconds(50));
		simKey(VK_DOWN, true);
		this_thread::sleep_for(chrono::milliseconds(5));
		simKey(VK_DOWN, false);
	}

	this_thread::sleep_for(chrono::milliseconds(25));

	//switch back
	SetForegroundWindow(RRhwnd);
}

bool tabToggle = true;
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
		if (centering || !processKeys()) { // if ruler window shown
			if (centering) {
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
		centering = false;
		SendMessage(SShwnd, WM_LBUTTONDOWN, param, lparam);
		int x, y;
		y = lparam >> 16;
		x = lparam & 0x0000FFFF;
		if (pow(y - tankOrigin.y, 2) + pow(x - tankOrigin.x, 2) <= aimCircle * aimCircle) aiming = true;
		return 0;

	case WM_LBUTTONUP:
		SendMessage(SShwnd, WM_LBUTTONUP, param, lparam);
		aiming = false;
		return 0;

	case WM_RBUTTONDOWN:
		return 0;

	case WM_RBUTTONUP:
		BitBlt(CPdc, 0, 0, SSrc.right - SSrc.left, SSrc.bottom - SSrc.top, SSdc, 0, 0, SRCCOPY);
		cv::imshow("CPmat",CPmat);
		return 0;

	case WM_KEYDOWN:
		if (param == VK_TAB && tabToggle) { // no repeats
			centering = !centering;
			tabToggle = false;
		}
		else if (centering) {
			if (param == VK_LEFT) tankOrigin.x -= 1;
			else if (param == VK_RIGHT) tankOrigin.x += 1;
			else if (param == VK_UP) tankOrigin.y -= 1;
			else if (param == VK_DOWN) tankOrigin.y += 1;
			else if (param == VK_OEM_3) { // `~ key
				GetCursorPos(&tankOrigin);
				ScreenToClient(hwnd, &tankOrigin);
			}
		}
		else {
			if (param >= VK_LEFT && param <= VK_DOWN) {
				if (param == VK_LEFT) a -= 1;
				else if (param == VK_RIGHT) a += 1;
				else if (param == VK_UP) p += 1;
				else if (param == VK_DOWN) p -= 1;

				if (a < -90) a = 269;
				else if (a > 269) a = -90;
				if (p > 100) 
					p = 100;
				else if (p < 0) p = 0;
			}
			else if (param == VK_OEM_5) { // \| key
				autoAim();
			}
			else if (param == VK_OEM_3) { // `~ key
				g = -g;
			}
		}
		return 0;

	case WM_KEYUP:
		if (param == VK_TAB) tabToggle = true;
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

void setupD2D() {
	ID2D1Factory* d2Factory;
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2Factory);

	IDWriteFactory *d2WriteFactory;
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(d2WriteFactory), reinterpret_cast<IUnknown**>(&d2WriteFactory));
	d2WriteFactory->CreateTextFormat(L"Consolas", nullptr,
		DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
		30,L"",&textFormat);
	textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

	auto props = D2D1::RenderTargetProperties();
	props.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);

	d2Factory->CreateDCRenderTarget(&props, &d2RenderTarget);

	d2Factory->CreateStrokeStyle(D2D1::StrokeStyleProperties(
		D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_FLAT,
		D2D1_LINE_JOIN_MITER,10.0F, D2D1_DASH_STYLE_CUSTOM,0.0f), &r, 1, &dashedStroke);

	d2RenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::DimGray),
		&stripBrush
	);
	d2RenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::White),
		&whiteBrush
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
	d2WriteFactory->Release();
}