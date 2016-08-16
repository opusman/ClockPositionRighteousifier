#include <Windows.h>

/** Make the position of the taskbar clock in the Windows 10 Anniversary Update righteous again! **/
/** by Jonathan Potter, no guarantees or warranties **/



UINT g_uiHookMsg, g_uiUnhookMsg, g_uiTaskbarCreated;
HINSTANCE g_hDll{};
HOOKPROC g_hHookProc{};
DWORD g_dwHookThread{};
HWND g_hwndTaskbar{};

bool GetProperClientRect(HWND hWnd, HWND hWndParent, LPRECT prc)
{
	if (!GetWindowRect(hWnd,prc))
		return false;
	if (hWndParent == HWND_DESKTOP)
		return true; // Success (no-op)

	// MapWindowPoints returns 0 on failure but 0 is also a valid result (window whose client area is at 0,0), so use SetLastError/GetLastError to clarify.
	DWORD dwErrorPreserve = ::GetLastError();
	::SetLastError(ERROR_SUCCESS);
	if (::MapWindowPoints(HWND_DESKTOP, hWndParent, reinterpret_cast<LPPOINT>(prc), 2) == 0)
	{
		if (::GetLastError() != ERROR_SUCCESS)
			return false;
		::SetLastError(dwErrorPreserve);
	}

	return true; // Success
}

bool FixClockPosition(bool fRestore)
{
	if (HWND hwndTray = FindWindowEx(g_hwndTaskbar, nullptr, L"TrayNotifyWnd", nullptr))
	{
		if (HWND hwndClock = FindWindowEx(hwndTray, nullptr, L"TrayClockWClass", nullptr))
		{
			if (HWND hwndActionCentre = FindWindowEx(hwndTray, nullptr, L"TrayButton", nullptr))
			{
				RECT rcClock, rcNotify;
				if (GetProperClientRect(hwndClock, hwndTray, &rcClock)
				&&	GetProperClientRect(hwndActionCentre, hwndTray, &rcNotify))
				{
					// action centre may be off - no-op
					if (!fRestore && !IsWindowVisible(hwndActionCentre))
						return false;

					APPBARDATA abd{sizeof(APPBARDATA)};
					if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd))
					{
						if ((abd.rc.right - abd.rc.left) > (abd.rc.bottom - abd.rc.top))
						{
							// horizontal
							int w = rcNotify.right - rcNotify.left;
							if (fRestore)
							{
								if (rcNotify.left < rcClock.left)
								{
									SetWindowPos(hwndClock, nullptr, rcClock.left - w, rcClock.top, 0, 0,
										SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
									SetWindowPos(hwndActionCentre, hwndClock, rcClock.right - w, rcNotify.top, 0, 0,
										SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
								}
							}
							else
							if (rcNotify.left > rcClock.left)
							{
								SetWindowPos(hwndClock, hwndActionCentre, rcClock.left + w, rcClock.top, 0, 0,
									SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
								SetWindowPos(hwndActionCentre, nullptr, rcClock.left, rcNotify.top, 0, 0,
									SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
							}
						}
						else
						{
							// vertical
							int h = rcNotify.bottom - rcNotify.top;
							if (fRestore)
							{
								if (rcNotify.top < rcClock.top)
								{
									SetWindowPos(hwndClock, nullptr, rcClock.left, rcClock.top - h, 0, 0,
										SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
									SetWindowPos(hwndActionCentre, hwndClock, rcNotify.left, rcClock.bottom - h, 0, 0,
										SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
								}
							}
							else
							if (rcNotify.top > rcClock.top)
							{
								SetWindowPos(hwndClock, hwndActionCentre, rcClock.left, rcClock.top + h, 0, 0,
									SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
								SetWindowPos(hwndActionCentre, nullptr, rcNotify.left, rcClock.top, 0, 0,
									SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
							}
						}
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool InstallHook()
{
	if (g_hwndTaskbar = FindWindow(L"Shell_TrayWnd", nullptr))
	{
		if (!FixClockPosition(false))
			return false;
		DWORD dwThread = GetWindowThreadProcessId(g_hwndTaskbar, nullptr);
		if (HHOOK hHook = SetWindowsHookEx(WH_CALLWNDPROC, g_hHookProc, g_hDll, dwThread))
		{
			SendMessage(g_hwndTaskbar, g_uiHookMsg, 0, 0);
			UnhookWindowsHookEx(hHook);
			g_dwHookThread = dwThread;
			return true;
		}
	}
	g_dwHookThread = 0;
	return false;
}

#define IDTIMER_RESTART	1

LRESULT WINAPI WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == g_uiTaskbarCreated)
	{
		// taskbar has restarted; set a timer to reinstall the hook (this is arbitrary but 10 seconds should be long enough)
		g_hwndTaskbar = nullptr;
		g_dwHookThread = 0;
		SetTimer(hWnd, IDTIMER_RESTART, 10000, 0);
		return 0;
	}
	switch (uMsg)
	{
	case WM_CLOSE:
		DestroyWindow(hWnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_TIMER:
		if (wParam == IDTIMER_RESTART)
		{
			KillTimer(hWnd, wParam);
			if (!g_dwHookThread && !InstallHook())
				PostMessage(hWnd, WM_CLOSE, 0, 0);
		}
		return 0;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

#define CLASSNAME L"ClockPositionRighteousifier"
#define MSGHOOK	  L"ClockPositionRighteousifierHook"
#define MSGUNHOOK L"ClockPositionRighteousifierUnhook"

typedef void (*HOOKINIT)(UINT, UINT);

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	if (HWND hwndExisting = FindWindow(CLASSNAME, nullptr))
	{
		PostMessage(hwndExisting, WM_CLOSE, 0, 0);
		return 0;
	}

	WNDCLASS wc{};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = CLASSNAME;
	RegisterClass(&wc);

	if (!(g_hDll = LoadLibrary(L"cprdll.dll")))
		return 0;

	HOOKINIT hHookInit = reinterpret_cast<HOOKINIT>( GetProcAddress(g_hDll, "HookInit") );
	g_hHookProc = reinterpret_cast<HOOKPROC>( GetProcAddress(g_hDll, "HookProc") );
	if (hHookInit && g_hHookProc)
	{
		g_uiHookMsg = RegisterWindowMessage(MSGHOOK);
		g_uiUnhookMsg = RegisterWindowMessage(MSGUNHOOK);
		g_uiTaskbarCreated = RegisterWindowMessage(L"TaskbarCreated");
		hHookInit(g_uiHookMsg, g_uiUnhookMsg);

		if (HWND hwndMain = CreateWindowEx(0, CLASSNAME, nullptr, WS_POPUP, 0, 0, 0, 0, HWND_DESKTOP, nullptr, GetModuleHandle(nullptr), 0))
		{
			if (InstallHook())
			{
				MSG msg;
				while (GetMessage(&msg, nullptr, 0, 0) > 0)
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}

				if (g_dwHookThread)
				{
					if (HHOOK hHook = SetWindowsHookEx(WH_CALLWNDPROC, g_hHookProc, g_hDll, g_dwHookThread))
					{
						SendMessage(g_hwndTaskbar, g_uiUnhookMsg, 0, 0);
						UnhookWindowsHookEx(hHook);
					}
				}

				FixClockPosition(true);
			}
		}
	}
	FreeLibrary(g_hDll);
	return 0;
}

