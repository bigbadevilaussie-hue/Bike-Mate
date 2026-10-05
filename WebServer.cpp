#include "WebServer.h"
#include "Config.h"
#include "SerialBuffer.h"

#include <WebServer.h>

extern void tprint(const char* fmt, ...);

static WebServer server(80);
static bool running = false;

volatile bool maintOffRequested = false;

static void handleSerialPage() {
  String html = R"rawliteral(<!DOCTYPE html><html><head>
<meta charset="utf-8"><title>Bike-Mate Serial</title>
<style>
body{font-family:ui-monospace,monospace;background:#181825;color:#cdd6f4;padding:20px;font-size:13px}
h1{color:#89b4fa;margin:0 0 14px}
.top{display:flex;justify-content:space-between;align-items:center;margin-bottom:14px}
#log{background:#232334;padding:14px;border-radius:8px;height:80vh;overflow-y:auto;white-space:pre-wrap;word-break:break-all}
button{background:#313145;color:#cdd6f4;border:1px solid #45475a;padding:6px 12px;border-radius:6px;font-family:inherit;font-size:12px;cursor:pointer}
button:hover{background:#45475a}
</style>
<script>
function refresh(){
  fetch('/serial-raw').then(r=>r.text()).then(t=>{
    var el=document.getElementById('log');
    var atBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 50;
    el.textContent = t;
    if(atBottom) el.scrollTop = el.scrollHeight;
  });
}
function maintOff(){
  if(!confirm('End maintenance mode and sleep?')) return;
  fetch('/maint/off', {method:'POST'}).then(()=>{
    document.getElementById('log').textContent =
      'Maintenance ended. Device will sleep shortly.\nYou can close this tab.';
  });
}
setInterval(refresh, 2000);
window.onload = refresh;
</script>
</head><body>
<div class="top">
  <h1>Bike-Mate Serial</h1>
  <button onclick="maintOff()">End Maintenance</button>
</div>
<div id="log">loading...</div>
</body></html>)rawliteral";
  server.send(200, "text/html", html);
}

static void handleSerialRaw() {
  server.send(200, "text/plain", serialBufGet());
}

static void handleMaintOff() {
  maintOffRequested = true;
  tprint("[MAINT] /maint/off received");
  server.send(200, "text/plain", "OK");
}

void serverSetup() {
  if (running) return;
  server.on("/serial",     HTTP_GET,  handleSerialPage);
  server.on("/serial-raw", HTTP_GET,  handleSerialRaw);
  server.on("/maint/off",  HTTP_POST, handleMaintOff);
  server.onNotFound([](){ server.send(404, "text/plain", "404"); });
  server.begin();
  running = true;
  tprint("[HTTP] server up on :80");
}

void serverLoop() {
  if (!running) return;
  server.handleClient();
}

void serverStop() {
  if (!running) return;
  server.stop();
  running = false;
  tprint("[HTTP] server down");
}

bool serverIsRunning() { return running; }

