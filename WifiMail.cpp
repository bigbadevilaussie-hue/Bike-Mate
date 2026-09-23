// WifiMail.cpp
// Sends email via Gmail SMTP.
// Uses unified WifiManager for radio bring-up.
// Radio off, BLE restarted, after send.
// Rate-limited by MAIL_RATE_LIMIT_SEC.

#include "WifiMail.h"
#include "Config.h"
#include "WifiManager.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>

extern void tprint(const char* fmt, ...);
extern uint32_t currentEpoch();

// ---------------- internal state ----------------
static WiFiClientSecure mailClient;

RTC_DATA_ATTR static uint32_t _lastMailSentEpoch = 0;
RTC_DATA_ATTR static uint8_t  _mailFailCount    = 0;

// ---------------- base64 ----------------
static String b64(const String& in) {
  static const char* tbl =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;
  int val = 0, bits = -6;
  for (size_t i = 0; i < in.length(); i++) {
    val = (val << 8) + (uint8_t)in[i];
    bits += 8;
    while (bits >= 0) {
      out += tbl[(val >> bits) & 0x3F];
      bits -= 6;
    }
  }
  if (bits > -6) out += tbl[((val << 8) >> (bits + 8)) & 0x3F];
  while (out.length() % 4) out += '=';
  return out;
}

// ---------------- SMTP helpers ----------------
static String readResp(unsigned long timeout = 10000) {
  unsigned long t0 = millis();
  String out;
  while (millis() - t0 < timeout) {
    while (mailClient.available()) {
      char c = mailClient.read();
      out += c;
      t0 = millis();
    }
    if (out.length() && out.endsWith("\r\n")) {
      int last = out.lastIndexOf("\r\n", out.length() - 3);
      if (last >= 0 && out.length() - last >= 5 && out[last + 4] == ' ') break;
      if (last < 0 && out.length() >= 4 && out[3] == ' ') break;
    }
    delay(10);
  }
  return out;
}

static bool sendCmd(const String& cmd, const char* expect = "250") {
  if (cmd.length()) mailClient.print(cmd + "\r\n");
  String r = readResp();
  tprint("[MAIL] << %s", r.c_str());
  if (!r.startsWith(expect)) {
    tprint("[MAIL] !! expected %s", expect);
    return false;
  }
  return true;
}

// ---------------- core send ----------------
bool sendMail(const String& subject, const String& body) {
#if !MAIL_ENABLED
  tprint("[MAIL] disabled by MAIL_ENABLED=0");
  return false;
#endif

  // Rate limit
  uint32_t now = currentEpoch();
  if (now > 0 && _lastMailSentEpoch > 0) {
    uint32_t elapsed = now - _lastMailSentEpoch;
    if (elapsed < MAIL_RATE_LIMIT_SEC) {
      tprint("[MAIL] rate-limited (%lus since last)",
             (unsigned long)elapsed);
      return false;
    }
  }

  // Failure backoff
  if (_mailFailCount >= MAIL_FAIL_BACKOFF) {
    tprint("[MAIL] backoff active (fails=%d)", _mailFailCount);
    return false;
  }

  if (!wifiBringUp()) {
    wifiBringDown();
    _mailFailCount++;
    return false;
  }

  tprint("[MAIL] TLS connect");
  mailClient.setInsecure();
  if (!mailClient.connect(SMTP_HOST, SMTP_PORT)) {
    tprint("[MAIL] TCP connect failed");
    mailClient.stop();
    wifiBringDown();
    _mailFailCount++;
    return false;
  }

  String r = readResp();
  tprint("[MAIL] << %s", r.c_str());

  bool ok = true;
  ok = ok && sendCmd("EHLO bike-mate", "250");
  if (ok) ok = ok && sendCmd("AUTH LOGIN", "334");
  if (ok) ok = ok && sendCmd(b64(GMAIL_SENDER), "334");
  if (ok) ok = ok && sendCmd(b64(String(GMAIL_APP_PW)), "235");
  if (ok) ok = ok && sendCmd("MAIL FROM:<" + String(GMAIL_SENDER) + ">", "250");
  if (ok) ok = ok && sendCmd("RCPT TO:<" + String(MAIL_RECIPIENT) + ">", "250");
  if (ok) ok = ok && sendCmd("DATA", "354");

  if (ok) {
    String msg;
    msg += "From: Bike-Mate <" + String(GMAIL_SENDER) + ">\r\n";
    msg += "To: <" + String(MAIL_RECIPIENT) + ">\r\n";
    msg += "Subject: " + subject + "\r\n";
    msg += "MIME-Version: 1.0\r\n";
    msg += "Content-Type: text/plain; charset=UTF-8\r\n";
    msg += "X-Mailer: Bike-Mate/" + String(BIKE_MATE_VERSION) + "\r\n";
    msg += "\r\n";
    msg += body;
    if (!msg.endsWith("\r\n")) msg += "\r\n";
    msg += ".\r\n";
    ok = sendCmd(msg, "250");
  }

  if (ok) sendCmd("QUIT", "221");

  mailClient.stop();
  wifiBringDown();

  if (ok) {
    _lastMailSentEpoch = currentEpoch();
    _mailFailCount = 0;
    tprint("[MAIL] SENT: %s", subject.c_str());
  } else {
    _mailFailCount++;
    tprint("[MAIL] FAILED (fails=%d)", _mailFailCount);
  }
  return ok;
}

// ---------------- accessors ----------------
uint32_t mailLastSentEpoch() { return _lastMailSentEpoch; }
uint8_t  mailFailCount()     { return _mailFailCount; }

// ---------------- convenience wrappers ----------------
bool sendLowBatteryAlert(float voltage, float threshold) {
  String subject = "Bike-Mate: Low battery (" + String(voltage, 2) + "V)";
  String body;
  body += "BIKE-MATE LOW BATTERY WARNING\r\n\r\n";
  body += "Battery voltage: " + String(voltage, 2) + " V\r\n";
  body += "Warning level:   " + String(threshold, 2) + " V\r\n";
  body += "\r\nThe motorcycle battery has dropped below the warning threshold.\r\n";
  body += "This may indicate the battery is failing to hold charge.\r\n\r\n";
  body += "--\r\nBike-Mate " + String(BIKE_MATE_VERSION) + "\r\n";
  body += "Automated alert - do not reply\r\n";
  return sendMail(subject, body);
}

bool sendRideSummary(uint32_t startEpoch, uint32_t durationSecs,
                     float preVolt, float avgVolt, float minVolt, float maxVolt,
                     int8_t minTemp, int8_t maxTemp, uint8_t flags) {
  String subject = "Bike-Mate: Ride " + String(durationSecs / 60) + "min "
                 + String(preVolt, 2) + "V";
  String body;
  body += "BIKE-MATE RIDE SUMMARY\r\n\r\n";
  body += "Duration:  " + String(durationSecs / 60) + " min\r\n\r\n";
  body += "Voltage:\r\n";
  body += "  Pre-ride: " + String(preVolt, 2) + " V\r\n";
  body += "  Min:      " + String(minVolt, 2) + " V\r\n";
  body += "  Max:      " + String(maxVolt, 2) + " V\r\n";
  body += "  Avg:      " + String(avgVolt, 2) + " V\r\n\r\n";
  body += "Temperature:\r\n";
  body += "  Min: " + String(minTemp) + " C\r\n";
  body += "  Max: " + String(maxTemp) + " C\r\n\r\n";
  body += "Flags: 0x" + String(flags, HEX) + "\r\n";
  body += "\r\n--\r\nBike-Mate " + String(BIKE_MATE_VERSION) + "\r\n";
  return sendMail(subject, body);
}

bool sendWeeklySummary(const String& text) {
  String subject = "Bike-Mate: Weekly summary";
  return sendMail(subject, text);
}
