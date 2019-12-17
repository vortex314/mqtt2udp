#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <LedBlinker.h>
#include <MqttUdp.h>
#include <Streams.h>
#include <WiFiUdp.h>

#define STRINGIFY(X) #X
#define S(X) STRINGIFY(X)

const char *ssid = S(MY_SSID);
const char *password = S(MY_PSWD);

const char *Sys::hostname() {
  static String _hostname;
  if (_hostname.length() == 0) {
      _hostname="ESP82-"+String(ESP.getChipId()&0xFFFF);
  }
  return _hostname.c_str();
}
uint64_t Sys::millis() { return ::millis(); }

Thread thisThread;
MqttUdp mqttUdp("192.168.0.207", 1883); 
LedBlinker ledBlinker(100, D0);

class Counter : public ValueFlow<uint32_t> {
  uint32_t _counter = 0;

public:
  Counter(){};
  void request() { this->emit(_counter++); }
  void onNext(uint32_t x){};
};
class Poller : public TimerSource, public Sink<TimerMsg> {
  std::vector<Requestable *> _requestables;
  uint32_t _idx = 0;

public:
  ValueFlow<bool> run = false;
  Poller(uint32_t iv) : TimerSource(1, 1000, true) {
    interval(iv);
    *this >> *this;
  };

  void onNext(const TimerMsg &tm) {
    _idx++;
    if (_idx >= _requestables.size())
      _idx = 0;
    if (_requestables.size() && run()) {
      _requestables[_idx]->request();
    }
  }

  Poller &operator()(Requestable &rq) {
    _requestables.push_back(&rq);
    return *this;
  }
};

Counter counter;
Poller poller(10);

void setup() {
  Serial.begin(115200);
  Serial.println();

  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("connected");
  INFO("connected");
  mqttUdp.init();
  ledBlinker.init();
  mqttUdp.observeOn(thisThread);
  mqttUdp.connected >> ledBlinker.blinkSlow;
  mqttUdp.connected >> poller.run;
  counter >> mqttUdp.toTopic<uint32_t>("system/counter");
  poller(counter);
  thisThread | poller;
  thisThread | ledBlinker;
}

void loop() { thisThread.run(); }