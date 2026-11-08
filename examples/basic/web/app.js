'use strict';

const reply = document.querySelector('#reply');
const payload = document.querySelector('#payload');
const sendBtn = document.querySelector('#send');

function sendMessage() {
    const text = payload.value.trim() || 'Hello from JavaScript';
    window.Pure.send(text);
}

sendBtn.addEventListener('click', sendMessage);
payload.addEventListener('keydown', event => {
    if (event.key === 'Enter') sendMessage();
});

window.Pure.onMessage(data => {
    reply.textContent = data;
});
