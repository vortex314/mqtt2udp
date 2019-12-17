#include <MqttUdp.h>
WiFiUDP wifiUdp;

#define TIMER_KEEP_ALIVE 1
#define TIMER_CONNECT 2

MqttUdp::MqttUdp(String host, uint16_t port)
    : connected(false), incoming(20), outgoing(20),
      keepAliveTimer(TIMER_KEEP_ALIVE, 1000, true),
      connectTimer(TIMER_CONNECT, 5000, true) {
  INFO("constructor");
  _host = host;
  _port = port;
}
MqttUdp::~MqttUdp() {}

void MqttUdp::init() {
  wifiUdp.begin(_port);
  INFO("MqttUdp started. ");
  txd.clear();
  rxd.clear();
  _hostPrefix = "dst/";
  _hostPrefix += Sys::hostname();
  _hostPrefix += "/";
  _loopbackTopic = _hostPrefix + "system/loopback";
  _loopbackReceived = 0;

  outgoing >> *this;
  *this >> incoming;
  Sink<TimerMsg> &me = *this;
  keepAliveTimer >> me;
  connectTimer >> me;
  connected.emitOnChange(true);
}

void MqttUdp::onNext(const TimerMsg &tm) {
  // LOG(" timer : %lu ",tm.id);
  switch (tm.id) {
  case TIMER_KEEP_ALIVE: {
    publish(_loopbackTopic, "true");
    outgoing.onNext({"system/alive", "true"});
    break;
  };
  case TIMER_CONNECT: {
    if (millis() > (_loopbackReceived + 2000)) {
      WARN(" lost mqtt connection %s ", _host.c_str());
      connected = false;
      String s = "dst/";
      s += Sys::hostname();
      s += "/#";
      subscribe(s);
      publish(_loopbackTopic, "true");
    } else {
      connected = true;
    }
    break;
  };
  default: { WARN(" invalid timer id : %d", tm.id); }
  }
}

void MqttUdp::checkUdp() {
  int packetSize = wifiUdp.parsePacket();
  if (packetSize) {
    // receive incoming UDP packets
    INFO("Received %d bytes from %s, port %d heap : %d time %lu ", packetSize,
         wifiUdp.remoteIP().toString().c_str(), wifiUdp.remotePort(),
         ESP.getFreeHeap(), millis());
    int len = wifiUdp.read(_incomingPacket, sizeof(_incomingPacket));
    if (len > 0) {
      _incomingPacket[len] = 0;
    }
    recvUdp(_incomingPacket);
  }
}

void MqttUdp::onNext(const MqttMessage &m) {
  if (connected()) {
    String s = "src/";
    s += Sys::hostname();
    s += "/" + m.topic;
    publish(s, m.message);
  };
}

void MqttUdp::request() {
  checkUdp();
  if (connected()) {
    incoming.request();
    outgoing.request();
  }
}

void MqttUdp::observeOn(Thread &thread) {
  thread.addTimer(&keepAliveTimer);
  thread.addTimer(&connectTimer);
  thread.addRequestable(*this);
}

void MqttUdp::recvUdp(String s) {
  if (s.length() > 0) {
    deserializeJson(rxd, s);
    JsonArray array = rxd.as<JsonArray>();
    if (!array.isNull()) {
      String topic = array[1];
      if (array[1].as<String>() == _loopbackTopic) {
        _loopbackReceived = millis();
        connected = true;
      } else {
        String topic = array[1];
        emit({topic.substring(_hostPrefix.length()), array[2]});
      }
    }
  }
}

void MqttUdp::publish(String topic, String message) {
  txd.clear();
  txd.add((int)CMD_PUBLISH);
  txd.add(topic);
  txd.add(message);
  sendUdp();
}

void MqttUdp::subscribe(String topic) {
  txd.clear();
  txd.add((int)CMD_SUBSCRIBE);
  txd.add(topic);
  sendUdp();
}

void MqttUdp::sendUdp() {
  String output = "";
  serializeJson(txd, output);
  wifiUdp.beginPacket(_host.c_str(), _port);
  wifiUdp.write(output.c_str());
  wifiUdp.endPacket();
}
