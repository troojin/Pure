# Pure

Pure is a lightweight, cross-platform C++17 library for native desktop applications with HTML, CSS, and JavaScript user interfaces. It manages the native window and platform webview engine so an application only needs to create a window, load a page, and run it:

- **Windows**: Win32 + Microsoft Edge WebView2
- **macOS**: Cocoa (`NSWindow` / `NSView`) + WebKit (`WKWebView`)
- **Linux**: GTK 3 (`GtkWindow` / `GtkWidget`) + WebKitGTK (`webkit2gtk`)

It can also attach an HTML overlay or panel to an existing native window or view owned by your application (`HWND`, `NSWindow*` / `NSView*`, or `GtkWidget*`). This is ideal for debug menus, inspectors, consoles, and developer overlays.

## Add it to a project

> For a full step-by-step walkthrough showing every file to create and where to put it, see [**USAGE.md**](USAGE.md).

1. Download **`Pure.zip`** from [Releases](https://github.com/troojin/pure/releases) (or copy the [`library/`](library) folder from this repository).
2. Extract or place it into your project folder as **`Pure/`**:

```text
MyApp/
├── Pure/                  <-- Put the downloaded Pure library folder here
│   ├── include/
│   ├── src/
│   ├── web/
│   └── CMakeLists.txt
├── src/
│   └── main.cpp           <-- Put your C++ code here
├── web/
│   ├── index.html         <-- Put your HTML/CSS/JS files here
│   ├── style.css
│   └── app.js
└── CMakeLists.txt         <-- Put your root CMakeLists.txt here
```

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyApp LANGUAGES CXX)

add_subdirectory(Pure)

add_executable(MyApp src/main.cpp)
target_link_libraries(MyApp PRIVATE Pure::Pure)
Pure_copy_runtime(MyApp)
```

On Windows, `Pure_copy_runtime()` places `WebView2Loader.dll` beside the executable and CMake downloads the WebView2 SDK automatically on the first configure. On macOS and Linux, `Pure_copy_runtime()` is a no-op so your `CMakeLists.txt` stays identical across platforms.

## Quick start

```cpp
#include <Pure/Pure.h>
#include <iostream>

int main() {
    Pure::Window window("My App", 1024, 768);
    if (!window.valid() || !window.load("web/index.html")) {
        return 1;
    }

    window.on_message([&window](const std::string& message) {
        std::cout << message << '\n';
        window.post_message("C++ received your message");
    });

    window.show();
    window.run();
}
```

`load()` accepts a local file path (absolute, relative to the working directory, or relative to the executable directory) as well as URLs (`https://`, `http://`, `file://`, `data:`). For local files, Pure automatically maps the containing directory so relative CSS, JavaScript, image, and font paths resolve cleanly. `load_html()` is also available to load raw HTML strings directly.

## C++ and JavaScript messages

Pure automatically injects the `window.Pure` bridge into every page across Windows, macOS, and Linux:

```js
window.Pure.send('save');

window.Pure.onMessage(data => {
    console.log('C++ says:', data);
});
```

For compatibility with existing WebView2 code, `window.chrome.webview.postMessage()` and `window.chrome.webview.addEventListener('message', ...)` are also supported across all three platforms.

`execute_script()` is available for one-off C++-initiated JavaScript:

```cpp
window.execute_script("document.body.classList.add('ready')");
```

## Embed in an existing native window

`Context` does not create or destroy the host window. Attach it to your existing native handle (`HWND` on Windows, `NSWindow*` or `NSView*` on macOS, `GtkWidget*` or `GtkOverlay*` on Linux):

```cpp
Pure::Context gui;
if (!gui.attach(host_window) || !gui.load("web/debug-menu.html")) {
    return 1;
}
```

`attach(window)` enables a transparent webview background by default so CSS can place floating panels over the host application. Pass `attach(window, false)` for an opaque embedded page. See the [`embedded` example](examples/embedded) for a working demonstration.

## API

| Method | Purpose |
| --- | --- |
| `Window(title, width, height)` | Creates a native window and platform webview instance. |
| `valid()` | Reports whether initialization succeeded. |
| `load(path_or_url)` | Loads an HTML file (mapping its folder for assets) or URL. |
| `load_html(html)` | Loads an HTML document directly from a string. |
| `show()` | Makes the native window visible and focuses it. |
| `poll_events()` | Processes pending window and webview events; returns `false` after close. |
| `run()` | Runs the event loop until the window closes. |
| `close()` | Closes the window or detaches the embedded context. |
| `set_title(title)` | Updates the native window title. |
| `set_size(width, height)` | Resizes the window client area. |
| `native_handle()` | Returns the underlying `HWND`, `NSWindow*`/`NSView*`, or `GtkWidget*`. |
| `on_message(handler)` | Sets the JavaScript-to-C++ message callback. |
| `post_message(message)` | Sends a string message to JavaScript. |
| `execute_script(script)` | Evaluates JavaScript in the current page. |
| `Context::attach(window, transparent)` | Attaches Pure to an existing native window/view without taking ownership. |
| `Context::poll_events()` / `Context::run()` | Optional event-loop helpers for an embedded host. |

## Requirements

- **All platforms**: CMake 3.24+ and a C++17 compiler
- **Windows**: Windows 10 or 11 (64-bit), Visual Studio 2019+ or Clang/MSVC, WebView2 Runtime
- **macOS**: macOS 11+, Xcode / Apple Clang (`Cocoa` and `WebKit` system frameworks)
- **Linux**: GCC or Clang, `pkg-config`, `gtk+-3.0`, and `webkit2gtk-4.1` (or `webkit2gtk-4.0`)
