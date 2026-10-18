# Pure

Pure is a small C++ library for native Windows applications with HTML, CSS, and JavaScript user interfaces. It owns the Win32 and WebView2 setup so an application only needs to create a window, load a page, and run it.

It can also attach the same HTML UI to an HWND that your application already owns. This is useful for debug menus, inspectors, consoles, and developer overlays; Pure supplies WebView2 integration, while HTML, CSS, and JavaScript remain the UI layer.

## Add it to a project

Download a Pure release and place the extracted `Pure/` folder in your project. A release is the contents of [`library/`](library) directly:

```text
MyApp/
├── Pure/
│   ├── include/
│   ├── src/
│   ├── web/
│   └── CMakeLists.txt
├── src/
├── web/
│   └── index.html
└── CMakeLists.txt
```

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyApp LANGUAGES CXX)

add_subdirectory(Pure)

add_executable(MyApp src/main.cpp)
target_link_libraries(MyApp PRIVATE Pure::Pure)
Pure_copy_runtime(MyApp)
```

`Pure_copy_runtime()` places `WebView2Loader.dll` beside the executable. On the first configure, CMake downloads the WebView2 SDK automatically. Your users need the [WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/), which is included with current Windows 11 and Microsoft Edge installations.

## Quick start

```cpp
#include <Pure/Pure.h>
#include <iostream>

int main() {
    Pure::Window window("My App", 800, 600);
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

`load()` accepts an absolute path or a path relative to the process working directory. Pure maps the page's containing directory, so its relative HTML, CSS, JavaScript, image, and font paths work normally. It does not copy or modify your web files.

## C++ and JavaScript messages

Send a string from JavaScript with WebView2's built-in bridge:

```js
window.chrome.webview.postMessage('save');

window.chrome.webview.addEventListener('message', event => {
    console.log('C++ says:', event.data);
});
```

C++ receives the string supplied by `postMessage()` through `on_message()` and sends one with `post_message()`. Use JSON when structured messages are useful; Pure intentionally leaves the message format to your application.

`execute_script()` is also available for one-off C++-initiated JavaScript:

```cpp
window.execute_script("document.body.classList.add('ready')");
```

## Embed in an existing Win32 window

`Context` does not create or own the host HWND. Attach it after creating the host window, then keep dispatching the application's normal Win32 message loop. Pure subclasses the host only to track size changes, and removes that subclass when the context is destroyed or closed. Mouse and keyboard input are handled by the WebView2 child window in the normal Win32 input route.

```cpp
Pure::Context gui;
if (!gui.attach(host_hwnd) || !gui.load("web/debug-menu.html")) {
    return 1;
}
```

`attach(hwnd)` enables a transparent WebView2 background by default so CSS can place panels over the host application. Use `attach(hwnd, false)` for an opaque embedded page. Make the document background transparent and give each panel its own opaque or translucent CSS background. The [`embedded` example](examples/embedded) demonstrates this with a small HTML debug menu and several independent controls.

## API

| Method | Purpose |
| --- | --- |
| `Window(title, width, height)` | Creates a native window and WebView2 instance. |
| `valid()` | Reports whether initialization succeeded. |
| `load(path)` | Loads an HTML file and maps its folder for assets. |
| `show()` | Makes the native window visible. |
| `poll_events()` | Processes pending window and WebView2 events; returns `false` after close. |
| `run()` | Runs the event loop until the window closes. |
| `close()` | Closes the window. |
| `on_message(handler)` | Sets the JavaScript-to-C++ message handler. |
| `post_message(message)` | Sends a string to JavaScript. |
| `execute_script(script)` | Runs JavaScript in the loaded page. |
| `Context::attach(hwnd, transparent)` | Attaches WebView2 to an existing HWND; Pure does not destroy that HWND. |
| `Context::poll_events()` / `Context::run()` | Optional message-loop helpers for an embedded host. Existing applications normally use their own loop. |

## Requirements

- Windows 10 or 11, x64
- CMake 3.24 or newer
- Visual Studio 2019 or newer with C++17 support
- Internet access for the first CMake configure