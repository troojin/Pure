'use strict';

const send = action => window.Pure.send(action);

document.querySelector('#wireframe').addEventListener('change', event => {
    send({ wireframe: event.target.checked });
});

document.querySelector('#pause').addEventListener('change', event => {
    send({ paused: event.target.checked });
});

document.querySelector('#inspect').addEventListener('click', () => {
    send({ action: 'inspect' });
});

window.Pure.onMessage(data => {
    document.querySelector('#reply').textContent = data;
});
