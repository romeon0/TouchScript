/*
* @author Valentin Simonov / http://va.lent.in/
*/

#include "WindowsTouch.h"

extern "C" 
{

	void __stdcall Init(TOUCH_API api, 
		MousePointerBeganFuncPtr mouseBegan, MousePointerMovedFuncPtr mouseMoved,
		TouchPointerBeganFuncPtr touchBegan, TouchPointerMovedFuncPtr touchMoved,
		PenPointerBeganFuncPtr penBegan, PenPointerMovedFuncPtr penMoved,
		PointerEndedFuncPtr ended, PointerCancelledFuncPtr cancelled)
	{
		_mousePointerBeganFunc = mouseBegan;
		_mousePointerMovedFunc = mouseMoved;
		_touchPointerBeganFunc = touchBegan;
		_touchPointerMovedFunc = touchMoved;
		_penPointerBeganFunc = penBegan;
		_penPointerMovedFunc = penMoved;
		_pointerEndedFunc = ended;
		_pointerCancelledFunc = cancelled;
		_api = api;

		_currentWindow = FindWindowA("UnityWndClass", NULL);
		if (api == WIN8)
		{
			_oldWindowProc = SetWindowLongPtr(_currentWindow, GWLP_WNDPROC, (LONG_PTR)wndProc8);
		}
		else
		{
			RegisterTouchWindow(_currentWindow, 0);
			_oldWindowProc = SetWindowLongPtr(_currentWindow, GWLP_WNDPROC, (LONG_PTR)wndProc7);
		}
	}

	void __stdcall Dispose()
	{
		if (_oldWindowProc)
		{
			SetWindowLongPtr(_currentWindow, GWLP_WNDPROC, (LONG_PTR)_oldWindowProc);
			_oldWindowProc = 0;
			if (_api == WIN7)
			{
				UnregisterTouchWindow(_currentWindow);
			}
		}
	}

	void __stdcall SetScreenParams(int width, int height, float offsetX, float offsetY, float scaleX, float scaleY)
	{
		_screenWidth = width;
		_screenHeight = height;
		_offsetX = offsetX;
		_offsetY = offsetY;
		_scaleX = scaleX;
		_scaleY = scaleY;
	}

}

LRESULT CALLBACK wndProc8(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_TOUCH:
		CloseTouchInputHandle((HTOUCHINPUT)lParam);
		break;
	case WM_POINTERDOWN:
	case WM_POINTERUP:
	case WM_POINTERUPDATE:
		decodeWin8Touches(msg, wParam, lParam);
		break;
	default:
		return CallWindowProc((WNDPROC)_oldWindowProc, hwnd, msg, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK wndProc7(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_TOUCH:
		decodeWin7Touches(msg, wParam, lParam);
		break;
	default:
		return CallWindowProc((WNDPROC)_oldWindowProc, hwnd, msg, wParam, lParam);
	}
	return 0;
}

void decodeWin8Touches(UINT msg, WPARAM wParam, LPARAM lParam)
{
	int pointerId = GET_POINTERID_WPARAM(wParam);

	POINTER_INFO pointerInfo;
	if (!GetPointerInfo(pointerId, &pointerInfo)) return;

	POINT p;
	p.x = pointerInfo.ptPixelLocation.x;
	p.y = pointerInfo.ptPixelLocation.y;
	ScreenToClient(_currentWindow, &p);

	switch (msg)
	{
	case WM_POINTERDOWN: 
	{
		if ((pointerInfo.pointerFlags & POINTER_FLAG_CANCELED) != 0) return;

		Vector2 position = Vector2(((float)p.x - _offsetX) * _scaleX, _screenHeight - ((float)p.y - _offsetY) * _scaleY);
		unsigned int buttons = 0, b;
		switch (pointerInfo.pointerType)
		{
		case PT_MOUSE:
			b = (((unsigned int)pointerInfo.ButtonChangeType - 1) / 2) * 3;
			buttons |= 1 << (b + 1); // add down
			buttons |= 1 << b; // add pressed
			_mousePointerBeganFunc(pointerId, buttons, position);
			break;
		case PT_TOUCH:
			POINTER_TOUCH_INFO touchInfo;
			GetPointerTouchInfo(pointerId, &touchInfo);
			buttons = 1 + 2; // first button down, pressed
			_touchPointerBeganFunc(pointerId, buttons, touchInfo.orientation, touchInfo.pressure, position);
			break;
		case PT_PEN:
			b = (((unsigned int)pointerInfo.ButtonChangeType - 1) / 2) * 3;
			buttons |= 1 << (b + 1); // add down
			buttons |= 1 << b; // add pressed
			_penPointerBeganFunc(pointerId, buttons, position);
			break;
		}
		break;
	}
	case WM_POINTERUP:
	{
		if ((pointerInfo.pointerFlags & POINTER_FLAG_CANCELED) != 0)
		{
			_pointerCancelledFunc(pointerId, pointerInfo.pointerType);
		}
		else 
		{
			unsigned int buttons = 0, b;
			switch (pointerInfo.pointerType)
			{
			case PT_MOUSE:
			case PT_PEN:
				b = (((unsigned int)pointerInfo.ButtonChangeType - 1) / 2) * 3;
				buttons |= 1 << (b + 2); // add up
				break;
			case PT_TOUCH:
				buttons = 4; // first button up
				break;
			}
			_pointerEndedFunc(pointerId, pointerInfo.pointerType, buttons);
		}
		break;
	}
	case WM_POINTERUPDATE:
	{
		if ((pointerInfo.pointerFlags & POINTER_FLAG_CANCELED) != 0)
		{
			_pointerCancelledFunc(pointerId, pointerInfo.pointerType);
		}
		else 
		{
			Vector2 position = Vector2(((float)p.x - _offsetX) * _scaleX, _screenHeight - ((float)p.y - _offsetY) * _scaleY);
			unsigned int buttonsSet = 0, buttonsClear = 0;
			if (pointerInfo.ButtonChangeType != POINTER_CHANGE_NONE)
			{
				unsigned int change = (unsigned int)pointerInfo.ButtonChangeType;
				if (change % 2 == 0) // up
				{
					unsigned int b = (change / 2 - 1) * 3;
					buttonsSet |= 1 << (b + 2); // add up
					buttonsClear |= 1 << b; // remove pressed
				}
				else // down
				{
					unsigned int b = ((change - 1) / 2) * 3;
					buttonsSet |= 1 << (b + 1); // add down
					buttonsSet |= 1 << b; // add pressed
				}
			}
			switch (pointerInfo.pointerType)
			{
			case PT_MOUSE:
				_mousePointerMovedFunc(pointerId, buttonsSet, buttonsClear, position);
				break;
			case PT_TOUCH:
				POINTER_TOUCH_INFO touchInfo;
				GetPointerTouchInfo(pointerId, &touchInfo);
				_touchPointerMovedFunc(pointerId, buttonsSet, buttonsClear, touchInfo.orientation, touchInfo.pressure, position);
				break;
			case PT_PEN:
				_penPointerMovedFunc(pointerId, buttonsSet, buttonsClear, position);
				break;
			}
		}
		break;
	}
	}
}

void decodeWin7Touches(UINT msg, WPARAM wParam, LPARAM lParam)
{
	UINT cInputs = LOWORD(wParam);
	PTOUCHINPUT pInputs = new TOUCHINPUT[cInputs];

	if (!pInputs) return;
	if (!GetTouchInputInfo((HTOUCHINPUT)lParam, cInputs, pInputs, sizeof(TOUCHINPUT))) return;

	for (UINT i = 0; i < cInputs; i++)
	{
		TOUCHINPUT touch = pInputs[i];

		POINT p;
		p.x = touch.x / 100;
		p.y = touch.y / 100;
		ScreenToClient(_currentWindow, &p);

		if ((touch.dwFlags & TOUCHEVENTF_DOWN) != 0)
		{
			Vector2 position = Vector2(((float)p.x - _offsetX) * _scaleX, _screenHeight - ((float)p.y - _offsetY) * _scaleY);
			_touchPointerBeganFunc(touch.dwID, 3, 0, 0, position);
		}
		else if ((touch.dwFlags & TOUCHEVENTF_UP) != 0)
		{
			_pointerEndedFunc(touch.dwID, PT_TOUCH, 4);
		}
		else if ((touch.dwFlags & TOUCHEVENTF_MOVE) != 0)
		{
			Vector2 position = Vector2(((float)p.x - _offsetX) * _scaleX, _screenHeight - ((float)p.y - _offsetY) * _scaleY);
			_touchPointerMovedFunc(touch.dwID, 0, 0, 0, 0, position);
		}
	}

	CloseTouchInputHandle((HTOUCHINPUT)lParam);
	delete[] pInputs;
}