'use strict';

if (!window.Pure) {
    const listeners = [];
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.addEventListener('message', event => {
            for (const fn of listeners) fn(event.data);
        });
    } else {
        window.__pureDispatch = data => {
            for (const fn of listeners) fn(data);
        };
    }

    const send = message => {
        const payload = typeof message === 'string' ? message : JSON.stringify(message);
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage(payload);
        } else if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.pure) {
            window.webkit.messageHandlers.pure.postMessage(payload);
        }
    };

    window.Pure = {
        send,
        postMessage: send,
        onMessage(handler) {
            if (typeof handler === 'function') listeners.push(handler);
        }
    };
}
