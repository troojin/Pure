#include "MacOSBackend.h"

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#include <mach-o/dyld.h>

#include <filesystem>
#include <vector>

@interface PureScriptMessageHandler : NSObject <WKScriptMessageHandler>
@property (nonatomic, assign) Pure::MacOSBackend* backend;
@end

@implementation PureScriptMessageHandler
- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
    (void)userContentController;
    if (!self.backend || !message.body) return;
    NSString* text = nil;
    if ([message.body isKindOfClass:[NSString class]]) {
        text = (NSString*)message.body;
    } else {
        text = [message.body description];
    }
    if (text) {
        const char* utf8 = [text UTF8String];
        if (utf8) {
            self.backend->handle_message(std::string(utf8));
        }
    }
}
@end

@interface PureNavigationDelegate : NSObject <WKNavigationDelegate>
@property (nonatomic, assign) bool done;
@property (nonatomic, assign) bool success;
@end

@implementation PureNavigationDelegate
- (void)webView:(WKWebView*)webView didFinishNavigation:(WKNavigation*)navigation {
    (void)webView;
    (void)navigation;
    self.success = true;
    self.done = true;
}

- (void)webView:(WKWebView*)webView didFailNavigation:(WKNavigation*)navigation withError:(NSError*)error {
    (void)webView;
    (void)navigation;
    (void)error;
    self.success = false;
    self.done = true;
}

- (void)webView:(WKWebView*)webView didFailProvisionalNavigation:(WKNavigation*)navigation withError:(NSError*)error {
    (void)webView;
    (void)navigation;
    (void)error;
    self.success = false;
    self.done = true;
}
@end

@interface PureWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) Pure::MacOSBackend* backend;
@end

@implementation PureWindowDelegate
- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    if (self.backend) {
        self.backend->handle_window_close();
    }
}
@end

namespace Pure {

namespace {

std::filesystem::path ExeDir() {
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size == 0) return {};
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
    std::error_code ec;
    return std::filesystem::canonical(std::filesystem::path(buffer.data()), ec).parent_path();
}

bool IsUrl(const std::string& target) {
    return target.rfind("http://", 0) == 0 ||
           target.rfind("https://", 0) == 0 ||
           target.rfind("file://", 0) == 0 ||
           target.rfind("data:", 0) == 0;
}

std::string EscapeJsString(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 16);
    out.push_back('"');
    for (unsigned char c : input) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char hex[7];
                snprintf(hex, sizeof(hex), "\\u%04x", c);
                out += hex;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    out.push_back('"');
    return out;
}

NSString* const kBridgeScript =
    @"(function(){"
    @"if(window.Pure)return;"
    @"const listeners=[];"
    @"window.__pureDispatch=function(data){"
    @"for(const fn of listeners){try{fn(data);}catch(err){console.error(err);}}"
    @"};"
    @"const send=function(msg){"
    @"const payload=typeof msg==='string'?msg:JSON.stringify(msg);"
    @"if(window.webkit&&window.webkit.messageHandlers&&window.webkit.messageHandlers.pure){"
    @"window.webkit.messageHandlers.pure.postMessage(payload);"
    @"}"
    @"};"
    @"window.Pure={"
    @"send:send,"
    @"postMessage:send,"
    @"onMessage:function(fn){if(typeof fn==='function')listeners.push(fn);}"
    @"};"
    @"if(!window.chrome)window.chrome={};"
    @"if(!window.chrome.webview){"
    @"window.chrome.webview={"
    @"postMessage:send,"
    @"addEventListener:function(type,fn){"
    @"if(type==='message'&&typeof fn==='function'){"
    @"listeners.push(function(data){fn({data:data});});"
    @"}"
    @"},"
    @"removeEventListener:function(){}"
    @"};"
    @"}"
    @"})();";

}

std::unique_ptr<Backend> Backend::create() {
    return std::make_unique<MacOSBackend>();
}

void MacOSBackend::handle_message(const std::string& message) {
    if (m_on_message) m_on_message(message);
}

void MacOSBackend::handle_window_close() {
    m_running = false;
    m_window = nil;
}

bool MacOSBackend::init(const char* title, int width, int height, MessageCallback on_message) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        if ([NSApp activationPolicy] == NSApplicationActivationPolicyProhibited) {
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        }
        [NSApp finishLaunching];

        m_on_message = std::move(on_message);

        const CGFloat w = width > 0 ? width : 1024;
        const CGFloat h = height > 0 ? height : 768;
        NSRect frame = NSMakeRect(0, 0, w, h);
        NSWindowStyleMask style = NSWindowStyleMaskTitled |
                                  NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskMiniaturizable |
                                  NSWindowStyleMaskResizable;

        m_window = [[NSWindow alloc] initWithContentRect:frame
                                               styleMask:style
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
        if (!m_window) return false;

        [m_window setReleasedWhenClosed:NO];
        [m_window setTitle:[NSString stringWithUTF8String:(title ? title : "Pure")]];
        [m_window setBackgroundColor:[NSColor colorWithSRGBRed:18.0 / 255.0
                                                         green:19.0 / 255.0
                                                          blue:24.0 / 255.0
                                                         alpha:1.0]];
        [m_window center];

        m_window_delegate = [[PureWindowDelegate alloc] init];
        m_window_delegate.backend = this;
        [m_window setDelegate:m_window_delegate];

        m_owns_window = true;
        m_host_view = [m_window contentView];
        return init_webview(m_host_view, false);
    }
}

bool MacOSBackend::attach(void* native_handle, bool transparent, MessageCallback on_message) {
    if (!native_handle || m_webview) return false;

    @autoreleasepool {
        [NSApplication sharedApplication];
        m_on_message = std::move(on_message);

        NSObject* obj = (__bridge NSObject*)native_handle;
        if ([obj isKindOfClass:[NSWindow class]]) {
            m_window = (NSWindow*)obj;
            m_host_view = [m_window contentView];
        } else if ([obj isKindOfClass:[NSView class]]) {
            m_host_view = (NSView*)obj;
            m_window = [m_host_view window];
        } else {
            return false;
        }

        if (!m_host_view) return false;
        m_owns_window = false;
        return init_webview(m_host_view, transparent);
    }
}

bool MacOSBackend::init_webview(NSView* parent_view, bool transparent) {
    @autoreleasepool {
        WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
        WKUserContentController* controller = [[WKUserContentController alloc] init];

        m_script_handler = [[PureScriptMessageHandler alloc] init];
        m_script_handler.backend = this;
        [controller addScriptMessageHandler:m_script_handler name:@"pure"];

        WKUserScript* script = [[WKUserScript alloc] initWithSource:kBridgeScript
                                                      injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                                   forMainFrameOnly:NO];
        [controller addUserScript:script];
        config.userContentController = controller;

        [config.preferences setValue:@YES forKey:@"developerExtrasEnabled"];

        NSRect bounds = [parent_view bounds];
        m_webview = [[WKWebView alloc] initWithFrame:bounds configuration:config];
        if (!m_webview) return false;

        [m_webview setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];

        m_nav_delegate = [[PureNavigationDelegate alloc] init];
        [m_webview setNavigationDelegate:m_nav_delegate];

        if (transparent) {
            [m_webview setValue:@NO forKey:@"drawsBackground"];
        }

        [parent_view addSubview:m_webview];
        m_running = true;
        return true;
    }
}

bool MacOSBackend::wait_for_navigation() {
    while (!m_nav_delegate.done && m_running) {
        @autoreleasepool {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                untilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]
                                                   inMode:NSDefaultRunLoopMode
                                                  dequeue:YES];
            if (event) {
                [NSApp sendEvent:event];
            }
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                     beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.005]];
        }
    }
    return m_nav_delegate.done && m_nav_delegate.success;
}

bool MacOSBackend::load(const std::string& file) {
    if (!m_webview || file.empty()) return false;

    @autoreleasepool {
        m_nav_delegate.done = false;
        m_nav_delegate.success = false;

        if (IsUrl(file)) {
            NSString* str = [NSString stringWithUTF8String:file.c_str()];
            NSURL* url = [NSURL URLWithString:str];
            if (!url) return false;
            [m_webview loadRequest:[NSURLRequest requestWithURL:url]];
            return wait_for_navigation();
        }

        std::filesystem::path page(file);
        std::error_code ec;
        if (page.is_relative()) {
            std::filesystem::path from_cwd = std::filesystem::absolute(page, ec);
            if (std::filesystem::is_regular_file(from_cwd, ec)) {
                page = from_cwd;
            } else {
                std::filesystem::path from_exe = ExeDir() / page;
                if (std::filesystem::is_regular_file(from_exe, ec)) {
                    page = std::filesystem::absolute(from_exe, ec);
                } else {
                    NSString* resource_dir = [[NSBundle mainBundle] resourcePath];
                    if (resource_dir) {
                        std::filesystem::path from_bundle = std::filesystem::path([resource_dir UTF8String]) / page;
                        if (std::filesystem::is_regular_file(from_bundle, ec)) {
                            page = std::filesystem::absolute(from_bundle, ec);
                        }
                    }
                }
            }
        } else {
            page = std::filesystem::absolute(page, ec);
        }

        if (!std::filesystem::is_regular_file(page, ec)) return false;

        NSString* file_path = [NSString stringWithUTF8String:page.string().c_str()];
        NSString* dir_path = [NSString stringWithUTF8String:page.parent_path().string().c_str()];
        NSURL* file_url = [NSURL fileURLWithPath:file_path isDirectory:NO];
        NSURL* dir_url = [NSURL fileURLWithPath:dir_path isDirectory:YES];
        if (!file_url || !dir_url) return false;

        [m_webview loadFileURL:file_url allowingReadAccessToURL:dir_url];
        return wait_for_navigation();
    }
}

bool MacOSBackend::load_html(const std::string& html) {
    if (!m_webview) return false;

    @autoreleasepool {
        m_nav_delegate.done = false;
        m_nav_delegate.success = false;
        NSString* content = [NSString stringWithUTF8String:html.c_str()];
        if (!content) return false;
        [m_webview loadHTMLString:content baseURL:nil];
        return wait_for_navigation();
    }
}

void MacOSBackend::show() {
    @autoreleasepool {
        if (m_window) {
            [m_window makeKeyAndOrderFront:nil];
            [NSApp activateIgnoringOtherApps:YES];
        }
    }
}

bool MacOSBackend::poll_events() {
    @autoreleasepool {
        for (;;) {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                untilDate:[NSDate distantPast]
                                                   inMode:NSDefaultRunLoopMode
                                                  dequeue:YES];
            if (!event) break;
            [NSApp sendEvent:event];
        }
        [NSApp updateWindows];
    }
    return m_running;
}

void MacOSBackend::run() {
    while (m_running) {
        @autoreleasepool {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                untilDate:[NSDate dateWithTimeIntervalSinceNow:0.016]
                                                   inMode:NSDefaultRunLoopMode
                                                  dequeue:YES];
            if (event) {
                [NSApp sendEvent:event];
            }
        }
        if (!poll_events()) break;
    }
}

void MacOSBackend::close() {
    @autoreleasepool {
        m_running = false;
        if (m_webview) {
            [[m_webview configuration].userContentController removeScriptMessageHandlerForName:@"pure"];
            [m_webview setNavigationDelegate:nil];
            if (!m_owns_window) {
                [m_webview removeFromSuperview];
            }
        }
        if (m_owns_window && m_window) {
            [m_window setDelegate:nil];
            [m_window close];
            m_window = nil;
        }
    }
}

void MacOSBackend::post_message(const std::string& message) {
    if (!m_webview) return;
    @autoreleasepool {
        const std::string js = "window.__pureDispatch && window.__pureDispatch(" + EscapeJsString(message) + ");";
        NSString* script = [NSString stringWithUTF8String:js.c_str()];
        if (script) {
            [m_webview evaluateJavaScript:script completionHandler:nil];
        }
    }
}

void MacOSBackend::execute_script(const std::string& script) {
    if (!m_webview || script.empty()) return;
    @autoreleasepool {
        NSString* ns_script = [NSString stringWithUTF8String:script.c_str()];
        if (ns_script) {
            [m_webview evaluateJavaScript:ns_script completionHandler:nil];
        }
    }
}

void MacOSBackend::set_title(const std::string& title) {
    @autoreleasepool {
        if (m_window) {
            NSString* ns_title = [NSString stringWithUTF8String:title.c_str()];
            if (ns_title) [m_window setTitle:ns_title];
        }
    }
}

void MacOSBackend::set_size(int width, int height) {
    if (width <= 0 || height <= 0) return;
    @autoreleasepool {
        if (m_owns_window && m_window) {
            [m_window setContentSize:NSMakeSize(width, height)];
        } else if (m_webview && m_host_view) {
            [m_webview setFrame:[m_host_view bounds]];
        }
    }
}

void* MacOSBackend::native_handle() const {
    if (m_window) return (__bridge void*)m_window;
    return (__bridge void*)m_host_view;
}

MacOSBackend::~MacOSBackend() {
    if (m_script_handler) m_script_handler.backend = nullptr;
    if (m_window_delegate) m_window_delegate.backend = nullptr;
    close();
}

}
