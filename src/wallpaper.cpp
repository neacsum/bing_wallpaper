#define _CRT_SECURE_NO_WARNINGS

#include <mlib/errorcode.h>
#include <mlib/json.h>
#include <mlib/trace.h>
#include <mlib/log.h>
#include <mlib/options.h>

#include <windows.h>
#include <shobjidl_core.h>
#include <shlobj_core.h>
#include <Wininet.h>
#include <iostream>
#include <format>
#include <filesystem>
#include <utf8/utf8.h>
#include <WtsApi32.h>

#include "resource.h"
#include "version.h"

#pragma comment (lib, "wininet.lib")
#pragma comment (lib, "Wtsapi32.lib")

#define WNDCLASSNAME      L"bing_wallpaper"
#define BING_SERVER       L"bing.com"
#define DELAY_MS          1000 //delay before restoring wallpaper

#define WM_TRAYNOTIFY     (WM_USER+1)
#define WM_REFRESH        (WM_USER+2)

json::node          img;
NOTIFYICONDATA      nid;
HWND                mainWnd;
HINTERNET           hhttp;
IDesktopWallpaper*  dwp;
std::string         app_title;
std::filesystem::path wp_folder;
std::string         wp_description;

const std::string info_query {"/HPImageArchive.aspx?format=js&idx=0&n=1&mKT=en-CA"};

mlib::erc get_info(HINTERNET hconn)
{
  HINTERNET req;        //handle returned by HttpOpenRequest
  DWORD rdlen;
  const wchar_t* types[] = {L"*/*", 0};


  req = HttpOpenRequest(hconn, L"GET", utf8::widen(info_query).c_str(), 0, 0, types, INTERNET_FLAG_SECURE, 0);
  if (!req)
  {
    mlib::erc x(1);
    x.message(std::format("HttpOpenRequest failed (0x{:x})", GetLastError()));
    return x;
  }
  if (!HttpSendRequest(req, NULL, 0, NULL, 0))
  {
    InternetCloseHandle(req);
    mlib::erc x(2);
    x.message(std::format("HttpSendRequest failed (0x{:x})", GetLastError()));
    return x;
  }

  std::string response(4096, 0);

  if (!InternetReadFile(req, response.data(), (DWORD)(response.capacity() - 1), &rdlen))
  {
    InternetCloseHandle(req);
    TRACE("InternetReadFile failed (0x%x)", GetLastError());
    mlib::erc x(3);
    x.message(std::format("InternetReadFile failed (0x{:x})", GetLastError()));
    return x;
  }

  json::node info;
  info.read(response);

  img = info.at("images").at(0);
  auto json_fname = wp_folder / (img["startdate"].to_str() + ".json");
  if (!std::filesystem::exists(json_fname))
  {
    std::ofstream json_file(json_fname);
    json_file << json::indent << json::utf8 << img;
    json_file.close();
  }
  wp_description = img["copyright"].to_str();
  InternetCloseHandle(req);

  return mlib::erc::success;
}


mlib::erc get_image(HINTERNET hconn, const std::string& url, const std::filesystem::path& fname)
{
  HINTERNET req = NULL;       //handle returned by HttpOpenRequest
  FILE* jpg = NULL;           //output file
  DWORD rdlen;
  const wchar_t* types[] = {L"image/jpeg", 0};
  char response[4096];

  req = HttpOpenRequest(hconn, L"GET", utf8::widen(url).c_str(), 0, 0, types, INTERNET_FLAG_SECURE, 0);
  if (!req)
  {
    mlib::erc ret(1);
    ret.message(std::format("get_image - HttpOpenRequest failed (0x{:x})", GetLastError()));
    return ret;
  }

  if (!HttpSendRequest(req, NULL, 0, NULL, 0))
  {
    mlib::erc ret(1);
    ret.message(std::format("get_image - HttpSendRequest failed (0x{:x})", GetLastError()));
    InternetCloseHandle (req);
    return ret;
  }

  jpg = fopen(fname.generic_string().c_str(), "wb");
  if (!jpg)
  {
    mlib::erc ret(2);
    ret.message(std::format("get_image - Failed to open output file {}", strerror(errno)));
    InternetCloseHandle (req);
    return ret;
  }
  
  size_t count = 0;
  do
  {
    if (!InternetReadFile(req, response, sizeof(response), &rdlen))
    {
      mlib::erc ret(1);
      ret.message(std::format("get_image - InternetReadFile failed (0x{:x})", GetLastError()));
      InternetCloseHandle (req);
      fclose (jpg);
      remove (fname);
      return ret;
    }
    fwrite(response, sizeof(char), rdlen, jpg);
    count += rdlen;
  } while (rdlen);

  fclose (jpg);
  InternetCloseHandle(req);

  return mlib::erc::success;
}

void restore_wallpaper ()
{
  LPWSTR wwallpaper = NULL;
  HRESULT result = dwp->GetWallpaper (NULL, &wwallpaper);
  std::filesystem::path current_wallpaper;
  if (result == S_FALSE)
    syslog (LOG_INFO, "Cannot retrieve current wallpaper");
  else if (result != S_OK)
    syslog (LOG_NOTICE, "GetWallpaper error 0x%x", GetLastError ());
  else
  {
    current_wallpaper = wwallpaper;
  }

  std::filesystem::path our_wallpaper (wp_folder);
  our_wallpaper /= img["startdate"].to_str () + ".jpg";
  if (current_wallpaper != our_wallpaper)
  {
    dwp->SetWallpaper (NULL, our_wallpaper.c_str ());
    syslog (LOG_DEBUG, "Current wallpaper is %s", current_wallpaper.generic_string ().c_str ());
    syslog (LOG_INFO, "Changed wallpaper to %s", our_wallpaper.generic_string ().c_str ());
    if (nid.cbSize == sizeof (NOTIFYICONDATA))
    {
      nid.uFlags = NIF_MESSAGE | NIF_TIP;
      wcsncpy (nid.szTip, utf8::widen (wp_description).c_str (), _countof (nid.szTip)); // Copy tooltip
      Shell_NotifyIcon (NIM_MODIFY, &nid);
    }
  }
}

// Check current Bing wallpaper and download a new one if needed
mlib::erc update_wallpaper ()
{
  auto hconn = InternetConnect(hhttp, BING_SERVER, INTERNET_DEFAULT_HTTPS_PORT,
    NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
  if (!hconn)
  {
    syslog(LOG_ERR, "InternetConnect failed (0x%x)", GetLastError());
    mlib::erc ret(4);
    ret.message(std::format("InternetConnect failed (0x{:x})", GetLastError()));
    return ret;
  }

  get_info(hconn);

  std::filesystem::path new_wallpaper (wp_folder);
  new_wallpaper /= img["startdate"].to_str () + ".jpg";
  if (!std::filesystem::exists (new_wallpaper))
  {
    get_image (hconn, img["url"].to_str (), new_wallpaper);
    syslog (LOG_DEBUG, "Downloaded image %s", img["url"].to_str ().c_str ());
  }
  restore_wallpaper ();

  InternetCloseHandle(hconn);

  return mlib::erc::success;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  int wmId, wmEvent;
  static UINT_PTR update_timerId = 1, restore_timerId = 2;
  static HMENU menu = 0;
  try {
    switch (message)
    {
    case WM_CREATE:
      menu = LoadMenu (GetModuleHandle (NULL), MAKEINTRESOURCE (IDM_MENU));
      WTSRegisterSessionNotification (hWnd, NOTIFY_FOR_THIS_SESSION);
      update_timerId = SetTimer (hWnd, 1, 3600 * 1000, NULL);
      restore_timerId = SetTimer (hWnd, 2, 10 * 1000, NULL);
      break;

    case WM_TIMER:
      if (wParam == update_timerId)
      {
        syslog (LOG_INFO, "Checking for new wallpaper");
        update_wallpaper ();
      }
      else if (wParam == restore_timerId)
        restore_wallpaper ();
      break;

    case WM_REFRESH:
    case WM_THEMECHANGED:
      syslog (LOG_INFO, "Theme change event");
      Sleep (DELAY_MS);
      update_wallpaper ();
      break;

    case WM_WTSSESSION_CHANGE:
      syslog (LOG_INFO, "Session change event 0x%x", wParam);
      if (wParam == WTS_SESSION_LOGON
       || wParam == WTS_SESSION_UNLOCK)
      {
        Sleep (DELAY_MS);
        update_wallpaper ();
      }
      break;

    case WM_COMMAND:
      wmId = LOWORD (wParam);
      wmEvent = HIWORD (wParam);
      // Parse the menu selections:
      switch (wmId)
      {
      case ID_UPDATE:
        syslog (LOG_DEBUG, "Update command received", wParam);
        update_wallpaper ();
        break;

      case ID_ABOUT:
        utf8::MessageBox (hWnd,
          FDESC_STRING "\n\nVersion: " FVERS_STRING "\n" LEGAL_STRING,
          "Wallpaper", MB_ICONINFORMATION);
        //DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
        break;

      case ID_EXIT:
        DestroyWindow (hWnd);
        break;
      default:
        return DefWindowProc (hWnd, message, wParam, lParam);
      }
      break;

    case WM_TRAYNOTIFY:
      switch (lParam)
      {
      case WM_LBUTTONDOWN:
      case WM_LBUTTONDBLCLK:
        PostMessage (hWnd, WM_COMMAND, ID_UPDATE, 0l);
        break;

      case WM_RBUTTONDOWN:
      {
        POINT pt;
        GetCursorPos (&pt);
        SetForegroundWindow (hWnd);
        HMENU submenu = GetSubMenu (menu, 0);
        TrackPopupMenuEx (submenu,
          TPM_LEFTALIGN | TPM_LEFTBUTTON,
          pt.x, pt.y, hWnd, NULL);
        PostMessage (hWnd, WM_NULL, 0, 0); //per MS KB Q135788
        break;
      }
      }
      break;

    case WM_DESTROY:
      WTSUnRegisterSessionNotification (hWnd);
      DestroyMenu (menu);
      KillTimer (hWnd, update_timerId);
      KillTimer (hWnd, restore_timerId);

      nid.uFlags = NIF_ICON;
      Shell_NotifyIcon (NIM_DELETE, &nid);

      PostQuitMessage (0);
      break;

    default:
      return DefWindowProc (hWnd, message, wParam, lParam);
    }
  }
  catch (mlib::erc& x) {
    syslog (LOG_ERR, "Exception %d (%s)", (int)x, x.message ().c_str());
  }
  return 0;
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
  openlog("wallpaper");

  syslog(LOG_NOTICE,  "started...");
  HWND prevWnd;
  if ((prevWnd = FindWindow(WNDCLASSNAME, 0)))
  {
    TRACE(__FILE__ " - WinMain another instance is running");
    PostMessage(prevWnd, WM_REFRESH, 0, 0);
    syslog(LOG_NOTICE, "Found previous instance - exiting...");
    closelog();
    return 0;   //already running
  }
  
  if (S_OK != CoInitializeEx(NULL, NULL))
	{
    syslog(LOG_ERR, "CoInitialize failed");
		return 1;
	}

  //Folder for wallpapers
  wchar_t* wpath;
  KNOWNFOLDERID app_data {FOLDERID_LocalAppData};
  SHGetKnownFolderPath(app_data, 0, NULL, &wpath);
  wp_folder = wpath;
  wp_folder /= "bing_wallpaper";
  std::filesystem::create_directory(wp_folder); //make sure it exists

  HRESULT hr = CoCreateInstance(
		CLSID_DesktopWallpaper,
		NULL,
		CLSCTX_LOCAL_SERVER,
		__uuidof(IDesktopWallpaper),
		(void**)&dwp
	);

	if (hr != S_OK)
	{
    syslog(LOG_ERR, "Cannot create interface");
    closelog();
    return 1;
	}


  hhttp = InternetOpen(L"Wallpaper", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
  if (hhttp == 0)
  {
    syslog(LOG_ERR, "InternetOpen failed (0x%x)", GetLastError());
    dwp->Release();
    closelog();
    return 1;
  }

  //Register window class
  WNDCLASSEX wcex;
  memset(&wcex, 0, sizeof(WNDCLASSEX));
  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = WndProc;
  wcex.hInstance = hInstance;
  wcex.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_BING), IMAGE_ICON,
    ::GetSystemMetrics(SM_CXICON),
    ::GetSystemMetrics(SM_CYICON), 0);
  wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.lpszMenuName = MAKEINTRESOURCE(IDM_MENU);
  wcex.lpszClassName = WNDCLASSNAME;
  wcex.hIconSm = (HICON)LoadImage(wcex.hInstance, MAKEINTRESOURCE(IDI_BING), IMAGE_ICON,
    ::GetSystemMetrics(SM_CXSMICON),
    ::GetSystemMetrics(SM_CYSMICON), 0);
  if (!RegisterClassEx(&wcex))
  {
    syslog(LOG_ERR, "RegisterClassEx failed (%d)", GetLastError());
    return 1;
  }

  //Create main window
  mainWnd = CreateWindowEx(
    0,                        //exStyle
    WNDCLASSNAME,             //classname
    utf8::widen(app_title).c_str(),        //title
    WS_POPUP,                 //style
    CW_USEDEFAULT,            //X
    0,                        //Y
    CW_USEDEFAULT,            //W
    0,                        //H
    HWND_MESSAGE,             //parent
    NULL,                     //menu
    hInstance,                //instance
    NULL);                    //param
  if (!mainWnd)
  {
    syslog(LOG_ERR, "Failed to create main window (%d)", GetLastError());
    return 1;
  }

  memset (&nid, 0, sizeof (NOTIFYICONDATA));
  update_wallpaper();

  nid.cbSize = sizeof(NOTIFYICONDATA);
  nid.hIcon = wcex.hIconSm;
  nid.hWnd = mainWnd;
  nid.uCallbackMessage = WM_TRAYNOTIFY;
  //    nid.uID              = 1;
  nid.uVersion = NOTIFYICON_VERSION_4;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  wcsncpy(nid.szTip, utf8::widen(wp_description).c_str(), _countof(nid.szTip)); // Copy tooltip
  wcsncpy(nid.szInfoTitle, utf8::widen(app_title).c_str(), _countof(nid.szInfoTitle));
  Shell_NotifyIcon(NIM_ADD, &nid);
  TRACE("Init done");

  MSG msg;
  try {
    while (true)
    {
      while (GetMessage(&msg, NULL, 0, 0))
      {
        if (msg.message == WM_QUIT)
          break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
      if (msg.message == WM_QUIT)
        break;
    }
    syslog(LOG_ERR, "End of run loop msg %d", msg.message);
  }
  catch (mlib::erc& e)
  {
    syslog(LOG_ERR, "Error %s-%d", e.facility().name(), e.code());
  }

  Shell_NotifyIcon(NIM_DELETE, &nid);

  syslog(LOG_NOTICE, "Exiting. Result is %d.", msg.wParam);
  closelog();
  return (int)msg.wParam;

  dwp->Release();
  CoUninitialize();

	return 0;
}