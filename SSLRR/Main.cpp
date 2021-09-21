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

LRESULT CALLBACK MsgCallback(HWND hwnd, UINT msg, WPARAM param, LPARAM lparam);

int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, PSTR cmdLine, INT cmdCount) {
	cout << "sdfsdf";

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

LRESULT CALLBACK MsgCallback(HWND hwnd, UINT msg, WPARAM param, LPARAM lparam) {
	switch (msg) {
	case WM_CREATE:
		SetTimer(hwnd, 1, 30, NULL); // 30 ms
		return 0;

	case WM_DESTROY:
		KillTimer(hwnd, 1);
		PostQuitMessage(0);
		return 0;

	case WM_TIMER:
		InvalidateRect(hwnd, nullptr, false);
		return 0;

	case WM_PAINT:
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		
		BitBlt(hdc, 0, 0, SSwid, SShgt, SSdc, 0, 0, SRCCOPY);

		EndPaint(hwnd, &ps);
		return 0;
	}

	return DefWindowProc(hwnd, msg, param, lparam);
}