#ifndef PORTAL_HTML_H
#define PORTAL_HTML_H

// Web portal HTML stored in flash (no heap allocation)
const char PORTAL_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>RetroPixel LED</title>
  <style>*{font-family:'Courier New',Courier,monospace;margin:0;padding:0;box-sizing:border-box}body{background:#a2a2a2;color:#000;padding:20px;min-height:100vh;display:flex;align-items:flex-start;justify-content:center;flex-direction:column}h1{text-align:center;font-size:28px;font-weight:700;margin-bottom:20px;text-shadow:-.3rem .3rem 0 rgba(29,30,28,.26);color:#fff;width:100%}.c{width:100%;max-width:1400px;margin:0 auto;background:#fff;border:2px solid #000;box-shadow:-.6rem .6rem 0 rgba(29,30,28,.26)}.c>h1{font-size:18px;padding:12px;border-bottom:2px solid #000;margin:0;text-shadow:none;color:#000}.grid{display:grid;grid-template-columns:1fr;gap:0;padding:0}@media(min-width:700px){.grid{grid-template-columns:repeat(2,1fr)}}@media(min-width:1100px){.grid{grid-template-columns:repeat(4,1fr)}}.span2{grid-column:span 1}@media(min-width:700px){.span2{grid-column:span 2}}fieldset{border:2px solid #000;margin:15px;padding:15px;background:#f5f5f5}legend{font-weight:700;border:2px solid #000;padding:4px 8px;background:#dadad3}label{display:block;font-weight:700;margin:12px 0 6px;font-size:12px}.mode-label{display:flex;align-items:center;margin:12px 0 6px;font-weight:700}.mode-input{margin-right:8px;cursor:pointer}input,textarea{width:100%;border:2px solid #000;background:#dadad3;padding:8px;font-family:inherit;font-size:12px;margin-bottom:10px}input:focus,textarea:focus{outline:0;background:#e8e8e1}button{width:100%;padding:10px;background:#dadad3;border:2px solid #000;font-weight:700;cursor:pointer;margin:10px 0;font-size:12px;font-family:inherit}button:hover{background:#c8c8c1}button:active{transform:translate(-2px,2px);box-shadow:-.2rem .2rem 0 rgba(29,30,28,.26)}.msg{display:none;padding:10px;margin:10px 0;border:2px solid #000;font-size:12px;background:#f5f5f5}.msg.show{display:block}.msg.ok{background:#d4edda;color:#155724}.msg.er{background:#f8d7da;color:#721c24}</style>
</head>
<body>
  <h1>RETRO PIXEL</h1>
  <div class="c">
    <h1>Settings</h1>

    <div class="grid">
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
      <label>Clock Duration (seconds):</label>
      <input type="number" id="cd" placeholder="10" min="1" max="300">
      <label>Text Duration (seconds):</label>
      <input type="number" id="td" placeholder="60" min="1" max="300">
      <button onclick="sm()">Save Modes</button>
      <div id="mmsg" class="msg"></div>
    </fieldset>

    <fieldset class="span2">
      <legend>Home Assistant / MQTT</legend>
      <label>Broker Address:</label>
      <input id="mb" placeholder="192.168.1.100" type="text">
      <label>Broker Port:</label>
      <input id="mp" placeholder="1883" type="number" min="1" max="65535">
      <label>Client Name:</label>
      <input id="mcn" placeholder="RetroPixelLED" type="text">
      <label>Username (optional):</label>
      <input id="mu" placeholder="mqtt user" type="text">
      <label>Password (optional):</label>
      <input id="mpass" placeholder="mqtt password" type="password">
      <label>Topic Prefix:</label>
      <input id="mprefix" placeholder="retropixel" type="text">
      <button onclick="sc()">Save MQTT Config</button>
      <div id="mcmsg" class="msg"></div>
      <div id="mstatus" style="margin-top:10px;"></div>
    </fieldset>

    <fieldset>
      <legend>System</legend>
      <button onclick="rb()">Reboot Device</button>
      <div id="sm" class="msg"></div>
    </fieldset>
    </div>
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
          if (d.clockDuration) document.getElementById('cd').value = d.clockDuration;
          if (d.textDuration) document.getElementById('td').value = d.textDuration;
        })
        .catch(e => console.log(e));
    }

    function sm() {
      var mc = document.getElementById('mc').checked;
      var mt = document.getElementById('mt').checked;
      var cd = document.getElementById('cd').value || 10;
      var td = document.getElementById('td').value || 60;

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
      f.append('clockDuration', cd);
      f.append('textDuration', td);
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

    function ldMqtt() {
      fetch('/api/mqtt-config')
        .then(r => r.json())
        .then(d => {
          if (d.broker) document.getElementById('mb').value = d.broker;
          if (d.port) document.getElementById('mp').value = d.port;
          if (d.clientname) document.getElementById('mcn').value = d.clientname;
          if (d.username) document.getElementById('mu').value = d.username;
          if (d.password) document.getElementById('mpass').value = d.password;
          if (d.prefix) document.getElementById('mprefix').value = d.prefix;
          var status = document.getElementById('mstatus');
          if (d.connected) {
            status.innerHTML = '<div style="color:green;font-weight:bold;">✓ Connected to MQTT</div>';
          } else {
            status.innerHTML = '<div style="color:red;">✗ Not connected to MQTT</div>';
          }
        })
        .catch(e => {
          console.log('Error loading MQTT config:', e);
          document.getElementById('mstatus').innerHTML = '<div style="color:gray;">MQTT not available</div>';
        });
    }

    function sc() {
      var broker = document.getElementById('mb').value.trim();
      var port = document.getElementById('mp').value.trim();
      var clientname = document.getElementById('mcn').value.trim();
      var user = document.getElementById('mu').value.trim();
      var pass = document.getElementById('mpass').value.trim();
      var prefix = document.getElementById('mprefix').value.trim();

      if (!broker || !port || !clientname) {
        alert('Broker, Port, and Client Name are required');
        return;
      }

      var m = document.getElementById('mcmsg');
      m.textContent = 'Saving...';
      m.className = 'msg show';
      var f = new FormData();
      f.append('broker', broker);
      f.append('port', port);
      f.append('clientname', clientname);
      if (user) f.append('username', user);
      if (pass) f.append('password', pass);
      if (prefix) f.append('prefix', prefix);
      fetch('/api/configure-mqtt', { method: 'POST', body: f })
        .then(r => r.json())
        .then(d => {
          if (d.success) {
            m.textContent = 'Saved! Connecting...';
            m.className = 'msg show ok';
            setTimeout(() => { ldMqtt(); }, 2000);
          } else {
            m.textContent = 'Error saving config';
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
      ldMqtt();
    }
  </script>
</body>
</html>)rawliteral";

#endif // PORTAL_HTML_H
