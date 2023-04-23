// compile with: /D_UNICODE /DUNICODE /DWIN32 /D_WINDOWS /c

#include <windows.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <wrl.h>
#include <wil/com.h>
// <IncludeHeader>
// include WebView2 header
#include "WebView2.h"
// </IncludeHeader>

using namespace Microsoft::WRL;

// Global variables

// The main window class name.
static TCHAR szWindowClass[] = _T("DesktopApp");

// The string that appears in the application's title bar.
static TCHAR szTitle[] = _T("WebView sample");

HINSTANCE hInst;

// Forward declarations of functions included in this code module:
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProc2(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
// Pointer to WebViewController
static wil::com_ptr<ICoreWebView2Controller> webviewController;

// Pointer to WebView window
static wil::com_ptr<ICoreWebView2> webview;

int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR     lpCmdLine,
	_In_ int       nCmdShow
)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL,
			_T("Call to RegisterClassEx failed!"),
			_T("Windows Desktop Guided Tour"),
			NULL);

		return 1;
	}

	// Store instance handle in our global variable
	hInst = hInstance;

	// The parameters to CreateWindow explained:
	// szWindowClass: the name of the application
	// szTitle: the text that appears in the title bar
	// WS_OVERLAPPEDWINDOW: the type of window to create
	// CW_USEDEFAULT, CW_USEDEFAULT: initial position (x, y)
	// 500, 100: initial size (width, length)
	// NULL: the parent of this window
	// NULL: this application does not have a menu bar
	// hInstance: the first parameter from WinMain
	// NULL: not used in this application
	HWND hWnd = CreateWindow(
		szWindowClass,
		szTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		1200, 900,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (!hWnd)
	{
		MessageBox(NULL,
			_T("Call to CreateWindow failed!"),
			_T("Windows Desktop Guided Tour"),
			NULL);

		return 1;
	}

	// The parameters to ShowWindow explained:
	// hWnd: the value returned from CreateWindow
	// nCmdShow: the fourth parameter from WinMain
	ShowWindow(hWnd,
		nCmdShow);
	UpdateWindow(hWnd);


	// <-- WebView2 sample code starts here -->
	// Step 3 - Create a single WebView within the parent window
	// Locate the browser and set up the environment for WebView
	CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[hWnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {

				// Create a CoreWebView2Controller and get the associated CoreWebView2 whose parent is the main window hWnd
				env->CreateCoreWebView2Controller(hWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
					[hWnd, env](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
						if (controller != nullptr) {
							webviewController = controller;
							webviewController->get_CoreWebView2(&webview);
						}

						// Add a few settings for the webview
						// The demo step is redundant since the values are the default settings
						wil::com_ptr<ICoreWebView2Settings> settings;
						webview->get_Settings(&settings);
						settings->put_IsScriptEnabled(TRUE);
						settings->put_AreDefaultScriptDialogsEnabled(TRUE);
						settings->put_IsWebMessageEnabled(FALSE);

						// Resize WebView to fit the bounds of the parent window
						RECT bounds;
						GetClientRect(hWnd, &bounds);
						webviewController->put_Bounds(bounds);

						// Schedule an async task to navigate to Outlook project
						webview->Navigate(L"https://outlook.live.com/");

						// <NavigationEvents>
						// Step 4 - Navigation events
						// register an ICoreWebView2NavigationStartingEventHandler to cancel any non-https navigation
						EventRegistrationToken token;
						webview->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
							[](ICoreWebView2* webview, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
								wil::unique_cotaskmem_string uri;
								args->get_Uri(&uri);
								std::wstring source(uri.get());
								if (source.substr(0, 5) != L"https") {
									args->put_Cancel(true);
								}
								return S_OK;
							}).Get(), &token);
						// </NavigationEvents>

						// <Scripting>
						// Step 5 - Scripting
						// Schedule an async task to add initialization script that freezes the Object object
						webview->AddScriptToExecuteOnDocumentCreated(L"Object.freeze(Object);", nullptr);
						// Schedule an async task to get the document URL
						webview->ExecuteScript(L"window.document.URL;", Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
							[](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
								LPCWSTR URL = resultObjectAsJson;
								//doSomethingWithURL(URL);
								return S_OK;
							}).Get());
						// </Scripting>

						// <CommunicationHostWeb>
						// Step 6 - Communication between host and web content
						// Set an event handler for the host to return received message back to the web content
						webview->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
							[](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
								wil::unique_cotaskmem_string message;
								args->TryGetWebMessageAsString(&message);
								// processMessage(&message);
								webview->PostWebMessageAsString(message.get());
								return S_OK;
							}).Get(), &token);

						// Add a custom handler for new window request
						webview->add_NewWindowRequested(
							Callback<ICoreWebView2NewWindowRequestedEventHandler>(
								[env](ICoreWebView2* sender,
									ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
										// Handle window.open event here
										LPWSTR uri;
										args->get_Uri(&uri);
										wchar_t const* prefix = L"about:blank";
										if (wcslen(uri) && wcsncmp(uri, prefix, wcslen(prefix)) != 0)
										{
											args->put_Handled(FALSE);
											return S_OK;
										}

										wil::com_ptr<ICoreWebView2NewWindowRequestedEventArgs> eventArgs;
										eventArgs = args;
										wil::com_ptr<ICoreWebView2Deferral> deferral;
										HRESULT hrOk =  args->GetDeferral(&deferral);

										if (SUCCEEDED(hrOk)) {

											// Define the window class and register it
											WNDCLASSEX wcex = {};
											wcex.cbSize = sizeof(WNDCLASSEX);
											wcex.lpfnWndProc = WndProc2;
											wcex.hInstance = hInst;
											wcex.lpszClassName = L"ProjectionClass";
											RegisterClassEx(&wcex);

											// Create the window
											HWND hWnd = CreateWindowEx(
												0,
												L"ProjectionClass",
												L"",
												WS_OVERLAPPEDWINDOW,
												CW_USEDEFAULT, CW_USEDEFAULT,
												800, 600,
												NULL,
												NULL,
												hInst,
												NULL
											);

											if (true)
											{
												HRESULT hr = env->CreateCoreWebView2Controller(hWnd,
													Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
														[eventArgs, hWnd, deferral](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
															if (controller) {

																wil::com_ptr<ICoreWebView2> newWebView;
																controller->get_CoreWebView2(&newWebView);
																eventArgs->put_NewWindow(newWebView.get());

																EventRegistrationToken token;
																newWebView->add_DocumentTitleChanged(
																	Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
																		[hWnd](ICoreWebView2* sender, IUnknown* args) -> HRESULT {
																			LPWSTR title;
																			sender->get_DocumentTitle(&title);
																			SetWindowText(hWnd, title);
																			return S_OK;
																		})
																	.Get(),
																			&token);

																//newWebView->add_SourceChanged(
																//	Callback<IWebView2SourceChangedEventHandler>(
																//	[](IWebView2WebView* sender, IWebView2SourceChangedEventArgs* args) -> HRESULT {
																//		return S_OK;
																//	})
																//	.Get(),
																//	nullptr
																//		);
																EventRegistrationToken token1;

																newWebView->add_SourceChanged(
																	Callback<ICoreWebView2SourceChangedEventHandler>(
																		[hWnd](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args)
																		-> HRESULT {
																			LPWSTR uri;
																			sender->get_Source(&uri);
																			PostMessage(hWnd, WM_CLOSE, 0, 0);
																			return S_OK;
																		})
																	.Get(),
																			&token1);


																controller->AddRef();
																SetProp(hWnd, L"Controller", controller);

																controller->put_IsVisible(TRUE);
																ShowWindow(hWnd, SW_SHOW);
																deferral->Complete();
																return S_OK;
															}
															return S_OK;
														}).Get());
											}
											eventArgs->put_Handled(TRUE);
										}
										return S_OK;
								})
							.Get(),
									nullptr);
						return S_OK;
					}).Get());
				return S_OK;
			}).Get());



	// <-- WebView2 sample code ends here -->

	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc2(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	ICoreWebView2Controller* controller = reinterpret_cast<ICoreWebView2Controller*>(GetProp(hWnd, L"Controller"));

	switch (message)
	{
	case WM_SIZE:
	case WM_WINDOWPOSCHANGED:
		if (controller != nullptr) {
			RECT bounds;
			GetClientRect(hWnd, &bounds);
			controller->put_Bounds(bounds);
		};
		break;
	case WM_DESTROY:
		if (controller)
		{
			controller->Release();
		}
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}

//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_DESTROY  - post a quit message and return
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	TCHAR greeting[] = _T("Hello, Windows desktop!");

	switch (message)
	{
	case WM_SIZE:
		if (webviewController != nullptr) {
			RECT bounds;
			GetClientRect(hWnd, &bounds);
			webviewController->put_Bounds(bounds);
		};
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}
