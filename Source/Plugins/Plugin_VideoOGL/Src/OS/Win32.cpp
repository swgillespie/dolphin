// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/



//////////////////////////////////////////////////////////////////////////////////////////
// Include
// ŻŻŻŻŻŻŻŻŻŻŻŻŻŻ
#include <windows.h>

#include <wx/wx.h>
#include <wx/filepicker.h>
#include <wx/notebook.h>
#include <wx/dialog.h>
#include <wx/aboutdlg.h>

#include "../Globals.h" // Local
#include "../Config.h"
#include "main.h"
#include "Win32.h"
#include "Render.h" // for AddMessage

#include "StringUtil.h" // Common: For StringFromFormat
//////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ŻŻŻŻŻŻŻŻŻŻ
void OpenConsole();
void CloseConsole();

HINSTANCE g_hInstance;

class wxDLLApp : public wxApp
{
	bool OnInit()
	{
		return true;
	}
};
IMPLEMENT_APP_NO_MAIN(wxDLLApp) 

WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);


BOOL APIENTRY DllMain(HINSTANCE hinstDLL,	// DLL module handle
					  DWORD dwReason,		// reason called
					  LPVOID lpvReserved)	// reserved
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		{       //use wxInitialize() if you don't want GUI instead of the following 12 lines
			wxSetInstance((HINSTANCE)hinstDLL);
			int argc = 0;
			char **argv = NULL;
			wxEntryStart(argc, argv);
			if ( !wxTheApp || !wxTheApp->CallOnInit() )
				return FALSE;
		}
		break; 

	case DLL_PROCESS_DETACH:
		CloseConsole();
		wxEntryCleanup(); //use wxUninitialize() if you don't want GUI 
		break;
	default:
		break;
	}

	g_hInstance = hinstDLL;
	return TRUE;
}

void DoDllDebugger();
extern bool gShowDebugger;
//////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////////////
// The rendering window
// ŻŻŻŻŻŻŻŻŻŻ
namespace EmuWindow
{
	HWND m_hWnd = NULL; // The new window that is created here
    HWND m_hParent = NULL, m_hMain = NULL; // The main CPanel
	HINSTANCE m_hInstance = NULL;
	WNDCLASSEX wndClass;
	const TCHAR m_szClassName[] = "DolphinEmuWnd";
    int g_winstyle;
	

	// ------------------------------------------
	/* Invisible cursor option. In the lack of a predefined IDC_BLANK we make
	   an empty transparent cursor */
	// ------------------
	HCURSOR hCursor = NULL, hCursorBlank = NULL;
	void CreateCursors(HINSTANCE hInstance)
	{
		BYTE ANDmaskCursor[] = { 0xff };
		BYTE XORmaskCursor[] = { 0x00 };
		hCursorBlank = CreateCursor(hInstance, 0,0, 1,1, ANDmaskCursor,XORmaskCursor);

		hCursor = LoadCursor( NULL, IDC_ARROW );
	}
	

	HWND GetWnd()
	{
		return m_hWnd;
	}

    HWND GetParentWnd()
    {
        return m_hParent;
    }

	LRESULT CALLBACK WndProc( HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam )
	{		
		HDC         hdc;
		PAINTSTRUCT ps;
		switch( iMsg )
		{
		case WM_CREATE:
			PostMessage(m_hMain, WM_USER, 15, (int)m_hParent); // 15 = WM_USER_CREATE, make enum table if it's confusing
			break;

		case WM_PAINT:
			hdc = BeginPaint( hWnd, &ps );
			EndPaint( hWnd, &ps );
			return 0;

		case WM_KEYDOWN:
			switch( LOWORD( wParam ))
			{
			case VK_ESCAPE: // Pressing Esc Stop or Maximize
				//DestroyWindow(hWnd);
				//PostQuitMessage(0);

				/* The fullscreen option for Windows users is not very user friendly. With this the user
				   can only get out of the fullscreen mode by pressing Esc or Alt + F4. But that also
				   closes*/
				//if(m_hParent == NULL) ExitProcess(0);
				if(m_hParent == NULL)
				{
					if (g_Config.bFullscreen)
						PostMessage(m_hMain, WM_USER, 5, 0); // Stop
					else
						// Toggle maximize and restore
						if (IsZoomed(hWnd)) ShowWindow(hWnd, SW_RESTORE); else ShowWindow(hWnd, SW_MAXIMIZE);
					return 0;
				}				
				break;
				/*
			case MY_KEYS:
				hypotheticalScene->sendMessage(KEYDOWN...);
				*/
			case 'E': // EFB hotkey
				if(g_Config.bEFBCopyDisableHotKey)
				{
					g_Config.bEFBCopyDisable = !g_Config.bEFBCopyDisable;
					Renderer::AddMessage(StringFromFormat("Copy EFB was turned %s",
						g_Config.bEFBCopyDisable ? "off" : "on").c_str(), 5000);
				}
				break;
			}
			g_VideoInitialize.pKeyPress(LOWORD(wParam), GetAsyncKeyState(VK_SHIFT) != 0, GetAsyncKeyState(VK_CONTROL) != 0);
			break;

		/* The reason we pick up the WM_MOUSEMOVE is to be able to change this option
		   during gameplay. The alternative is to load one of the cursors when the plugin
		   is loaded and go with that. This should only produce a minimal performance hit
		   because SetCursor is not supposed to actually change the cursor if it's the
		   same as the one before. */
		case WM_MOUSEMOVE:
			/* Check rendering mode; child or parent. Then post the mouse moves to the main window
			   it's nessesary for both the chil dwindow and separate rendering window because
			   moves over the rendering window do not reach the main program then. */
			if (GetParentWnd() == NULL) // Separate rendering window
				PostMessage(m_hMain, iMsg, wParam, -1);			
			else
				PostMessage(GetParentWnd(), iMsg, wParam, lParam);
			break;

		/* To support the separate window rendering we get the message back here. So we basically
		    only let it pass through DolphinWX > Frame.cpp to determine if it should be on or off
			and coordinate it with the other settings if nessesary */
		case WM_USER:
			/* I set wParam to 10 just in case there are other WM_USER events. If we want more
			   WM_USER cases we would start making wParam or lParam cases */
			if(wParam == 10)
			{
				if(lParam)
					SetCursor(hCursor);
				else
					SetCursor(hCursorBlank);
			}
			break;

		/* Post thes mouse events to the main window, it's nessesary becase in difference to the
		   keyboard inputs these events only appear here, not in the main WndProc() */
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
			PostMessage(GetParentWnd(), iMsg, wParam, lParam);
		break;

		// This is called when we close the window when we render to a separate window
		case WM_CLOSE:	
			if(m_hParent == NULL)
			{
				ExitProcess(0);
				//Core::SetState(Core::CORE_UNINITIALIZED);

				/* Attempt to only Stop when we close the separate window. But it didn't work, it hanged.
				  It may need some more coordination with the Stop code in the Core */
				//PostMessage(m_hMain, WM_USER, 5, 0);

				return 0;
			}

		/* This is called from the Core when we Stop, but currently we only use DefWindowProc(),
		   whatever that does with it, if any */
		//case WM_QUIT:
			//Video_Shutdown();
		//	ExitProcess(0);			
		//	return 0;

		case WM_DESTROY:
			//Shutdown();
			//PostQuitMessage( 0 ); // Call WM_QUIT
			break;

		// Called when a screensaver wants to show up while this window is active
		case WM_SYSCOMMAND:
			switch (wParam) 
			{
			case SC_SCREENSAVE:
			case SC_MONITORPOWER:
				return 0;
			}
			break;
		}

		return DefWindowProc(hWnd, iMsg, wParam, lParam);
	}


	// This is called from 
	HWND OpenWindow(HWND parent, HINSTANCE hInstance, int width, int height, const TCHAR *title)
	{
		wndClass.cbSize = sizeof( wndClass );
		wndClass.style  = CS_HREDRAW | CS_VREDRAW;
		wndClass.lpfnWndProc = WndProc;
		wndClass.cbClsExtra = 0;
		wndClass.cbWndExtra = 0;
		wndClass.hInstance = hInstance;
		wndClass.hIcon = LoadIcon( NULL, IDI_APPLICATION );
		//wndClass.hCursor = LoadCursor( NULL, IDC_ARROW );
		wndClass.hCursor = NULL; // To interfer less with SetCursor() later
		wndClass.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
		wndClass.lpszMenuName = NULL;
		wndClass.lpszClassName = m_szClassName;
		wndClass.hIconSm = LoadIcon( NULL, IDI_APPLICATION );

		m_hInstance = hInstance;
		RegisterClassEx( &wndClass );

		CreateCursors(m_hInstance);

		// Create child window
        if (parent)
        {
			m_hParent = m_hMain = parent;

            m_hWnd = CreateWindow(m_szClassName, title,
                WS_CHILD,
                CW_USEDEFAULT, CW_USEDEFAULT,CW_USEDEFAULT, CW_USEDEFAULT,
                parent, NULL, hInstance, NULL );

            ShowWindow(m_hWnd, SW_SHOWMAXIMIZED);            
        }

		// Create new separate window
        else
        {
			DWORD style = g_Config.bFullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW;

            RECT rc = {0, 0, width, height};
            AdjustWindowRect(&rc, style, false);

            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            rc.left = (1280 - w)/2;
            rc.right = rc.left + w;
            rc.top = (1024 - h)/2;
            rc.bottom = rc.top + h;

			// I save this to m_hMain instead of m_hParent because it casused problems otherwise
			m_hMain = (HWND)g_VideoInitialize.pWindowHandle;

            m_hWnd = CreateWindow(m_szClassName, title,
                style,
                rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top,
                parent, NULL, hInstance, NULL );

            g_winstyle = GetWindowLong( m_hWnd, GWL_STYLE );
            g_winstyle &= ~WS_MAXIMIZE & ~WS_MINIMIZE; // remove minimize/maximize style
        }

		return m_hWnd;
	}

	void Show()
	{
		ShowWindow(m_hWnd, SW_SHOW);
		BringWindowToTop(m_hWnd);
		UpdateWindow(m_hWnd);

		// gShowDebugger from main.cpp is forgotten between the Dolphin-Debugger is opened and a game is
		// started so we have to use an ini file setting here
		/*
		bool bVideoWindow = false;
		IniFile ini;
		ini.Load(DEBUGGER_CONFIG_FILE);
		ini.Get("ShowOnStart", "VideoWindow", &bVideoWindow, false);
		if(bVideoWindow) DoDllDebugger();
		*/
	}

	HWND Create(HWND hParent, HINSTANCE hInstance, const TCHAR *title)
	{
		return OpenWindow(hParent, hInstance, 640, 480, title);
	}

	void Close()
	{
		DestroyWindow(m_hWnd);
		UnregisterClass(m_szClassName, m_hInstance);
	}


	void SetSize(int width, int height)
	{
		RECT rc = {0, 0, width, height};
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);

		int w = rc.right - rc.left;
		int h = rc.bottom - rc.top;

		rc.left = (1280 - w)/2;
		rc.right = rc.left + w;
		rc.top = (1024 - h)/2;
		rc.bottom = rc.top + h;
		::MoveWindow(m_hWnd, rc.left,rc.top,rc.right-rc.left,rc.bottom-rc.top, TRUE);
	}

} // EmuWindow
////////////////////////////////////