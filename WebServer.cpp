#include "WebServer.h"
#include "Config.h"
#include "Settings.h"

#include <WebServer.h>
#include <WiFi.h>

extern void tprint(const char* fmt, ...);

static WebServer server(80);
static bool _done = false;

static void handle_settings_get() {
  char buf[256];
  settingsToJson(buf, sizeof(buf));
  server.send(200, "application/json", buf);
  tprint("[WEB] GET /settings");
}

static void handle_settings_post() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "no body");
    return;
  }
  String body = server.arg("plain");
  if (settingsApplyJson(body.c_str())) {
    server.send(200, "text/plain", "OK");
    _done = true;
    tprint("[WEB] POST /settings OK");
  } else {
    server.send(400, "text/plain", "rejected");
    tprint("[WEB] POST /settings rejected");
  }
}

void webServerStart() {
  _done = false;
  server.on("/settings", HTTP_GET, handle_settings_get);
  server.on("/settings", HTTP_POST, handle_settings_post);
  server.begin();
  tprint("[WEB] server started on port 80");
}

void webServerLoop() {
  server.handleClient();
}

void webServerStop() {
  server.stop();
  tprint("[WEB] server stopped");
}

bool webServerDone() {
  return _done;
}
