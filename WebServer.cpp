#include "WebServer.h"
#include "Config.h"
#include "SerialBuffer.h"
#include "DisplayManager.h"
#include "OtaManager.h"

#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>

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

static bool otaStarted = false;

static void handleOtaUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    tprint("[OTA] start %s", upload.filename.c_str());
    otaStarted = false;
    // V4.88: drive the same OLED state the BLE OTA path used, so
    // the display shows Downloading -> Flashing -> Rebooting.
    extern volatile bool otaRequest;
    extern volatile uint8_t  otaStage;
    extern volatile uint32_t otaProgressBytes;
    extern volatile uint32_t otaProgressTotal;
    extern char otaVersion[16];
    otaRequest = true;
    otaStage = OTA_STAGE_DOWNLOAD;
    otaProgressBytes = 0;
    otaProgressTotal = 0;
    // V4.90: read Content-Length so /ota-progress has a total.
    {
      String cl = server.header("Content-Length");
      if (cl.length() > 0) {
        otaProgressTotal = (uint32_t)cl.toInt();
      }
      tprint("[OTA] content-length=%lu", (unsigned long)otaProgressTotal);
    }
    // V4.90: GUI passes target version as ?ver=X.YZ so the OLED can
    // show it. HTTP OTA has no other source for the incoming version.
    if (server.hasArg("ver")) {
      String v = server.arg("ver");
      strncpy(otaVersion, v.c_str(), 15);
      otaVersion[15] = 0;
      tprint("[OTA] target ver=%s", otaVersion);
    } else {
      otaVersion[0] = 0;
    }
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      tprint("[OTA] begin FAILED: %s", Update.errorString());
      otaRequest = false;
    } else {
      otaStarted = true;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (otaStarted) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        tprint("[OTA] write FAILED: %s", Update.errorString());
        otaStarted = false;
        extern volatile bool otaRequest;
        otaRequest = false;
      } else {
        extern volatile uint32_t otaProgressBytes;
        otaProgressBytes += upload.currentSize;
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (otaStarted) {
      if (Update.end(true)) {
        tprint("[OTA] OK %u bytes", upload.totalSize);
        extern volatile uint8_t otaStage;
        otaStage = OTA_STAGE_FLASH;
      } else {
        tprint("[OTA] end FAILED: %s", Update.errorString());
        extern volatile bool otaRequest;
        otaRequest = false;
      }
    }
  }
}

static void handleOtaDone() {
  if (Update.hasError()) {
    extern volatile bool otaRequest;
    otaRequest = false;
    server.send(500, "text/plain",
                "FAIL: " + String(Update.errorString()));
  } else {
    tprint("[OTA] rebooting");

    // V4.83: set NVS flag so the next boot forces maintenance mode.
    // HTTP OTA bypasses OtaManager.cpp entirely, so the flag has to be
    // set here.
    Preferences prefs;
    prefs.begin("ota", false);
    prefs.putBool("pending", true);
    bool verifyPending = prefs.getBool("pending", false);
    prefs.end();
    tprint("[OTA] NVS verify pending=%d", verifyPending ? 1 : 0);
    delay(200);

    server.send(200, "text/plain", "OK, rebooting");
    delay(200);

    // V4.88: show "Rebooting in Ns" on the OLED before reset.
    extern volatile uint8_t otaStage;
    extern volatile uint8_t otaRebootCountdown;
    otaStage = OTA_STAGE_REBOOT;
    for (int i = 5; i > 0; i--) {
      otaRebootCountdown = (uint8_t)i;
      tprint("[OTA] reboot in %d...", i);
      drawOLED();
      delay(1000);
    }

    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    ESP.restart();
  }
}

static void handleOtaProgress() {
  extern volatile uint8_t  otaStage;
  extern volatile uint32_t otaProgressBytes;
  extern volatile uint32_t otaProgressTotal;
  extern volatile uint8_t  otaRebootCountdown;
  char buf[128];
  snprintf(buf, sizeof(buf),
           "{\"stage\":%u,\"bytes\":%lu,\"total\":%lu,\"countdown\":%u}",
           (unsigned)otaStage,
           (unsigned long)otaProgressBytes,
           (unsigned long)otaProgressTotal,
           (unsigned)otaRebootCountdown);
  server.send(200, "application/json", buf);
}

static void handleVersion() {
  server.send(200, "text/plain", BIKE_MATE_VERSION);
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
  server.on("/ota",        HTTP_POST, handleOtaDone, handleOtaUpload);
  server.on("/version",    HTTP_GET,  handleVersion);
  server.on("/ota-progress", HTTP_GET, handleOtaProgress);
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

