#define UNICODE
#define _UNICODE
#define CINTERFACE
#define COBJMACROS

#include <windows.h>
#include <d3d9.h>
#include <WebView2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/*
 * The reproducer is intentionally written in C.  WebView2's header exposes
 * C COM vtables when CINTERFACE is defined, so the 32-bit MinGW C compiler is
 * sufficient; no C++ SDK or extra runtime is required.
 */

typedef struct App App;
typedef struct Surface Surface;

typedef struct EnvironmentHandler {
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl *lpVtbl;
    LONG refs;
    App *app;
} EnvironmentHandler;

typedef struct ControllerHandler {
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl *lpVtbl;
    LONG refs;
    Surface *surface;
} ControllerHandler;

typedef struct NavigationHandler {
    ICoreWebView2NavigationCompletedEventHandlerVtbl *lpVtbl;
    LONG refs;
    Surface *surface;
} NavigationHandler;

typedef HRESULT (WINAPI *create_environment_fn)(
    PCWSTR browserExecutableFolder,
    PCWSTR userDataFolder,
    ICoreWebView2EnvironmentOptions *environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *environmentCreatedHandler);

struct Vertex {
    float x, y, z, rhw;
    DWORD color;
};

#define VERTEX_FVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

#define GAME_SURFACE_COUNT 4
#define AD_SURFACE_INDEX GAME_SURFACE_COUNT
#define SURFACE_COUNT (GAME_SURFACE_COUNT + 1)
#define OBSERVED_HTML_WINDOW_COUNT 16

struct Surface {
    App *app;
    unsigned int index;
    BOOL is_ad;
    HWND controller_parent;
    ICoreWebView2Controller *controller;
    ICoreWebView2 *webview;
    EventRegistrationToken navigation_token;
    BOOL navigation_token_valid;
    BOOL navigation_reported;
    ControllerHandler controller_handler;
    NavigationHandler navigation_handler;
};

struct App {
    HINSTANCE instance;
    HWND frame_window;
    HWND main_window;
    HWND ad_window;
    HWND map_window;
    HMODULE loader;
    IDirect3D9 *d3d;
    IDirect3DDevice9 *device;
    D3DPRESENT_PARAMETERS present;
    ICoreWebView2Environment *environment;
    create_environment_fn create_environment;
    const wchar_t *loader_path;
    const wchar_t *user_data_path;
    const wchar_t *base_url;
    wchar_t surface_urls[SURFACE_COUNT][4096];
    Surface surfaces[SURFACE_COUNT];
    unsigned long frame;
    unsigned long layout_count;
    unsigned long tree_count;
    unsigned int navigations_completed;
    HWND observed_html_hwnds[OBSERVED_HTML_WINDOW_COUNT];
    wchar_t observed_html_titles[OBSERVED_HTML_WINDOW_COUNT][256];
    RECT observed_html_rects[OBSERVED_HTML_WINDOW_COUNT];
    BOOL observed_html_visible[OBSERVED_HTML_WINDOW_COUNT];
    unsigned int observed_html_count;
    HWND last_hit_window;
    BOOL have_hit_window;
    BOOL ad_transition_done;
    BOOL game_controllers_started;
    EnvironmentHandler environment_handler;
};

static const wchar_t frame_class[] = L"DreamSeekerReproducerFrame";
static const wchar_t main_class[] = L"DreamSeekerReproducerGameClient";
static const wchar_t ad_class[] = L"DreamSeekerReproducerAd";
static const wchar_t map_class[] = L"DreamSeekerD3D9MapViewport";

static void app_environment_ready(App *, HRESULT, ICoreWebView2Environment *);
static void app_controller_ready(Surface *, HRESULT, ICoreWebView2Controller *);
static void app_navigation_completed(Surface *, ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *);
static LRESULT CALLBACK frame_window_proc(HWND, UINT, WPARAM, LPARAM);

static HRESULT STDMETHODCALLTYPE environment_query(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *iface,
    REFIID iid, void **object)
{
    EnvironmentHandler *handler = (EnvironmentHandler *)iface;
    if (!object) return E_POINTER;
    *object = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) ||
        IsEqualIID(iid, &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler))
    {
        *object = iface;
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_AddRef(iface);
        return S_OK;
    }
    (void)handler;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE environment_add_ref(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *iface)
{
    EnvironmentHandler *handler = (EnvironmentHandler *)iface;
    return (ULONG)InterlockedIncrement(&handler->refs);
}

static ULONG STDMETHODCALLTYPE environment_release(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *iface)
{
    EnvironmentHandler *handler = (EnvironmentHandler *)iface;
    /* Handler storage belongs to App and intentionally lives until exit. */
    return (ULONG)InterlockedDecrement(&handler->refs);
}

static HRESULT STDMETHODCALLTYPE environment_invoke(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *iface,
    HRESULT error_code, ICoreWebView2Environment *environment)
{
    EnvironmentHandler *handler = (EnvironmentHandler *)iface;
    app_environment_ready(handler->app, error_code, environment);
    return S_OK;
}

static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl environment_vtbl = {
    environment_query, environment_add_ref, environment_release, environment_invoke
};

static HRESULT STDMETHODCALLTYPE controller_query(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *iface,
    REFIID iid, void **object)
{
    if (!object) return E_POINTER;
    *object = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) ||
        IsEqualIID(iid, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler))
    {
        *object = iface;
        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_AddRef(iface);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE controller_add_ref(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *iface)
{
    ControllerHandler *handler = (ControllerHandler *)iface;
    return (ULONG)InterlockedIncrement(&handler->refs);
}

static ULONG STDMETHODCALLTYPE controller_release(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *iface)
{
    ControllerHandler *handler = (ControllerHandler *)iface;
    return (ULONG)InterlockedDecrement(&handler->refs);
}

static HRESULT STDMETHODCALLTYPE controller_invoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *iface,
    HRESULT error_code, ICoreWebView2Controller *controller)
{
    ControllerHandler *handler = (ControllerHandler *)iface;
    app_controller_ready(handler->surface, error_code, controller);
    return S_OK;
}

static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl controller_vtbl = {
    controller_query, controller_add_ref, controller_release, controller_invoke
};

static HRESULT STDMETHODCALLTYPE navigation_query(
    ICoreWebView2NavigationCompletedEventHandler *iface,
    REFIID iid, void **object)
{
    if (!object) return E_POINTER;
    *object = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) ||
        IsEqualIID(iid, &IID_ICoreWebView2NavigationCompletedEventHandler))
    {
        *object = iface;
        ICoreWebView2NavigationCompletedEventHandler_AddRef(iface);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE navigation_add_ref(
    ICoreWebView2NavigationCompletedEventHandler *iface)
{
    NavigationHandler *handler = (NavigationHandler *)iface;
    return (ULONG)InterlockedIncrement(&handler->refs);
}

static ULONG STDMETHODCALLTYPE navigation_release(
    ICoreWebView2NavigationCompletedEventHandler *iface)
{
    NavigationHandler *handler = (NavigationHandler *)iface;
    return (ULONG)InterlockedDecrement(&handler->refs);
}

static HRESULT STDMETHODCALLTYPE navigation_invoke(
    ICoreWebView2NavigationCompletedEventHandler *iface,
    ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args)
{
    NavigationHandler *handler = (NavigationHandler *)iface;
    app_navigation_completed(handler->surface, sender, args);
    return S_OK;
}

static ICoreWebView2NavigationCompletedEventHandlerVtbl navigation_vtbl = {
    navigation_query, navigation_add_ref, navigation_release, navigation_invoke
};

static void log_hresult(const char *operation, HRESULT result)
{
    printf("[repro] %-48s -> 0x%08lx\n", operation, (unsigned long)result);
    fflush(stdout);
}

static void log_message(const char *message)
{
    printf("[repro] %s\n", message);
    fflush(stdout);
}

static const char *bool_text(BOOL value)
{
    return value ? "yes" : "no";
}

static void narrow_wide(const wchar_t *source, char *target, int target_size)
{
    if (!WideCharToMultiByte(CP_UTF8, 0, source, -1, target, target_size, "?", NULL))
        lstrcpynA(target, "?", target_size);
}

static void dump_window_tree_node(HWND window, int depth, unsigned long tree_count)
{
    wchar_t class_name[128];
    wchar_t title[160];
    char class_utf8[384];
    char title_utf8[480];
    RECT rect;
    DWORD pid = 0;
    HWND child;
    LONG style;
    LONG exstyle;
    BOOL visible;

    if (!window || depth > 6) return;
    class_name[0] = 0;
    title[0] = 0;
    GetClassNameW(window, class_name, sizeof(class_name) / sizeof(class_name[0]));
    GetWindowTextW(window, title, sizeof(title) / sizeof(title[0]));
    GetWindowRect(window, &rect);
    GetWindowThreadProcessId(window, &pid);
    style = GetWindowLongW(window, GWL_STYLE);
    exstyle = GetWindowLongW(window, GWL_EXSTYLE);
    visible = IsWindowVisible(window);
    narrow_wide(class_name, class_utf8, sizeof(class_utf8));
    narrow_wide(title, title_utf8, sizeof(title_utf8));

    printf("[tree %lu] %*s hwnd=%p pid=%lu visible=%s parent=%p style=%08lx "
           "exstyle=%08lx class=%s title=%s rect=%ld,%ld,%ld,%ld\n",
           tree_count, depth * 2, "", window, (unsigned long)pid, bool_text(visible),
           GetParent(window), (unsigned long)style, (unsigned long)exstyle,
           class_utf8, title_utf8, rect.left, rect.top, rect.right, rect.bottom);

    child = GetWindow(window, GW_CHILD);
    while (child)
    {
        dump_window_tree_node(child, depth + 1, tree_count);
        child = GetWindow(child, GW_HWNDNEXT);
    }
}

static void dump_window_tree(App *app)
{
    ++app->tree_count;
    printf("[tree %lu] ---- Win32 tree for DreamSeeker-like host ----\n", app->tree_count);
    dump_window_tree_node(app->main_window, 0, app->tree_count);
    fflush(stdout);
}

/*
 * The old diagnostic printed the whole recursive HWND tree on a timer.  That
 * made the log enormous while still hiding the one fact we need for this
 * test: did a real WebView2 child receive input and change its title?
 *
 * Poll only the browser-owned document windows and print a line when their
 * title, visibility, or screen rectangle changes.  The pages use their
 * title as a small, non-visual pointer-move counter, so this observes real
 * WebView2 input without manufacturing a flicker.
 */
static int observed_html_index(App *app, HWND window)
{
    unsigned int index;
    for (index = 0; index < app->observed_html_count; ++index)
        if (app->observed_html_hwnds[index] == window)
            return (int)index;
    return -1;
}

static BOOL CALLBACK observe_html_window(HWND window, LPARAM lparam)
{
    App *app = (App *)lparam;
    wchar_t class_name[128];
    wchar_t title[256];
    char title_utf8[768];
    RECT rect;
    DWORD pid = 0;
    BOOL visible;
    int existing_index;
    unsigned int index;
    BOOL changed;

    class_name[0] = 0;
    title[0] = 0;
    GetClassNameW(window, class_name, sizeof(class_name) / sizeof(class_name[0]));
    if (wcscmp(class_name, L"Chrome_WidgetWin_1") != 0)
        return TRUE;

    GetWindowTextW(window, title, sizeof(title) / sizeof(title[0]));
    GetWindowRect(window, &rect);
    GetWindowThreadProcessId(window, &pid);
    visible = IsWindowVisible(window);
    existing_index = observed_html_index(app, window);

    if (existing_index < 0)
    {
        if (app->observed_html_count >= OBSERVED_HTML_WINDOW_COUNT)
            return TRUE;
        index = app->observed_html_count++;
        app->observed_html_hwnds[index] = window;
        app->observed_html_titles[index][0] = 0;
        SetRectEmpty(&app->observed_html_rects[index]);
        app->observed_html_visible[index] = FALSE;
    }
    else
    {
        index = (unsigned int)existing_index;
    }

    changed = wcscmp(app->observed_html_titles[index], title) != 0 ||
              !EqualRect(&app->observed_html_rects[index], &rect) ||
              app->observed_html_visible[index] != visible;
    if (changed)
    {
        narrow_wide(title, title_utf8, sizeof(title_utf8));
        printf("[html-state] hwnd=%p pid=%lu visible=%s title=%s rect=%ld,%ld,%ld,%ld\n",
               window, (unsigned long)pid, bool_text(visible), title_utf8,
               rect.left, rect.top, rect.right, rect.bottom);
        fflush(stdout);
    }

    lstrcpynW(app->observed_html_titles[index], title,
              sizeof(app->observed_html_titles[index]) / sizeof(wchar_t));
    app->observed_html_rects[index] = rect;
    app->observed_html_visible[index] = visible;
    return TRUE;
}

static void poll_html_state(App *app)
{
    if (app->main_window)
        EnumChildWindows(app->main_window, observe_html_window, (LPARAM)app);
}

/*
 * This is an input-routing observation, not input synthesis.  WindowFromPoint
 * asks the current window system which native HWND is under the real mouse
 * cursor.  Comparing that result with the browser HWNDs above distinguishes
 * "the WebView2 page did not receive pointermove" from "the pointer never
 * reached the WebView2 surface".
 */
static void poll_hit_test(App *app)
{
    POINT cursor;
    HWND window;
    wchar_t class_name[128];
    wchar_t title[256];
    char class_utf8[384];
    char title_utf8[768];
    DWORD pid = 0;

    if (!GetCursorPos(&cursor)) return;
    window = WindowFromPoint(cursor);
    if (app->have_hit_window && app->last_hit_window == window)
        return;

    class_name[0] = 0;
    title[0] = 0;
    if (window)
    {
        GetClassNameW(window, class_name, sizeof(class_name) / sizeof(class_name[0]));
        GetWindowTextW(window, title, sizeof(title) / sizeof(title[0]));
        GetWindowThreadProcessId(window, &pid);
    }
    narrow_wide(class_name, class_utf8, sizeof(class_utf8));
    narrow_wide(title, title_utf8, sizeof(title_utf8));
    printf("[hit-test] cursor=%ld,%ld hwnd=%p pid=%lu class=%s title=%s parent=%p\n",
           cursor.x, cursor.y, window, (unsigned long)pid, class_utf8,
           title_utf8, window ? GetParent(window) : NULL);
    fflush(stdout);
    app->last_hit_window = window;
    app->have_hit_window = TRUE;
}

/*
 * WebView2 creates its native controller container asynchronously.  Keep the
 * D3D9 map below those containers in the host's child-window z-order once the
 * containers exist.  This is ordinary Win32 child ordering; it is not an
 * always-on-top window and it does not synthesize input.
 */
static BOOL CALLBACK raise_webview_container(HWND window, LPARAM lparam)
{
    wchar_t class_name[128];
    (void)lparam;
    class_name[0] = 0;
    GetClassNameW(window, class_name, sizeof(class_name) / sizeof(class_name[0]));
    if (wcscmp(class_name, L"Chrome_WidgetWin_0") == 0)
        SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    return TRUE;
}

static BOOL CALLBACK find_login_container(HWND window, LPARAM lparam)
{
    HWND *login_window = (HWND *)lparam;
    wchar_t class_name[128];
    RECT rect;

    class_name[0] = 0;
    GetClassNameW(window, class_name, sizeof(class_name) / sizeof(class_name[0]));
    if (wcscmp(class_name, L"Chrome_WidgetWin_0") != 0) return TRUE;
    if (!GetWindowRect(window, &rect)) return TRUE;
    if (rect.right - rect.left == 1258 && rect.bottom - rect.top == 696)
    {
        *login_window = window;
        return FALSE;
    }
    return TRUE;
}

static void raise_webview_containers(App *app)
{
    HWND login_window = NULL;

    if (!app->main_window) return;
    EnumChildWindows(app->main_window, raise_webview_container, (LPARAM)app);
    /* The real tree places the centered login surface above the full-size
     * document surface.  The first pass raises every browser container; put
     * the 1258x696 login container last so the manual hover test reaches B. */
    EnumChildWindows(app->main_window, find_login_container, (LPARAM)&login_window);
    if (login_window)
        SetWindowPos(login_window, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

static LRESULT CALLBACK map_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    (void)window;
    (void)wparam;
    (void)lparam;
    if (message == WM_ERASEBKGND) return 1;
    return DefWindowProcW(window, message, wparam, lparam);
}

static void layout_webviews(App *app);
static void render_map(App *app);
static void resize_host(App *app);
static void finish_ad_transition(App *app);

static LRESULT CALLBACK frame_window_proc(HWND window, UINT message,
                                          WPARAM wparam, LPARAM lparam)
{
    App *app = (App *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
        app = (App *)create->lpCreateParams;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)app);
        app->frame_window = window;
    }
    if (!app) return DefWindowProcW(window, message, wparam, lparam);

    if (message == WM_SIZE && app->main_window)
    {
        RECT client;
        GetClientRect(window, &client);
        SetWindowPos(app->main_window, HWND_TOP,
                     0, 0, client.right, client.bottom,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        return 0;
    }
    if (message == WM_CLOSE)
    {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    if (message == WM_ERASEBKGND) return 1;
    return DefWindowProcW(window, message, wparam, lparam);
}

static LRESULT CALLBACK main_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    App *app = (App *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW *create = (CREATESTRUCTW *)lparam;
        app = (App *)create->lpCreateParams;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)app);
        app->main_window = window;
    }
    if (!app) return DefWindowProcW(window, message, wparam, lparam);

    switch (message)
    {
    case WM_SIZE:
        resize_host(app);
        return 0;
    case WM_TIMER:
        if (wparam == 1) render_map(app);
        if (wparam == 2)
        {
            poll_html_state(app);
            poll_hit_test(app);
        }
        if (wparam == 3) finish_ad_transition(app);
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static BOOL register_classes(App *app)
{
    WNDCLASSW frame_window_class;
    WNDCLASSW main_window_class;
    WNDCLASSW ad_window_class;
    WNDCLASSW map_window_class;
    ZeroMemory(&frame_window_class, sizeof(frame_window_class));
    ZeroMemory(&main_window_class, sizeof(main_window_class));
    ZeroMemory(&ad_window_class, sizeof(ad_window_class));
    ZeroMemory(&map_window_class, sizeof(map_window_class));

    frame_window_class.lpfnWndProc = frame_window_proc;
    frame_window_class.hInstance = app->instance;
    frame_window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    frame_window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    frame_window_class.lpszClassName = frame_class;

    main_window_class.lpfnWndProc = main_window_proc;
    main_window_class.hInstance = app->instance;
    main_window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    main_window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    main_window_class.lpszClassName = main_class;

    ad_window_class.lpfnWndProc = DefWindowProcW;
    ad_window_class.hInstance = app->instance;
    ad_window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    ad_window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    ad_window_class.lpszClassName = ad_class;

    map_window_class.lpfnWndProc = map_window_proc;
    map_window_class.hInstance = app->instance;
    map_window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    map_window_class.lpszClassName = map_class;

    if (!RegisterClassW(&frame_window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return FALSE;
    if (!RegisterClassW(&main_window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return FALSE;
    if (!RegisterClassW(&ad_window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return FALSE;
    if (!RegisterClassW(&map_window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return FALSE;
    return TRUE;
}

static BOOL create_host_windows(App *app)
{
    RECT window_rect = {0, 0, 1366, 696};
    RECT ad_rect = {0, 0, 706, 582};
    AdjustWindowRectEx(&window_rect, WS_OVERLAPPEDWINDOW, FALSE, 0);
    AdjustWindowRectEx(&ad_rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

    app->frame_window = CreateWindowExW(
        0, frame_class,
        L"STABLE D3D9 MAP | WebView2 HTML surface hover test",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        NULL, NULL, app->instance, app);
    if (!app->frame_window)
    {
        log_message("CreateWindowExW(frame) failed");
        return FALSE;
    }

    app->main_window = CreateWindowExW(
        0, main_class,
        L"DreamSeeker game client",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_TABSTOP,
        0, 0, 1366, 696,
        app->frame_window, NULL, app->instance, app);
    if (!app->main_window)
    {
        log_message("CreateWindowExW(game client) failed");
        return FALSE;
    }

    app->map_window = CreateWindowExW(
        0, map_class, L"D3D9 map viewport",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 1366, 696, app->main_window, NULL, app->instance, NULL);
    if (!app->map_window)
    {
        log_message("CreateWindowExW(map) failed");
        return FALSE;
    }

    app->ad_window = CreateWindowExW(
        0, ad_class,
        L"BYOND: Your Game Is Starting",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        ad_rect.right - ad_rect.left,
        ad_rect.bottom - ad_rect.top,
        NULL, NULL, app->instance, NULL);
    if (!app->ad_window)
    {
        log_message("CreateWindowExW(ad) failed");
        return FALSE;
    }

    ShowWindow(app->main_window, SW_SHOW);
    UpdateWindow(app->main_window);
    ShowWindow(app->frame_window, SW_SHOW);
    UpdateWindow(app->frame_window);
    ShowWindow(app->ad_window, SW_SHOWNA);
    UpdateWindow(app->ad_window);
    SetTimer(app->main_window, 1, 16, NULL);
    SetTimer(app->main_window, 2, 500, NULL);
    return TRUE;
}

static BOOL create_d3d_device(App *app)
{
    HRESULT result;

    app->d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!app->d3d)
    {
        log_message("Direct3DCreate9 failed");
        return FALSE;
    }

    ZeroMemory(&app->present, sizeof(app->present));
    app->present.Windowed = TRUE;
    app->present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    app->present.hDeviceWindow = app->map_window;
    app->present.BackBufferFormat = D3DFMT_X8R8G8B8;
    app->present.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    result = IDirect3D9_CreateDevice(
        app->d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, app->map_window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &app->present, &app->device);
    log_hresult("IDirect3D9::CreateDevice", result);
    if (FAILED(result)) return FALSE;

    IDirect3DDevice9_SetRenderState(app->device, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(app->device, D3DRS_LIGHTING, FALSE);
    return TRUE;
}

static void reset_d3d_device(App *app)
{
    RECT rect;
    HRESULT result;
    if (!app->device) return;
    GetClientRect(app->map_window, &rect);
    app->present.BackBufferWidth = rect.right - rect.left;
    app->present.BackBufferHeight = rect.bottom - rect.top;
    if (!app->present.BackBufferWidth || !app->present.BackBufferHeight) return;
    result = IDirect3DDevice9_Reset(app->device, &app->present);
    if (FAILED(result)) log_hresult("IDirect3DDevice9::Reset", result);
}

static void draw_quad(App *app, float left, float top, float right, float bottom, DWORD color)
{
    struct Vertex vertices[6];
    memset(vertices, 0, sizeof(vertices));
    vertices[0].x = left;  vertices[0].y = top;    vertices[0].z = 0.0f; vertices[0].rhw = 1.0f; vertices[0].color = color;
    vertices[1].x = right; vertices[1].y = top;    vertices[1].z = 0.0f; vertices[1].rhw = 1.0f; vertices[1].color = color;
    vertices[2].x = right; vertices[2].y = bottom; vertices[2].z = 0.0f; vertices[2].rhw = 1.0f; vertices[2].color = color;
    vertices[3] = vertices[0];
    vertices[4] = vertices[2];
    vertices[5].x = left;  vertices[5].y = bottom; vertices[5].z = 0.0f; vertices[5].rhw = 1.0f; vertices[5].color = color;
    IDirect3DDevice9_DrawPrimitiveUP(app->device, D3DPT_TRIANGLELIST, 2, vertices, sizeof(vertices[0]));
}

static void render_map(App *app)
{
    RECT rect;
    float width;
    float height;
    float phase;
    float band;
    HRESULT result;
    int x;
    int y;

    if (!app->device) return;
    result = IDirect3DDevice9_TestCooperativeLevel(app->device);
    if (result == D3DERR_DEVICELOST) return;
    if (result == D3DERR_DEVICENOTRESET)
    {
        reset_d3d_device(app);
        return;
    }

    GetClientRect(app->map_window, &rect);
    width = (float)(rect.right - rect.left);
    height = (float)(rect.bottom - rect.top);
    if (width <= 0.0f || height <= 0.0f) return;

    IDirect3DDevice9_Clear(app->device, 0, NULL, D3DCLEAR_TARGET,
                           D3DCOLOR_XRGB(18, 32, 52), 1.0f, 0);
    if (FAILED(IDirect3DDevice9_BeginScene(app->device))) return;

    IDirect3DDevice9_SetFVF(app->device, VERTEX_FVF);
    phase = (float)(app->frame % 240) / 240.0f;
    for (y = 0; y < 7; ++y)
    {
        for (x = 0; x < 13; ++x)
        {
            float left = (float)x * width / 13.0f;
            float top = (float)y * height / 7.0f;
            float right = (float)(x + 1) * width / 13.0f + 1.0f;
            float bottom = (float)(y + 1) * height / 7.0f + 1.0f;
            BYTE red = (BYTE)(22 + ((x * 7 + y * 3) % 35));
            BYTE green = (BYTE)(62 + ((y * 11 + x * 2) % 55));
            BYTE blue = (BYTE)(80 + ((x * 13 + y * 5 + (int)(phase * 20)) % 80));
            draw_quad(app, left, top, right, bottom, D3DCOLOR_XRGB(red, green, blue));
        }
    }

    band = (phase * width) - width * 0.25f;
    draw_quad(app, band, 0.0f, band + width * 0.08f, height,
              D3DCOLOR_ARGB(55, 240, 220, 120));
    draw_quad(app, width - band, 0.0f, width - band + width * 0.04f, height,
              D3DCOLOR_ARGB(35, 120, 210, 255));

    IDirect3DDevice9_EndScene(app->device);
    IDirect3DDevice9_Present(app->device, NULL, NULL, NULL, NULL);
    ++app->frame;
}

static void resize_host(App *app)
{
    RECT rect;
    int width;
    int height;
    if (!app->main_window) return;
    GetClientRect(app->main_window, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
    SetWindowPos(app->map_window, HWND_BOTTOM, 0, 0, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    reset_d3d_device(app);
    layout_webviews(app);
}

static const char *surface_name(unsigned int index)
{
    static const char *names[SURFACE_COUNT] = {
        "main-ui/a0284aed.htm",
        "login/e33cfbfa.htm",
        "hidden/b4175bf4.htm",
        "icon/bdc319d8.htm",
        "ad/ad.htm"
    };
    return index < SURFACE_COUNT ? names[index] : "unknown";
}

static const wchar_t *surface_path(unsigned int index)
{
    static const wchar_t *paths[SURFACE_COUNT] = {
        L"a0284aed.htm",
        L"e33cfbfa.htm",
        L"b4175bf4.htm",
        L"bdc319d8.htm",
        L"ad.htm"
    };
    return index < SURFACE_COUNT ? paths[index] : L"unknown.htm";
}

static void surface_bounds(App *app, Surface *surface, RECT *bounds)
{
    RECT client;
    int width;
    int height;

    GetClientRect(surface->is_ad ? app->ad_window : app->main_window, &client);
    width = client.right - client.left;
    height = client.bottom - client.top;
    SetRectEmpty(bounds);

    if (surface->is_ad)
    {
        bounds->right = width;
        bounds->bottom = height;
        return;
    }

    switch (surface->index)
    {
    case 0:
        bounds->right = width;
        bounds->bottom = height;
        break;
    case 1:
        /* SOA's login document is 1258x696 over a 1366x696 game client. */
        bounds->left = width > 1258 ? (width - 1258) / 2 : 0;
        bounds->right = width > 1258 ? bounds->left + 1258 : width;
        bounds->bottom = height > 696 ? 696 : height;
        break;
    case 2:
        bounds->right = 1;
        bounds->bottom = 1;
        break;
    case 3:
        bounds->right = 24;
        bounds->bottom = 24;
        break;
    }
}

static void layout_surface(App *app, Surface *surface)
{
    HRESULT result;
    RECT bounds;
    char operation[160];

    if (!surface->controller || !app->main_window) return;
    surface_bounds(app, surface, &bounds);
    result = ICoreWebView2Controller_SetBoundsAndZoomFactor(
        surface->controller, bounds, 1.0);
    if (app->layout_count < 16 || FAILED(result))
    {
        snprintf(operation, sizeof(operation),
                 "surface[%u] %s SetBoundsAndZoomFactor",
                 surface->index, surface_name(surface->index));
        log_hresult(operation, result);
    }
    result = ICoreWebView2Controller_NotifyParentWindowPositionChanged(
        surface->controller);
    if (app->layout_count < 16 || FAILED(result))
    {
        snprintf(operation, sizeof(operation),
                 "surface[%u] %s NotifyParentWindowPositionChanged",
                 surface->index, surface_name(surface->index));
        log_hresult(operation, result);
    }
}

static void layout_webviews(App *app)
{
    unsigned int index;
    ++app->layout_count;
    for (index = 0; index < SURFACE_COUNT; ++index)
        layout_surface(app, &app->surfaces[index]);
}

static BOOL start_webview(App *app)
{
    HRESULT result;
    app->loader = LoadLibraryW(app->loader_path && app->loader_path[0]
                               ? app->loader_path : L"WebView2Loader.dll");
    if (!app->loader)
    {
        log_message("LoadLibraryW(WebView2Loader.dll) failed");
        return FALSE;
    }
    app->create_environment = (create_environment_fn)GetProcAddress(
        app->loader, "CreateCoreWebView2EnvironmentWithOptions");
    if (!app->create_environment)
    {
        log_message("CreateCoreWebView2EnvironmentWithOptions was not exported");
        return FALSE;
    }
    result = app->create_environment(NULL, app->user_data_path, NULL,
        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)&app->environment_handler);
    log_hresult("CreateCoreWebView2EnvironmentWithOptions", result);
    return SUCCEEDED(result);
}

static void initialize_surfaces(App *app)
{
    unsigned int index;
    for (index = 0; index < SURFACE_COUNT; ++index)
    {
        Surface *surface = &app->surfaces[index];
        surface->app = app;
        surface->index = index;
        surface->is_ad = index == AD_SURFACE_INDEX;
        surface->controller_parent = NULL;
        surface->controller_handler.lpVtbl = &controller_vtbl;
        surface->controller_handler.refs = 1;
        surface->controller_handler.surface = surface;
        surface->navigation_handler.lpVtbl = &navigation_vtbl;
        surface->navigation_handler.refs = 1;
        surface->navigation_handler.surface = surface;
        surface->navigation_reported = FALSE;
        lstrcpynW(app->surface_urls[index], app->base_url,
                  sizeof(app->surface_urls[index]) / sizeof(wchar_t));
        lstrcatW(app->surface_urls[index], surface_path(index));
    }
}

static BOOL app_start(App *app, const wchar_t *loader_path,
                      const wchar_t *user_data_path, const wchar_t *base_url)
{
    app->loader_path = loader_path;
    app->user_data_path = user_data_path;
    app->base_url = base_url;
    initialize_surfaces(app);
    if (!register_classes(app)) return FALSE;
    if (!create_host_windows(app)) return FALSE;
    if (!create_d3d_device(app)) return FALSE;
    resize_host(app);
    dump_window_tree(app);
    return start_webview(app);
}

static void create_game_controllers(App *app)
{
    unsigned int index;

    if (app->game_controllers_started || !app->environment) return;
    app->game_controllers_started = TRUE;
    printf("[repro] creating four game WebView2 controllers after ad transition\n");
    fflush(stdout);
    for (index = 0; index < GAME_SURFACE_COUNT; ++index)
    {
        Surface *surface = &app->surfaces[index];
        HRESULT create_result;
        char operation[160];

        surface->controller_parent = GetDesktopWindow();
        create_result = ICoreWebView2Environment_CreateCoreWebView2Controller(
            app->environment, surface->controller_parent,
            (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *)
                &surface->controller_handler);
        snprintf(operation, sizeof(operation),
                 "surface[%u] %s CreateCoreWebView2Controller",
                 index, surface_name(index));
        log_hresult(operation, create_result);
    }
}

static void app_environment_ready(App *app, HRESULT result,
                                  ICoreWebView2Environment *environment)
{
    Surface *surface = &app->surfaces[AD_SURFACE_INDEX];
    HRESULT create_result;
    char operation[160];

    log_hresult("environment callback", result);
    if (FAILED(result) || !environment) return;
    app->environment = environment;
    ICoreWebView2Environment_AddRef(environment);

    surface->controller_parent = app->ad_window;
    create_result = ICoreWebView2Environment_CreateCoreWebView2Controller(
        environment, surface->controller_parent,
        (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *)
            &surface->controller_handler);
    snprintf(operation, sizeof(operation),
             "surface[%u] %s CreateCoreWebView2Controller",
             AD_SURFACE_INDEX, surface_name(AD_SURFACE_INDEX));
    log_hresult(operation, create_result);
}

static void app_controller_ready(Surface *surface, HRESULT result,
                                 ICoreWebView2Controller *controller)
{
    App *app = surface->app;
    HWND parent = NULL;
    ICoreWebView2Controller2 *controller2 = NULL;
    HRESULT operation;
    char operation_name[192];

    snprintf(operation_name, sizeof(operation_name),
             "surface[%u] %s controller callback",
             surface->index, surface_name(surface->index));
    log_hresult(operation_name, result);
    if (FAILED(result) || !controller) return;
    surface->controller = controller;
    ICoreWebView2Controller_AddRef(controller);

    operation = ICoreWebView2Controller_get_ParentWindow(controller, &parent);
    snprintf(operation_name, sizeof(operation_name),
             "surface[%u] get_ParentWindow", surface->index);
    log_hresult(operation_name, operation);
    printf("[repro] surface[%u] %s initial_parent=%p expected_parent=%p\n",
           surface->index, surface_name(surface->index), parent,
           surface->is_ad ? app->ad_window : GetDesktopWindow());

    if (!surface->is_ad)
    {
        operation = ICoreWebView2Controller_put_ParentWindow(
            controller, app->main_window);
        snprintf(operation_name, sizeof(operation_name),
                 "surface[%u] put_ParentWindow(game)", surface->index);
        log_hresult(operation_name, operation);
        if (FAILED(operation)) return;
    }

    parent = NULL;
    operation = ICoreWebView2Controller_get_ParentWindow(controller, &parent);
    snprintf(operation_name, sizeof(operation_name),
             "surface[%u] get_ParentWindow(after reparent)", surface->index);
    log_hresult(operation_name, operation);
    printf("[repro] surface[%u] %s final_parent=%p expected %s=%p\n",
           surface->index, surface_name(surface->index), parent,
           surface->is_ad ? "ad" : "game",
           surface->is_ad ? app->ad_window : app->main_window);

    operation = ICoreWebView2Controller_QueryInterface(
        controller, &IID_ICoreWebView2Controller2, (void **)&controller2);
    snprintf(operation_name, sizeof(operation_name),
             "surface[%u] QueryInterface(ICoreWebView2Controller2)", surface->index);
    log_hresult(operation_name, operation);
    if (SUCCEEDED(operation) && controller2)
    {
        COREWEBVIEW2_COLOR transparent = {0, 0, 0, 0};
        operation = ICoreWebView2Controller2_put_DefaultBackgroundColor(
            controller2, transparent);
        snprintf(operation_name, sizeof(operation_name),
                 "surface[%u] put_DefaultBackgroundColor(A=0)", surface->index);
        log_hresult(operation_name, operation);
        ICoreWebView2Controller2_Release(controller2);
    }

    layout_surface(app, surface);
    if (!surface->is_ad) raise_webview_containers(app);
    operation = ICoreWebView2Controller_put_IsVisible(controller, TRUE);
    snprintf(operation_name, sizeof(operation_name),
             "surface[%u] put_IsVisible(TRUE)", surface->index);
    log_hresult(operation_name, operation);
    operation = ICoreWebView2Controller_get_CoreWebView2(
        controller, &surface->webview);
    snprintf(operation_name, sizeof(operation_name),
             "surface[%u] get_CoreWebView2", surface->index);
    log_hresult(operation_name, operation);
    if (FAILED(operation) || !surface->webview) return;

    operation = ICoreWebView2_add_NavigationCompleted(
        surface->webview,
        (ICoreWebView2NavigationCompletedEventHandler *)&surface->navigation_handler,
        &surface->navigation_token);
    surface->navigation_token_valid = SUCCEEDED(operation);
    snprintf(operation_name, sizeof(operation_name),
             "surface[%u] add_NavigationCompleted", surface->index);
    log_hresult(operation_name, operation);
    operation = ICoreWebView2_Navigate(
        surface->webview, app->surface_urls[surface->index]);
    snprintf(operation_name, sizeof(operation_name),
             "surface[%u] Navigate(127.0.0.1 %s)",
             surface->index, surface_name(surface->index));
    log_hresult(operation_name, operation);
}

static void app_navigation_completed(Surface *surface, ICoreWebView2 *sender,
                                     ICoreWebView2NavigationCompletedEventArgs *args)
{
    App *app = surface->app;
    BOOL success = FALSE;
    HRESULT result;
    char operation_name[192];
    (void)sender;
    result = args ? ICoreWebView2NavigationCompletedEventArgs_get_IsSuccess(args, &success) : E_POINTER;
    snprintf(operation_name, sizeof(operation_name),
             "surface[%u] NavigationCompleted::get_IsSuccess", surface->index);
    log_hresult(operation_name, result);
    printf("[repro] surface[%u] %s navigation success=%s\n",
           surface->index, surface_name(surface->index), bool_text(success));
    if (surface->is_ad && success && !app->ad_transition_done)
    {
        printf("[repro] ad navigation complete; hiding ad surface after 1500 ms\n");
        fflush(stdout);
        SetTimer(app->main_window, 3, 1500, NULL);
    }
    printf("[repro] surface[%u] post-navigation layout begin\n", surface->index);
    fflush(stdout);
    layout_surface(app, surface);
    printf("[repro] surface[%u] post-navigation layout end\n", surface->index);
    if (!surface->navigation_reported)
    {
        surface->navigation_reported = TRUE;
        ++app->navigations_completed;
    }
    printf("[repro] navigation surfaces_seen=%u/%u\n",
           app->navigations_completed, SURFACE_COUNT);
    if (app->navigations_completed == SURFACE_COUNT)
    {
        printf("[repro] all %u WebView2 surfaces completed navigation;\n",
               SURFACE_COUNT);
        printf("[repro] subsequent diagnostics report only HTML state changes.\n");
        fflush(stdout);
    }
}

static void finish_ad_transition(App *app)
{
    Surface *surface = &app->surfaces[AD_SURFACE_INDEX];
    HRESULT result;

    if (app->ad_transition_done) return;
    KillTimer(app->main_window, 3);
    if (surface->controller)
    {
        result = ICoreWebView2Controller_put_IsVisible(
            surface->controller, FALSE);
        log_hresult("ad put_IsVisible(FALSE)", result);
    }
    if (app->ad_window) ShowWindow(app->ad_window, SW_HIDE);
    app->ad_transition_done = TRUE;
    printf("[repro] ad surface hidden; controller and renderer remain alive\n");
    fflush(stdout);
    create_game_controllers(app);
}

static void app_stop(App *app)
{
    unsigned int index;
    if (app->main_window)
    {
        KillTimer(app->main_window, 1);
        KillTimer(app->main_window, 2);
        KillTimer(app->main_window, 3);
    }
    for (index = 0; index < SURFACE_COUNT; ++index)
    {
        Surface *surface = &app->surfaces[index];
        if (surface->webview && surface->navigation_token_valid)
            ICoreWebView2_remove_NavigationCompleted(
                surface->webview, surface->navigation_token);
        if (surface->controller)
            ICoreWebView2Controller_Close(surface->controller);
        if (surface->webview)
            ICoreWebView2_Release(surface->webview);
        if (surface->controller)
            ICoreWebView2Controller_Release(surface->controller);
        surface->webview = NULL;
        surface->controller = NULL;
        surface->navigation_token_valid = FALSE;
        surface->navigation_reported = FALSE;
    }
    if (app->environment) ICoreWebView2Environment_Release(app->environment);
    if (app->ad_window) DestroyWindow(app->ad_window);
    if (app->frame_window) DestroyWindow(app->frame_window);
    if (app->device) IDirect3DDevice9_Release(app->device);
    if (app->d3d) IDirect3D9_Release(app->d3d);
    if (app->loader) FreeLibrary(app->loader);
    app->environment = NULL;
    app->frame_window = NULL;
    app->main_window = NULL;
    app->ad_window = NULL;
    app->device = NULL;
    app->d3d = NULL;
    app->loader = NULL;
}

static BOOL argument_to_wide(const char *source, wchar_t *target, int target_size)
{
    return MultiByteToWideChar(CP_UTF8, 0, source, -1, target, target_size) != 0;
}

int main(int argc, char **argv)
{
    wchar_t loader_path[2048];
    wchar_t user_data_path[4096];
    wchar_t base_url[4096];
    MSG message;
    HRESULT result;
    App app;

    if (argc < 4)
    {
        printf("usage: dreamseeker-webview2.exe LOADER_DLL USER_DATA_DIR BASE_URL\n");
        return 2;
    }
    if (!argument_to_wide(argv[1], loader_path, sizeof(loader_path) / sizeof(loader_path[0])) ||
        !argument_to_wide(argv[2], user_data_path, sizeof(user_data_path) / sizeof(user_data_path[0])) ||
        !argument_to_wide(argv[3], base_url, sizeof(base_url) / sizeof(base_url[0])))
    {
        printf("[repro] failed to convert command-line arguments\n");
        return 1;
    }

    ZeroMemory(&app, sizeof(app));
    app.instance = GetModuleHandleW(NULL);
    app.environment_handler.lpVtbl = &environment_vtbl;
    app.environment_handler.refs = 1;
    app.environment_handler.app = &app;

    result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(result))
    {
        log_hresult("CoInitializeEx", result);
        return 1;
    }

    if (!app_start(&app, loader_path, user_data_path, base_url))
    {
        app_stop(&app);
        CoUninitialize();
        return 1;
    }

    while (GetMessageW(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    app_stop(&app);
    CoUninitialize();
    return 0;
}
