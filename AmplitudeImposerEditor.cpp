//	AmplitudeImposerEditor.cpp - Plugin's gui.
//	--------------------------------------------------------------------------
//	Copyright (c) 2004 Niall Moody
//	
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:

//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.

//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//	THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.
//	--------------------------------------------------------------------------
#include "deps/include/node.h"
#include "AmplitudeImposerEditor.h"
#include "AmplitudeImposer.h"
#include "include/cef_client.h"
#include "include/cef_browser.h"
#include "cefclient/cefclient.h"
#include "cefclient/client_handler.h"
#include "cefclient/resource.h"

#include <stdio.h>

#ifdef WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif
#include <winuser.h>

#define MAX_LOADSTRING 100
#define MAX_URL_LENGTH  255
#define BUTTON_WIDTH 72
#define URLBAR_HEIGHT  24

static HWND backWnd = NULL, 
			forwardWnd = NULL, 
			reloadWnd = NULL,
			stopWnd = NULL, 
			editWnd = NULL;

//-----------------------------------------------------------------------------
// resource id's
enum {
	// bitmaps
	kBack = 128,
	kSlider,
};

//-----------------------------------------------------------------------------
// AmplitudeImposerEditor class implementation
//-----------------------------------------------------------------------------
AmplitudeImposerEditor::AmplitudeImposerEditor(AudioEffect *effect)
 : AEffGUIEditor(effect) 
{
	depth = 0;
	threshold = 0;

	backBitmap = 0;
	sliderBitmap = 0;

	// load the background bitmap
	// we don't need to load all bitmaps, this could be done when open is called
	backBitmap  = new CBitmap(kBack);

	// init the size of the plugin
// 	rect.left   = 0;
// 	rect.top    = 0;
// 	rect.right  = effect->getEditor()->getRect( rect );
// 	rect.bottom = getFrame()->getHeight();

// 	RECT rect;
// 	GetWindowRect( (HWND)getFrame()->getSystemWindow(), &rect );
// 
// 	int i = 0;
// 	i++;
}

//-----------------------------------------------------------------------------
AmplitudeImposerEditor::~AmplitudeImposerEditor()
{
	// free background bitmap
	if(backBitmap)
		backBitmap->forget();
	backBitmap = 0;
}

//-----------------------------------------------------------------------------
void launchNode(uv_work_t *req) {
	char cCurrentPath[FILENAME_MAX]; 

	GetCurrentDir(cCurrentPath, sizeof(cCurrentPath));

	char *argv[] = {cCurrentPath, "C:/test.js" };
	int argc = 2;

	node::Start( argc, argv, true );
	//node::RunBlockingLoopAsync();
}

void afterCB(uv_work_t *req) {
	fprintf(stderr, "Done calculating %dth fibonacci\n", *(int *) req->data);
}

static uv_thread_t asyncThread;

//-----------------------------------------------------------------------------
bool AmplitudeImposerEditor::open(void *ptr)
{
	CPoint point;
	CRect size;

	// !!! always call this !!!
	AEffGUIEditor::open(ptr);

	// Initialize Node.js
	initNode();

	// load some bitmaps
	if(!sliderBitmap)
		sliderBitmap = new CBitmap(kSlider);

	//--init background frame-------------------------------------------------
	size(0, 0, 1024, 768 );
	frame = new CFrame(size, ptr, this);


	RECT rect;
	GetWindowRect( (HWND)getFrame()->getSystemWindow(), &rect );

	// Copy over into a CRect
	CRect cRect;
	cRect.bottom = rect.bottom;
	cRect.top = rect.top;
	cRect.left = rect.left;
	cRect.right = rect.right;

	getFrame()->setViewSize( cRect );

	//--init sliders----------------------------------------------------------
	size(114, 71, 278, 87);
	point(114, 71);
	depth = new RightClickHSlider(size, this, kDepth, 114, (278-sliderBitmap->getWidth()), sliderBitmap, backBitmap, point, kLeft);
	depth->setValue(effect->getParameter(kDepth));
	depth->setDrawTransparentHandle(false);
	frame->addView(depth);

	size(114, 110, 278, 126);
	point(114, 110);
	threshold = new RightClickHSlider(size, this, kThreshold, 114, (278-sliderBitmap->getWidth()), sliderBitmap, backBitmap, point, kLeft);
	threshold->setValue(effect->getParameter(kThreshold));
	threshold->setDrawTransparentHandle(false);
	frame->addView(threshold);

	// Initialize CEF
	initCEF( rect );

	return true;
}

void AmplitudeImposerEditor::initCEF( RECT rect ) {
	HWND hWnd = (HWND)getFrame()->getSystemWindow();
	HINSTANCE hInst = (HINSTANCE)GetWindowLong((HWND)getFrame()->getSystemWindow(), GWL_HINSTANCE);

	// Grab our client rect
	//GetClientRect( (HWND)getFrame()->getSystemWindow(), &rect );

	rect.top = 0;
	rect.bottom = getFrame()->getHeight();
	rect.left = 0;
	rect.right = getFrame()->getWidth();

	int x = 0;
	backWnd = CreateWindow("BUTTON", "Back",
							WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON
							| WS_DISABLED, x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
							hWnd, (HMENU) IDC_NAV_BACK, hInst, 0);
	x += BUTTON_WIDTH;

	forwardWnd = CreateWindow("BUTTON", "Forward",
							WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON
							| WS_DISABLED, x, 0, BUTTON_WIDTH,
							URLBAR_HEIGHT, hWnd, (HMENU) IDC_NAV_FORWARD,
							hInst, 0);
	x += BUTTON_WIDTH;

	reloadWnd = CreateWindow("BUTTON", "Reload",
							WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON
							| WS_DISABLED, x, 0, BUTTON_WIDTH,
							URLBAR_HEIGHT, hWnd, (HMENU) IDC_NAV_RELOAD,
							hInst, 0);
	x += BUTTON_WIDTH;

	stopWnd = CreateWindow("BUTTON", "Stop",
							WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON
							| WS_DISABLED, x, 0, BUTTON_WIDTH, URLBAR_HEIGHT,
							hWnd, (HMENU) IDC_NAV_STOP, hInst, 0);
	x += BUTTON_WIDTH;

	editWnd = CreateWindow("EDIT", 0,
							WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT |
							ES_AUTOVSCROLL | ES_AUTOHSCROLL| WS_DISABLED,
							x, 0, rect.right - BUTTON_WIDTH * 4,
							URLBAR_HEIGHT, hWnd, 0, hInst, 0);

	// Initialize CEF
	init( hInst, hWnd, rect );

	// Create an instance of our CefClient implementation. Various methods in the
	// MyClient instance will be called to notify about and customize browser
	// behavior.
	CefRefPtr<ClientHandler> client;

	client = new ClientHandler();
	client->SetMainHwnd( (HWND)getFrame()->getSystemWindow() );
	client->SetEditHwnd( editWnd );
	client->SetButtonHwnds( backWnd, forwardWnd, reloadWnd, stopWnd );

	// Information about the parent window, client rectangle, etc.
	CefWindowInfo info;
	CefBrowserSettings settings;

	info.SetAsChild( (HWND)getFrame()->getSystemWindow(), rect );

	// Populate the settings based on command line arguments.
	AppGetBrowserSettings( settings );

	// Create the new child browser window
	//CefBrowserHost::CreateBrowserSync( info, client.get(), "http://www.google.com", settings );

	// Create the new browser window object asynchronously. This eventually results
	// in a call to CefLifeSpanHandler::OnAfterCreated().
	CefBrowserHost::CreateBrowser(info, client.get(), "http://www.google.com", settings);
}

void AmplitudeImposerEditor::initNode() {
	uv_work_t req;
	uv_thread_create( &asyncThread, (void (__cdecl *)(void *))launchNode, NULL );
}

//-----------------------------------------------------------------------------
void AmplitudeImposerEditor::close()
{
	delete frame;
	frame = 0;

	if(sliderBitmap)
	{
		sliderBitmap->forget();
		sliderBitmap = 0;
	}
}

//-----------------------------------------------------------------------------
void AmplitudeImposerEditor::setParameter(long index, float value)
{
	if(!frame)
		return;

	// called from Template
	switch(index)
	{
		case kDepth:
			depth->setValue(effect->getParameter(index));
			break;
		case kThreshold:
			threshold->setValue(effect->getParameter(index));
			break;
	}
	//postUpdate();
}

//-----------------------------------------------------------------------------
void AmplitudeImposerEditor::valueChanged(CDrawContext* context, CControl* control)
{
	long tag = control->getTag();

	switch(tag)
	{
		case kDepth:
		case kThreshold:
			effect->setParameterAutomated(tag, control->getValue());
			//control->update(context);
			break;
	}
}

