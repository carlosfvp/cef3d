//----------------------------------------------------------------------------
// Cef3D
// Copyright (C) 2017 arkenthera
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// https://github.com/arkenthera/cef3d
// Cef3DGlobals.h
// Date: 13.04.2017
//---------------------------------------------------------------------------

#include <Cef3D.h>
#include <Cef3DPCH.h>
#include "Direct3D11Renderer.h"


#include <Windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3d9.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#pragma comment(lib, "windowscodecs.lib")

#define PROFILE 1

class Cef3DDelegate;

int GetMouseModifiers(WPARAM wparam);
int GetKeyboardModifiers(WPARAM wparam, LPARAM lparam);
bool IsKeyDown(WPARAM wparam);
void PumpMessageLoop();
LRESULT CALLBACK RootWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

Cef3D::Cef3DBrowser* OverlayBrowser = 0;
bool MouseTracking = false;

Cef3DDirect3D11Renderer Renderer;

class Cef3DDelegate
{
public:
	void OnPaint(Cef3D::Cef3DBrowser* browser,
		Cef3D::Cef3DOsrRenderType type,
		const std::vector<Cef3D::Cef3DRect>& dirtyRects,
		const void* buffer,
		int width,
		int height)
	{
		if (OverlayBrowser)
		{
			if (Renderer.GetWindow()->GetWidth() != width || Renderer.GetWindow()->GetHeight() != height)
			{
				// Re init Direct3D
				if (!Renderer.Resize(width, height))
				{
					PostQuitMessage(0);
				}
			}

			Renderer.UpdateOffscreenTexture(buffer, width, height);
			Renderer.Render();
		}
	}

	void OnBrowserCreated(Cef3D::Cef3DBrowser* browser)
	{
		if(!OverlayBrowser)
			OverlayBrowser = browser;
	}
};

int WINAPI WinMain(_In_ HINSTANCE hInInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ char* lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	{
		int WinWidth = 800;
		int WinHeight = 600;

		class RootWindowWndProc : public WndProcListener
		{
		public:
			LRESULT WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override
			{
				return RootWindowProc(hWnd, message, wParam, lParam);
			}
		};

		InitWindowDefinition windowDef;
		windowDef.Width = WinWidth;
		windowDef.Height = WinHeight;
		windowDef.Instance = hInInstance;
		windowDef.Delegate = new RootWindowWndProc();

		Win32Window window;
		HWND TopWindow = window.CreateNativeWindow(windowDef);
		if (!TopWindow)
			return -1;

		if (!Renderer.Init(&window))
			return -1;
		
		if (!Renderer.InitResources())
			return -1;
		
		bool isSubProcessed = true;

		Cef3D::Cef3DDefinition definition;
		definition.UseChildProcess = isSubProcessed;
		definition.OffscreenRendering = false;
		definition.UseCefLoop = true;

		bool init = Cef3D_Init(definition);

		if (!init)
			return -1;

		scoped_ptr<Cef3DDelegate> Cef3DListener(new Cef3DDelegate);

		Cef3D::Cef3DBrowserDefinition def;
		def.DefaultUrl = "file:///test/index.html";
		def.Rect = Cef3D::Cef3DRect(WinWidth, WinHeight);
		def.ParentHandle = TopWindow;

		scoped_ptr<Cef3D::Cef3DBrowser> browser2;
		browser2.reset(Cef3D_CreateBrowser(def));

		Cef3D::Cef3DDelegates::OnPaint.Add(CEF3D_BIND(&Cef3DDelegate::OnPaint, Cef3DListener.get()));
		Cef3D::Cef3DDelegates::OnAfterCreated.Add(CEF3D_BIND(&Cef3DDelegate::OnBrowserCreated, Cef3DListener.get()));
		
		Cef3D_PumpMessageLoop(true);
		
		Renderer.Shutdown();

		delete windowDef.Delegate;

		// Currently, if we close like this, Chromium might not have enough time to cleanup everything, so wait for everything to shut down before exiting our message loop
		Cef3D_Shutdown();
	}

	return 0;
}

LRESULT CALLBACK RootWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
	{
		return OverlayBrowser->OnClose();
	} break;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);

		if(OverlayBrowser)
			OverlayBrowser->Invalidate();
		break;
	}

	case WM_SYSCHAR:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_CHAR:
	{
		if (!OverlayBrowser)
			break;

		if (message == WM_KEYDOWN)
		{
			if (wParam == VK_ESCAPE)
			{
				PostQuitMessage(0);
				return 0;
			}
		}

		bool isSystemKey = message == WM_SYSCHAR ||
			message == WM_SYSKEYDOWN ||
			message == WM_SYSKEYUP;

		if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
			OverlayBrowser->SendKeyEvent(Cef3D::CEF3D_KEY_RAWKEYDOWN, lParam, wParam, isSystemKey, GetKeyboardModifiers(wParam, lParam));
		else if (message == WM_KEYUP || message == WM_SYSKEYUP)
			OverlayBrowser->SendKeyEvent(Cef3D::CEF3D_KEY_UP, lParam, wParam, isSystemKey, GetKeyboardModifiers(wParam, lParam));
		else
			OverlayBrowser->SendKeyEvent(Cef3D::CEF3D_KEY_CHAR, lParam, wParam, isSystemKey, GetKeyboardModifiers(wParam, lParam));

		break;
	}

	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		int x = GET_X_LPARAM(lParam);
		int y = GET_Y_LPARAM(lParam);

		Cef3D::Cef3DMouseEventType btnType =
			(message == WM_LBUTTONDOWN ? Cef3D::CEF3D_LBUTTON_DOWN : (
				message == WM_RBUTTONDOWN ? Cef3D::CEF3D_RBUTTON_DOWN : Cef3D::CEF3D_MBUTTON_DOWN));

		if (OverlayBrowser)
		{
			OverlayBrowser->SendMouseClickEvent(btnType, x, y, GetMouseModifiers(wParam));
		}
		break;
	}

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		int x = GET_X_LPARAM(lParam);
		int y = GET_Y_LPARAM(lParam);

		Cef3D::Cef3DMouseEventType btnType =
			(message == WM_LBUTTONUP ? Cef3D::CEF3D_LBUTTON_UP : (
				message == WM_RBUTTONUP ? Cef3D::CEF3D_RBUTTON_UP : Cef3D::CEF3D_MBUTTON_UP));

		if (OverlayBrowser)
		{
			OverlayBrowser->SendMouseClickEvent(btnType, x, y, GetMouseModifiers(wParam));
		}
		break;
	}

	case WM_MOUSEMOVE:
	{
		int x = GET_X_LPARAM(lParam);
		int y = GET_Y_LPARAM(lParam);

		if (!MouseTracking)
		{
			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof(TRACKMOUSEEVENT);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = hWnd;
			TrackMouseEvent(&tme);
		}

		if (OverlayBrowser)
		{
			OverlayBrowser->SendMouseClickEvent(Cef3D::CEF3D_MOUSE_MOVE, x, y, GetMouseModifiers(wParam));
		}
		break;
	}
	case WM_MOUSELEAVE:
	{
		if (MouseTracking)
		{
			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof(TRACKMOUSEEVENT);
			tme.dwFlags = TME_LEAVE & TME_CANCEL;
			tme.hwndTrack = hWnd;
			TrackMouseEvent(&tme);
			MouseTracking = false;
		}

		if (OverlayBrowser)
		{
			POINT p;
			::GetCursorPos(&p);
			::ScreenToClient(hWnd, &p);

			OverlayBrowser->SendMouseClickEvent(Cef3D::CEF3D_MOUSE_LEAVE, p.x, p.y, GetMouseModifiers(wParam));
		}
		break;
	}
	case WM_MOUSEWHEEL:
	{
		POINT screen_point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		HWND scrolled_wnd = ::WindowFromPoint(screen_point);
		if (scrolled_wnd != hWnd)
			break;

		ScreenToClient(hWnd, &screen_point);
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);

		if (OverlayBrowser)
		{
			OverlayBrowser->SendMouseWheelEvent(screen_point.x, screen_point.y, IsKeyDown(VK_SHIFT) ? delta : 0, !IsKeyDown(VK_SHIFT) ? delta : 0, GetMouseModifiers(wParam));
		}
		break;
	}
	case WM_SIZE:
	{
		if (OverlayBrowser)
		{
			RECT clientRect;
			::GetClientRect(hWnd, &clientRect);

			OverlayBrowser->SendResize(Cef3D::Cef3DRect(clientRect.left,clientRect.top,clientRect.right - clientRect.left,clientRect.bottom - clientRect.top));
		}
		break;
	}
	break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

void PumpMessageLoop()
{
	MSG msg;

	while (TRUE)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				break;
		}
	}
}

/* Input helpers for Windows */
int GetMouseModifiers(WPARAM wparam)
{
	int modifiers = 0;
	if (wparam & MK_CONTROL)
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (wparam & MK_SHIFT)
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (IsKeyDown(VK_MENU))
		modifiers |= EVENTFLAG_ALT_DOWN;
	if (wparam & MK_LBUTTON)
		modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
	if (wparam & MK_MBUTTON)
		modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
	if (wparam & MK_RBUTTON)
		modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;

	// Low bit set from GetKeyState indicates "toggled".
	if (::GetKeyState(VK_NUMLOCK) & 1)
		modifiers |= EVENTFLAG_NUM_LOCK_ON;
	if (::GetKeyState(VK_CAPITAL) & 1)
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;
	return modifiers;
}

int GetKeyboardModifiers(WPARAM wparam, LPARAM lparam)
{
	int modifiers = 0;
	if (IsKeyDown(VK_SHIFT))
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (IsKeyDown(VK_CONTROL))
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (IsKeyDown(VK_MENU))
		modifiers |= EVENTFLAG_ALT_DOWN;

	// Low bit set from GetKeyState indicates "toggled".
	if (::GetKeyState(VK_NUMLOCK) & 1)
		modifiers |= EVENTFLAG_NUM_LOCK_ON;
	if (::GetKeyState(VK_CAPITAL) & 1)
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;

	switch (wparam) {
	case VK_RETURN:
		if ((lparam >> 16) & KF_EXTENDED)
			modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_INSERT:
	case VK_DELETE:
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_UP:
	case VK_DOWN:
	case VK_LEFT:
	case VK_RIGHT:
		if (!((lparam >> 16) & KF_EXTENDED))
			modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_NUMLOCK:
	case VK_NUMPAD0:
	case VK_NUMPAD1:
	case VK_NUMPAD2:
	case VK_NUMPAD3:
	case VK_NUMPAD4:
	case VK_NUMPAD5:
	case VK_NUMPAD6:
	case VK_NUMPAD7:
	case VK_NUMPAD8:
	case VK_NUMPAD9:
	case VK_DIVIDE:
	case VK_MULTIPLY:
	case VK_SUBTRACT:
	case VK_ADD:
	case VK_DECIMAL:
	case VK_CLEAR:
		modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_SHIFT:
		if (IsKeyDown(VK_LSHIFT))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (IsKeyDown(VK_RSHIFT))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_CONTROL:
		if (IsKeyDown(VK_LCONTROL))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (IsKeyDown(VK_RCONTROL))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_MENU:
		if (IsKeyDown(VK_LMENU))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (IsKeyDown(VK_RMENU))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_LWIN:
		modifiers |= EVENTFLAG_IS_LEFT;
		break;
	case VK_RWIN:
		modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	}
	return modifiers;
}

bool IsKeyDown(WPARAM wparam) {
	return (GetKeyState(wparam) & 0x8000) != 0;
}

