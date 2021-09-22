#include <Windows.h>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

unsigned short a = 45;	// in game value: angle
unsigned short p = 50;	// in game value: power
unsigned short w = 0;	// in game value: wind

float g = 0;	// gravity
float i = 0;	// cos(a)
float j = 1;	// sin(a)

const int SSwid = 1350;	// game wid
const int SShgt = 760;	// game hgt

HWND SShwnd;	// game hwnd
HWND RRhwnd;	// ruler hwnd
HDC SSdc;		// game device context

bool offStandby = true;
LRESULT CALLBACK MsgCallback(HWND hwnd, UINT msg, WPARAM param, LPARAM lparam);
INPUT ip;
void simKey(int vk, bool press);

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

	WNDCLASS wc{};
	wc.hInstance = currentInstance;
	wc.lpszClassName = L"SSLRR";
	wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);;
	wc.lpfnWndProc = MsgCallback;
	if (!RegisterClass(&wc)) return 1;

	RRhwnd = CreateWindow(L"SSLRR", L"SSLRR", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 1500, 800,
		nullptr, nullptr, nullptr, nullptr);
	if (!RRhwnd) return 1;
	MoveWindow(RRhwnd, 0, 0, SSwid + 120, SShgt + 50, true);

	MSG msg{};
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

//            a    d    w    s
int vks[] = { 0x41,0x44,0x57,0x53 };
bool isDown[] = { 0, 0, 0, 0 };
bool wasUp[] = { 1, 1, 1, 1 };
bool keyActive = false;

bool processKeys() {
	bool anyKey = false;
	for (int i = 0; i < 4; ++i) {
		isDown[i] = GetAsyncKeyState(vks[i]) & 0x8000;
		if (isDown[i]) anyKey = true;
	}

	if (anyKey && !keyActive) {
		SetForegroundWindow(SShwnd);
		keyActive = true;
	}

	for (int i = 0; i < 4; ++i) {
		if (isDown[i] && wasUp[i]) {
			simKey(vks[i], true);
			wasUp[i] = false;
		}
		else if (!isDown[i] && !wasUp[i]) {
			simKey(vks[i], false);
			wasUp[i] = true;
		}
	}

	if (!anyKey && keyActive) {
		keyActive = false;
		offStandby = true; //update screen asap
		SetForegroundWindow(RRhwnd);
	}

	return anyKey;
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
		if (!processKeys()) {
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

		BitBlt(hdc, 0, 0, SSwid, SShgt, SSdc, 0, 0, SRCCOPY);

		EndPaint(hwnd, &ps);
		return 0;

	case WM_LBUTTONDOWN:
		SendMessage(SShwnd, WM_LBUTTONDOWN, param, lparam);
		return 0;

	case WM_LBUTTONUP:
		SendMessage(SShwnd, WM_LBUTTONUP, param, lparam);
		return 0;

	case WM_KEYDOWN:
		/*INPUT ip;
		ip.type = INPUT_KEYBOARD;
		ip.ki.wScan = 0; // hardware scan code for key
		ip.ki.time = 0;
		ip.ki.dwExtraInfo = 0;
		ip.ki.wVk = 0x41;
		ip.ki.dwFlags = 0; // 0 for key press*/
		//SetForegroundWindow(SShwnd);
		//SendInput(1, &ip, sizeof(INPUT));
		PostMessage(SShwnd, WM_KEYDOWN, param, lparam);
		return 0;

	case WM_KEYUP:
		//PostMessage(SShwnd, WM_KEYUP, param, lparam);
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