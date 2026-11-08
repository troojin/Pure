# How to Use Pure in Your Project

This guide walks through downloading **Pure**, placing it into your project folder, writing your first C++ + HTML/CSS/JS app, and building it.

---

## Step 1: Download Pure

You can get Pure in either of two ways:

1. **From GitHub Releases (Recommended)**
   - Go to the [Releases](https://github.com/troojin/pure/releases) page and download **`Pure.zip`**.
   - Extract `Pure.zip` into a folder named **`Pure`** inside your project directory.

2. **From the Repository**
   - Clone or download `https://github.com/troojin/pure`.
   - Copy the **`library/`** folder into your project directory and rename it to **`Pure/`**.

---

## Step 2: Put Everything in Your Project Folder

Create a new folder for your project (for example, `MyApp/`) and arrange your files like this:

```text
MyApp/
├── Pure/                  <-- Extracted Pure.zip (or copied from library/)
│   ├── include/
│   │   └── Pure/
│   │       └── Pure.h
│   ├── src/
│   ├── web/
│   └── CMakeLists.txt
├── src/
│   └── main.cpp           <-- Your C++ application code
├── web/                   <-- Your HTML, CSS, and JS UI files
│   ├── index.html
│   ├── style.css
│   └── app.js
└── CMakeLists.txt         <-- Your project's CMake build file
```

---

## Step 3: Create Your Project Files

### 1. `CMakeLists.txt` (in `MyApp/CMakeLists.txt`)

This tells CMake to build `Pure`, link it to your executable, copy the runtime (`WebView2Loader.dll` on Windows), and copy your `web/` folder next to your compiled binary:

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyApp LANGUAGES CXX)

add_subdirectory(Pure)

add_executable(MyApp src/main.cpp)
target_link_libraries(MyApp PRIVATE Pure::Pure)
Pure_copy_runtime(MyApp)

add_custom_command(TARGET MyApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/web"
        "$<TARGET_FILE_DIR:MyApp>/web"
)
```

### 2. `src/main.cpp` (in `MyApp/src/main.cpp`)

Create a window, load `web/index.html`, listen for messages from JavaScript, and run the window loop:

```cpp
#include <Pure/Pure.h>
#include <iostream>

int main() {
    Pure::Window window("My App", 1024, 768);
    if (!window.valid() || !window.load("web/index.html")) {
        std::cerr << "Failed to initialize Pure window.\n";
        return 1;
    }

    window.on_message([&window](const std::string& message) {
        std::cout << "From JS: " << message << '\n';
        window.post_message("C++ received: " + message);
    });

    window.show();
    window.run();
}
```

### 3. `web/index.html` (in `MyApp/web/index.html`)

```html
<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>My App</title>
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <main>
        <h1>My App</h1>
        <button id="send" type="button">Send Message to C++</button>
        <p id="reply">Waiting...</p>
    </main>
    <script src="app.js"></script>
</body>
</html>
```

### 4. `web/style.css` (in `MyApp/web/style.css`)

```css
body {
    margin: 0;
    padding: 32px;
    font-family: system-ui, -apple-system, sans-serif;
    background: #121318;
    color: #e4e6eb;
}

button {
    padding: 8px 16px;
    border-radius: 6px;
    border: 1px solid #38bdf8;
    background: #38bdf8;
    color: #082f49;
    font-weight: 600;
    cursor: pointer;
}
```

### 5. `web/app.js` (in `MyApp/web/app.js`)

Pure automatically provides `window.Pure` in every loaded page:

```js
'use strict';

document.querySelector('#send').addEventListener('click', () => {
    window.Pure.send('Hello from JavaScript!');
});

window.Pure.onMessage(data => {
    document.querySelector('#reply').textContent = data;
});
```

---

## Step 4: Build and Run

Open a terminal inside your `MyApp/` folder and run:

### Windows (Visual Studio)

```powershell
cmake -B build -A x64
cmake --build build --config Release
.\build\Release\MyApp.exe
```

### macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/MyApp
```

### Linux

Make sure GTK 3 and WebKitGTK development headers are installed first (e.g., `sudo apt install libgtk-3-dev libwebkit2gtk-4.1-dev` on Ubuntu/Debian):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/MyApp
```

---

## Optional: Embed Pure Inside an Existing Window

If your application or game already creates its own native window (`HWND` on Windows, `NSWindow*` / `NSView*` on macOS, or `GtkWidget*` / `GtkOverlay*` on Linux), use `Pure::Context` instead of `Pure::Window`:

```cpp
#include <Pure/Pure.h>

Pure::Context overlay;
if (!overlay.attach(existing_native_window, true) || !overlay.load("web/overlay.html")) {
    return 1;
}

overlay.on_message([&overlay](const std::string& msg) {
    overlay.post_message("Overlay updated");
});
```

- Pass `true` (the default) as the second argument to `attach(window, true)` to make the webview background transparent so your HTML panels float over your native rendering.
- In your overlay's CSS, keep `body { background: transparent; pointer-events: none; }` and set `pointer-events: auto;` on your interactive menu panels.
