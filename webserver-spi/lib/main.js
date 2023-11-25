'use strict';

const stringify = require('onml/stringify.js');
const getElement = require('./get-element.js');

global.SAKI = async (divName) => {
  let t0;
  const content = getElement(divName);

  // p2k groups
  const btns = [
    {id: 'btn_start', label: 'Start', cmd: 0},
    {id: 'btn_stop',  label: 'Stop',  cmd: 1},
    {id: 'btn_pause', label: 'Pause', cmd: 2}
  ];

  const ml = ['div'];
  btns.map(btno => {
    ml.push(['button', {id: btno.id}, btno.label]);
  });
  content.innerHTML = stringify(ml);

  const socket = new WebSocket('ws://' + location.host + '/dev1');
  socket.binaryType = 'arraybuffer';

  // Connection opened
  await new Promise((resolve, reject) => {
    socket.addEventListener('open', (event) => {
      resolve();
    });
  });

  btns.map(btno => {
    document.getElementById(btno.id).addEventListener('click', (event) => {
      console.log(btno.label, event);
      socket.send(btno.cmd);
    });
  });

  // Listen for messages
  socket.addEventListener('message', (event) => {
    const u8 = new Uint8Array(event.data);
    let str = '< ';
    for (const c of u8) {
      str += c.toString(16).padStart(2, 0) + ' ';
    }
    str += '>';
    console.log('Message from server: ' + str + ' (' + (performance.now() - t0) + ')');
  });

  global.SOCKET = {
    send: (msg) => {
      t0 = performance.now();
      socket.send(msg);
    }
  };
};

/* eslint-env browser */
