#include "BleManager.h"
#include "Config.h"
#include "RideLogger.h"
#include "RideStorage.h"

#include <Preferences.h>
#include <LittleFS.h>
#include <string>
#include <time.h>

extern Preferences prefs;
extern uint32_t currentEpoch();
extern void tprint(const char* fmt, ...);
extern unsigned long lastDisconnectMillis;

extern bool gpsHasFix();
extern int32_t gpsLat_x1e7();
extern int32_t gpsLon_x1e7();
extern uint8_t gpsSats();

BLEServer* pServer = nullptr;
BLECharacteristic* pDataChar = nullptr;
BLECharacteristic* pTimeChar = nullptr;
BLECharacteristic* pStreamChar = nullptr;
BLECharacteristic* pRequestChar = nullptr;
BLECharacteristic* pOtaChar = nullptr;

bool bleInited = false;

volatile bool pushRequested = false;
volatile bool pushInProgress = false;
volatile uint32_t pushGuiEpoch = 0;

static volatile bool bleSuspended = false;

extern float latestBatteryVoltage;
extern float latestTemperatureC;
extern bool accState;
extern bool engineWasRunning;
extern bool inPanic;
extern bool isLogging;
extern const char* currentStateString();

bool isActuallyConnected() {
  return pServer && pServer->getConnectedCount() > 0;
}

void bleStop() {
  if (!bleInited) return;

  bleSuspended = true;

  delay(50);

  if (pServer) {
    pServer->getAdvertising()->stop();

    if (pServer->getConnectedCount() > 0) {
      pServer->disconnect(pServer->getConnId());
    }
  }

  delay(100);
}

void bleStart() {
  if (!bleInited) return;

  bleSuspended = false;

  if (pServer) {
    pServer->getAdvertising()->start();
  }
}

static bool isHeaderLine(const String& line) {
  if (line.length() == 0) return true;
  if (line.startsWith("#")) return true;
  if (line.startsWith("epoch,")) return true;
  return false;
}

void pushNewSlots(uint32_t guiHighestEpoch) {
  pushInProgress = true;

  prefs.begin("rides", true);

  uint32_t newestEpoch = prefs.getUInt("newest_epoch", 0);

  RideSummary newest;
  memset(&newest, 0, sizeof(newest));

  if (newestEpoch > 0) {
    char key[16];

    snprintf(key, sizeof(key), "s%lu",
             (unsigned long)newestEpoch);

    size_t sz = prefs.getBytes(
        key,
        &newest,
        sizeof(newest));

    if (sz != sizeof(newest)) {
      newestEpoch = 0;
    }
  }

  prefs.end();

  if (newestEpoch == 0) {
    uint8_t r = 0x00;

    if (pRequestChar) {
      pRequestChar->setValue(&r, 1);
      pRequestChar->notify();
    }

    pushInProgress = false;
    return;
  }

  if (newestEpoch <= guiHighestEpoch) {
    uint8_t r = 0x00;

    if (pRequestChar) {
      pRequestChar->setValue(&r, 1);
      pRequestChar->notify();
    }

    pushInProgress = false;
    return;
  }

  uint8_t r = 0x01;

  if (pRequestChar) {
    pRequestChar->setValue(&r, 1);
    pRequestChar->notify();
  }

  delay(80);

  pStreamChar->setValue(
      (uint8_t*)&newest,
      sizeof(newest));

  pStreamChar->notify();

  delay(100);

  char path[40];

  setenv("TZ", "AEST-10", 1);
  tzset();

  rideStorageBuildFilename(
      newestEpoch,
      path,
      sizeof(path));

  File f = LittleFS.open(path, "r");

  if (!f) {
    tprint("[PUSH] file not found %s", path);

    r = 0x02;

    if (pRequestChar) {
      pRequestChar->setValue(&r, 1);
      pRequestChar->notify();
    }

    pushInProgress = false;
    return;
  }

  while (f.available()) {
    int peek = f.peek();

    if (peek == '#' || peek == 'e') {
      f.readStringUntil('\n');
    } else {
      break;
    }
  }

  uint16_t sent = 0;
  uint8_t buf[8];

  while (f.available() && sent < newest.rowCount) {
    String line = f.readStringUntil('\n');

    if (isHeaderLine(line)) {
      continue;
    }

    uint32_t ep = 0;
    int32_t lat = 0;
    int32_t lon = 0;
    float v = 0;
    int temp = 0;
    int sats = 0;

    int parsed = sscanf(
        line.c_str(),
        "%lu,%ld,%ld,%f,%d,%d",
        (unsigned long*)&ep,
        (long*)&lat,
        (long*)&lon,
        &v,
        &temp,
        &sats);

    if (parsed < 6) {
      continue;
    }

    uint16_t v100 = (uint16_t)(v * 100);

    memcpy(buf + 0, &ep, 4);
    memcpy(buf + 4, &v100, 2);

    buf[6] = (uint8_t)temp;
    buf[7] = (uint8_t)sats;

    pStreamChar->setValue(buf, 8);
    pStreamChar->notify();

    sent++;

    delay(40);
  }

  f.close();

  if (isActuallyConnected()) {
    r = 0x02;

    if (pRequestChar) {
      pRequestChar->setValue(&r, 1);
      pRequestChar->notify();
    }
  }

  delay(100);

  pushInProgress = false;
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) {

    delay(300);

    char vbuf[32];

    snprintf(
        vbuf,
        sizeof(vbuf),
        "{\"fv\":\"%s\"}",
        BIKE_MATE_VERSION);

    if (pDataChar) {
      pDataChar->setValue(
          (uint8_t*)vbuf,
          strlen(vbuf));

      pDataChar->notify();
    }
  }

  void onDisconnect(BLEServer* s) {
    lastDisconnectMillis = millis();

    if (!bleSuspended && pServer) {
      pServer->getAdvertising()->start();
    }
  }
};

class TimeCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) {

    std::string v = c->getValue();

    if (v.length() < 4) return;

    uint32_t t = 0;

    memcpy(&t, v.c_str(), 4);

    if (t < 1700000000UL ||
        t > 4102444800UL) {
      return;
    }

    extern uint32_t macTimeEpoch;
    extern uint32_t secondsAtSync;
    extern uint32_t totalSeconds;

    macTimeEpoch = t;
    secondsAtSync = totalSeconds;
  }
};

class RequestCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) {

    std::string v = c->getValue();

    if (v.length() < 4) return;

    uint32_t epoch = 0;

    memcpy(&epoch, v.c_str(), 4);

    pushGuiEpoch = epoch;
    pushRequested = true;
  }
};

class OtaCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) {

    std::string v = c->getValue();

    if (v.length() < 8) {
      uint8_t nack = 0xFF;

      pOtaChar->setValue(&nack, 1);
      pOtaChar->notify();

      return;
    }

    String payload = String(v.c_str());

    int urlIdx = payload.indexOf("\"url\":\"");
    int urlEnd = payload.indexOf("\"", urlIdx + 7);

    if (urlIdx < 0 || urlEnd < 0) {
      tprint("[OTA] parse: no url");

      uint8_t nack = 0xFF;

      pOtaChar->setValue(&nack, 1);
      pOtaChar->notify();

      return;
    }

    String url =
        payload.substring(urlIdx + 7, urlEnd);

    uint32_t size = 0;

    int sizeIdx =
        payload.indexOf("\"size\":");

    if (sizeIdx > 0) {
      int sizeEnd =
          payload.indexOf(",", sizeIdx);

      if (sizeEnd < 0) {
        sizeEnd =
            payload.indexOf("}", sizeIdx);
      }

      if (sizeEnd > 0) {
        String sizeStr =
            payload.substring(
                sizeIdx + 7,
                sizeEnd);

        size = (uint32_t)sizeStr.toInt();
      }
    }

    String md5 = "";

    int md5Idx =
        payload.indexOf("\"md5\":\"");

    if (md5Idx >= 0) {
      int md5End =
          payload.indexOf("\"", md5Idx + 7);

      if (md5End > 0) {
        md5 =
            payload.substring(
                md5Idx + 7,
                md5End);
      }
    }

    String ver = "";

    int verIdx =
        payload.indexOf("\"ver\":\"");

    if (verIdx >= 0) {
      int verEnd =
          payload.indexOf("\"", verIdx + 7);

      if (verEnd > 0) {
        ver =
            payload.substring(
                verIdx + 7,
                verEnd);
      }
    }

    extern volatile bool otaRequest;
    extern char otaUrl[];
    extern uint32_t otaSize;
    extern char otaMd5[];
    extern char otaVersion[16];
    extern unsigned long otaStartMillis;

    if (url.length() == 0 ||
        url.length() >= OTA_URL_MAX) {

      tprint("[OTA] url bad length");

      uint8_t nack = 0xFF;

      pOtaChar->setValue(&nack, 1);
      pOtaChar->notify();

      return;
    }

    strncpy(
        otaUrl,
        url.c_str(),
        OTA_URL_MAX - 1);

    otaUrl[OTA_URL_MAX - 1] = 0;

    otaSize = size;

    strncpy(
        otaMd5,
        md5.c_str(),
        OTA_MD5_LEN - 1);

    otaMd5[OTA_MD5_LEN - 1] = 0;

    strncpy(
        otaVersion,
        ver.c_str(),
        15);

    otaVersion[15] = 0;

    otaStartMillis = millis();
    otaRequest = true;

    uint8_t ack = 0x01;

    pOtaChar->setValue(&ack, 1);
    pOtaChar->notify();
  }
};

void bleInit() {
  if (bleInited) return;

  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(185);

  pServer = BLEDevice::createServer();

  pServer->setCallbacks(
      new ServerCallbacks());

  BLEService* svc =
      pServer->createService(SERVICE_UUID);

  pDataChar = svc->createCharacteristic(
      DATA_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_NOTIFY);

  pDataChar->addDescriptor(
      new BLE2902());

  pTimeChar = svc->createCharacteristic(
      TIME_UUID,
      BLECharacteristic::PROPERTY_WRITE);

  pTimeChar->setCallbacks(
      new TimeCallbacks());

  pStreamChar = svc->createCharacteristic(
      STREAM_UUID,
      BLECharacteristic::PROPERTY_NOTIFY);

  pStreamChar->addDescriptor(
      new BLE2902());

  pRequestChar = svc->createCharacteristic(
      REQUEST_UUID,
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY);

  pRequestChar->addDescriptor(
      new BLE2902());

  pRequestChar->setCallbacks(
      new RequestCallbacks());

  pOtaChar = svc->createCharacteristic(
      OTA_UUID,
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY);

  pOtaChar->addDescriptor(
      new BLE2902());

  pOtaChar->setCallbacks(
      new OtaCallbacks());

  svc->start();

  BLEAdvertising* adv =
      BLEDevice::getAdvertising();

  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);

  bleInited = true;
}

void publishBLE() {
  if (!pDataChar || !bleInited) return;
  if (!isActuallyConnected()) return;
  if (pushInProgress) return;

  int32_t lat = gpsLat_x1e7();
  int32_t lon = gpsLon_x1e7();

  char buf[240];

  snprintf(
      buf,
      sizeof(buf),
      "{\"v\":%.2f,\"t\":%.1f,\"a\":%d,\"e\":%d,\"w\":%d,\"s\":\"%s\",\"p\":%d,\"ll\":\"%ld.%04ld,%ld.%04ld\",\"lg\":%d,\"gs\":%d}",
      latestBatteryVoltage,
      latestTemperatureC,
      accState ? 1 : 0,
      engineWasRunning ? 1 : 0,
      inPanic ? 1 : 0,
      currentStateString(),
      inPanic ? 1 : 0,
      (long)(lat / 10000000),
      (long)(labs((lat % 10000000) / 1000)),
      (long)(lon / 10000000),
      (long)(labs((lon % 10000000) / 1000)),
      isLogging ? 1 : 0,
      (int)gpsSats());

  pDataChar->setValue(
      (uint8_t*)buf,
      strlen(buf));

  pDataChar->notify();
}