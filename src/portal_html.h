#ifndef PORTAL_HTML_H
#define PORTAL_HTML_H

#include <Arduino.h>

// Web portal HTML with embedded CSS and JavaScript
String getPortalHTML() {
  return String(R"=====(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>RetroPixel LED</title>
  <style>
    * {
      font-family: 'Courier New', Courier, monospace;
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }

    html, body { height: 100%; }

    body {
      background-color: #a2a2a2;
      color: #000;
      padding: 20px;
      min-height: 100vh;
      display: flex;
      justify-content: center;
      flex-direction: column;
    }

    h1 {
      text-align: center;
      font-size: 28px;
      font-weight: 700;
      margin-bottom: 20px;
      text-shadow: -0.3rem 0.3rem 0 rgba(29, 30, 28, 0.26);
      color: #fff;
    }

    .c {
      max-width: 800px;
      margin: 0 auto;
      background-color: #fff;
      border: 2px solid #000;
      box-shadow: -0.6rem 0.6rem 0 rgba(29, 30, 28, 0.26);
      padding: 0;
    }

    .c > h1 {
      font-size: 18px;
      padding: 12px;
      border-bottom: 2px solid #000;
      margin: 0;
      text-shadow: none;
      color: #000;
    }

    fieldset {
      border: 2px solid #000 !important;
      margin: 15px !important;
      padding: 15px !important;
      background-color: #f5f5f5;
    }

    legend {
      font-weight: 700;
      color: #000;
      border: 2px solid #000;
      padding: 4px 8px;
      background-color: #dadad3;
    }

    label {
      display: block;
      font-weight: 700;
      margin: 12px 0 6px 0;
      font-size: 12px;
      color: #000;
    }

    .mode-label {
      display: flex;
      align-items: center;
      margin: 12px 0 6px 0;
      font-weight: 700;
      color: #000;
    }

    .mode-input {
      margin-right: 8px;
      cursor: pointer;
    }

    input, textarea {
      width: 100%;
      border: 2px solid #000;
      background-color: #dadad3;
      padding: 8px;
      font-family: 'Courier New', Courier, monospace;
      font-size: 12px;
      margin-bottom: 10px;
      box-shadow: none !important;
    }

    input:focus, textarea:focus {
      outline: none;
      background-color: #e8e8e1;
      border: 2px solid #000;
    }

    button {
      width: 100%;
      padding: 10px;
      background-color: #dadad3;
      border: 2px solid #000;
      font-weight: 700;
      cursor: pointer;
      margin: 10px 0;
      font-size: 12px;
      box-shadow: none !important;
      transition: all 0.1s;
      font-family: 'Courier New', Courier, monospace;
    }

    button:hover {
      background-color: #c8c8c1;
      border: 2px solid #000;
    }

    button:active {
      transform: translate(-2px, 2px);
      box-shadow: -0.2rem 0.2rem 0 rgba(29, 30, 28, 0.26);
    }

    .msg {
      display: none;
      padding: 10px;
      margin: 10px 0;
      border: 2px solid #000;
      font-size: 12px;
      background-color: #f5f5f5;
      font-weight: 500;
    }

    .msg.show {
      display: block;
    }

    .msg.ok {
      background-color: #d4edda;
      border-color: #000;
      color: #155724;
    }

    .msg.er {
      background-color: #f8d7da;
      border-color: #000;
      color: #721c24;
    }

    .fs-title {
      font-size: 18px;
      padding: 12px;
      border-bottom: 2px solid #000;
      margin: 0;
      text-shadow: none;
      color: #000;
    }
  </style>
</head>
<body>
  <h1>RETRO PIXEL</h1>
  <div class="c">
    <h1 class="fs-title">Settings</h1>

    <fieldset>
      <legend>WiFi Configuration</legend>
      <label>SSID:</label>
      <input id="ss" placeholder="Network name">
      <label>Password:</label>
      <input type="password" id="pw" placeholder="WiFi password">
      <button onclick="cn()">Connect</button>
      <div id="wm" class="msg"></div>
    </fieldset>

    <fieldset>
      <legend>Display Text</legend>
      <label>Scroll Text (50 max):</label>
      <textarea id="tx" placeholder="Scroll text..."></textarea>
      <button onclick="sv()">Save Text</button>
      <div id="tm" class="msg"></div>
    </fieldset>

    <fieldset>
      <legend>Display Modes</legend>
      <label class="mode-label"><input type="checkbox" id="mc" class="mode-input"> Clock</label>
      <label class="mode-label"><input type="checkbox" id="mt" class="mode-input"> Text</label>
      <label>Change Interval (seconds):</label>
      <input type="number" id="mi" placeholder="10" min="1" max="300">
      <button onclick="sm()">Save Modes</button>
      <div id="mmsg" class="msg"></div>
    </fieldset>

    <fieldset>
      <legend>System</legend>
      <button onclick="rb()">Reboot Device</button>
      <div id="sm" class="msg"></div>
    </fieldset>
  </div>

  <script>
    function ldWiFi() {
      fetch('/api/wifi-status')
        .then(r => r.json())
        .then(d => {
          if (d.ssid) {
            document.getElementById('ss').value = d.ssid;
            document.getElementById('pw').value = '••••••••';
          }
        })
        .catch(e => console.log(e));
    }

    function cn() {
      var s = document.getElementById('ss').value.trim();
      var p = document.getElementById('pw').value;
      if (!s || !p) {
        alert('Enter SSID and password');
        return;
      }
      var m = document.getElementById('wm');
      m.textContent = 'Connecting...';
      m.className = 'msg show';
      var f = new FormData();
      f.append('ssid', s);
      f.append('password', p);
      fetch('/api/connect-wifi', { method: 'POST', body: f })
        .then(r => r.json())
        .then(d => {
          if (d.success) {
            m.textContent = 'Connected! Redirecting...';
            m.className = 'msg show ok';
            setTimeout(function() { location.reload(); }, 2000);
          } else {
            m.textContent = 'Connection failed';
            m.className = 'msg show er';
          }
        })
        .catch(e => {
          m.textContent = 'Error';
          m.className = 'msg show er';
        });
    }

    function ld() {
      fetch('/api/get-text')
        .then(r => r.json())
        .then(d => {
          if (d.text) document.getElementById('tx').value = d.text;
        })
        .catch(e => console.log(e));
    }

    function sv() {
      var t = document.getElementById('tx').value.trim();
      if (!t) {
        alert('Enter text');
        return;
      }
      var m = document.getElementById('tm');
      m.textContent = 'Saving...';
      m.className = 'msg show';
      var f = new FormData();
      f.append('text', t);
      fetch('/api/update-text', { method: 'POST', body: f })
        .then(r => r.json())
        .then(d => {
          if (d.success) {
            m.textContent = 'Saved!';
            m.className = 'msg show ok';
          } else {
            m.textContent = 'Error';
            m.className = 'msg show er';
          }
        })
        .catch(e => {
          m.textContent = 'Failed';
          m.className = 'msg show er';
        });
    }

    function rb() {
      var m = document.getElementById('sm');
      m.textContent = 'Rebooting...';
      m.className = 'msg show';
      fetch('/api/reboot', { method: 'POST' })
        .then(r => r.json())
        .then(d => {
          m.textContent = 'Device is restarting...';
          m.className = 'msg show ok';
          setTimeout(() => { location.reload(); }, 3000);
        })
        .catch(e => {
          m.textContent = 'Reboot failed';
          m.className = 'msg show er';
        });
    }

    function ldModes() {
      fetch('/api/get-modes')
        .then(r => r.json())
        .then(d => {
          if (d.clockEnabled) document.getElementById('mc').checked = true;
          if (d.textEnabled) document.getElementById('mt').checked = true;
          if (d.interval) document.getElementById('mi').value = d.interval;
        })
        .catch(e => console.log(e));
    }

    function sm() {
      var mc = document.getElementById('mc').checked;
      var mt = document.getElementById('mt').checked;
      var mi = document.getElementById('mi').value || 10;

      if (!mc && !mt) {
        alert('Select at least one mode');
        return;
      }

      var m = document.getElementById('mmsg');
      m.textContent = 'Saving...';
      m.className = 'msg show';
      var f = new FormData();
      f.append('clock', mc ? 1 : 0);
      f.append('text', mt ? 1 : 0);
      f.append('interval', mi);
      fetch('/api/update-modes', { method: 'POST', body: f })
        .then(r => r.json())
        .then(d => {
          if (d.success) {
            m.textContent = 'Saved!';
            m.className = 'msg show ok';
          } else {
            m.textContent = 'Error';
            m.className = 'msg show er';
          }
        })
        .catch(e => {
          m.textContent = 'Failed';
          m.className = 'msg show er';
        });
    }

    window.onload = function() {
      ldWiFi();
      ld();
      ldModes();
    }
  </script>
</body>
</html>)=====" );
}

#endif // PORTAL_HTML_H
