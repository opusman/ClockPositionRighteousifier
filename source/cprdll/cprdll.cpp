/** Make the position of the taskbar clock in the Windows 10 Anniversary Update righteous again! **/
/** by Jonathan Potter, no guarantees or warranties **/

#include "stdafx.h"

#pragma data_seg(".shared")
UINT g_uiHookMsg{}, g_uiUnhookMsg{};
#pragma data_seg()
#pragma comment(linker, "/section:.shared,rws")

HINSTANCE g_hInstanceDll{};
HWND g_hwndClock{}, g_hwndNotify{};
POINT g_ptClockPos{}, g_ptNotifyPos{};

enum { IdClock, IdNotify };

LRESULT CALLBACK SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	if (uMsg == WM_WINDOWPOSCHANGING)
	{
		LPWINDOWPOS pwp = reinterpret_cast<LPWINDOWPOS>(lParam);
		if (pwp && !(pwp->flags & SWP_NOMOVE))
		{
			RECT rc;
			if (GetWindowRect(hWnd, &rc))
			{
				// get screen-relative coords to compare
				POINT pt{pwp->x, pwp->y};
				ClientToScreen(GetParent(hWnd), &pt);

				if ((uIdSubclass == IdClock && (pt.x != g_ptClockPos.x || pt.y != g_ptClockPos.y))
				||	(uIdSubclass == IdNotify && (pt.x != g_ptNotifyPos.x || pt.y != g_ptNotifyPos.y)))
				{
					// stop clock moving to the left or notify to the right
					pt = (uIdSubclass == IdClock) ? g_ptClockPos : g_ptNotifyPos;
					ScreenToClient(GetParent(hWnd), &pt);
					pwp->x = pt.x;
					pwp->y = pt.y;
				}
			}
		}
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void InstallSubClass(HWND hwndTaskbar)
{
	// increment refcount to keep DLL in explorer
	if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCTSTR>(InstallSubClass), &g_hInstanceDll))
		return;
	if (HWND hwndTray = FindWindowEx(hwndTaskbar, nullptr, L"TrayNotifyWnd", nullptr))
	{
		if (HWND hwndClock = FindWindowEx(hwndTray, nullptr, L"TrayClockWClass", nullptr))
		{
			if (HWND hwndActionCentre = FindWindowEx(hwndTray, nullptr, L"TrayButton", nullptr))
			{
				g_hwndClock = hwndClock;
				g_hwndNotify = hwndActionCentre;

				RECT rc;
				GetWindowRect(g_hwndClock, &rc);
				g_ptClockPos = POINT{rc.left, rc.top};
				GetWindowRect(g_hwndNotify, &rc);
				g_ptNotifyPos = POINT{rc.left, rc.top};

				SetWindowSubclass(g_hwndClock, SubclassProc, IdClock, 0);
				SetWindowSubclass(g_hwndNotify, SubclassProc, IdNotify, 0);
			}
		}
	}
}

void RemoveSubClass(HWND hwndTaskbar)
{
	if (g_hwndClock)
	{
		RemoveWindowSubclass(g_hwndClock, SubclassProc, IdClock);
		g_hwndClock = nullptr;
	}
	if (g_hwndNotify)
	{
		RemoveWindowSubclass(g_hwndNotify, SubclassProc, IdNotify);
		g_hwndNotify = nullptr;
	}
	if (g_hInstanceDll)
	{
		HINSTANCE hMod = g_hInstanceDll;
		g_hInstanceDll = nullptr;
		FreeLibrary(hMod);
	}
}


extern "C" {

__declspec(dllexport) void HookInit(UINT uiHookMsg, UINT uiUnhookMsg)
{
	g_uiHookMsg = uiHookMsg;
	g_uiUnhookMsg = uiUnhookMsg;
}

__declspec(dllexport) LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		CWPSTRUCT* pcwp = reinterpret_cast<CWPSTRUCT*>(lParam);
		if (pcwp)
		{
			if (pcwp->message == g_uiHookMsg)
				InstallSubClass(pcwp->hwnd);
			else
			if (pcwp->message == g_uiUnhookMsg)
				RemoveSubClass(pcwp->hwnd);
		}
	}
	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

};
