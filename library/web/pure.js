'use strict';

window.WebGUI = {
    send(message) {
        window.chrome.webview.postMessage(typeof message === 'string' ? message : JSON.stringify(message));
    },
    onMessage(handler) {
        window.chrome.webview.addEventListener('message', event => handler(event.data));
    }
};
