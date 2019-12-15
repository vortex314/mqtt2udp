//_______________________________________________________________________________________________________________
//
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <Streams.h>


typedef struct MqttMessage {
  String topic;
  String message;
} MqttMessage;
//____________________________________________________________________________________________________________
//
template <class T> class ToMqtt : public Flow<T, MqttMessage> {
  String _name;

public:
  ToMqtt(String name) : _name(name){};
  void onNext(const T& event) {
    String s;
    DynamicJsonDocument doc(100);
    JsonVariant variant = doc.to<JsonVariant>();
    variant.set(event);
    serializeJson(doc, s);
    this->emit({_name, s});
    // emit doesn't work as such
    // https://stackoverflow.com/questions/9941987/there-are-no-arguments-that-depend-on-a-template-parameter
  }
  void request(){};
};

//_______________________________________________________________________________________________________________
//
template <class T> class FromMqtt : public Flow<MqttMessage, T> {
  String _name;

public:
  FromMqtt(String name) : _name(name){};
  void onNext(const MqttMessage& mqttMessage) {
    if (mqttMessage.topic != _name) {
      return;
    }
    DynamicJsonDocument doc(100);
    auto error = deserializeJson(doc, mqttMessage.message);
    if (error) {
      WARN(" failed JSON parsing '%s' : '%s' ", mqttMessage.message.c_str(),
           error.c_str());
      return;
    }
    JsonVariant variant = doc.as<JsonVariant>();
    if (variant.isNull()) {
      WARN(" is not a JSON variant '%s' ", mqttMessage.message.c_str());
      return;
    }
    if (variant.is<T>() == false) {
      WARN(" message '%s' JSON type doesn't match.",
           mqttMessage.message.c_str());
      return;
    }
    T value = variant.as<T>();
    this->emit(value);
    // emit doesn't work as such
    // https://stackoverflow.com/questions/9941987/there-are-no-arguments-that-depend-on-a-template-parameter
  }
  void request(){};
};

class MqttUdp :  public Sink<TimerMsg>,public Flow<MqttMessage, MqttMessage> {
public:
private:
  StaticJsonDocument<256> txd;
  StaticJsonDocument<256> rxd;
  char _incomingPacket[1024];
  String rxdString;
  String _loopbackTopic;
  String _host;
  uint16_t _port;
  uint64_t _loopbackReceived;
  String _hostPrefix;

  enum { CMD_SUBSCRIBE = 0, CMD_PUBLISH };


public:
  ValueFlow<bool> connected=false;
  AsyncFlow<MqttMessage> incoming;
  AsyncFlow<MqttMessage> outgoing;
  TimerSource keepAliveTimer;
  TimerSource connectTimer;
  MqttUdp(String host,uint16_t port);
  ~MqttUdp();
  void init();
  void checkUdp();
  void recvUdp(String s);
  void publish(String topic, String message);
  void subscribe(String topic);
  void sendUdp();
  void onNext(const MqttMessage&);
  void onNext(const TimerMsg&);
  void request();
  void observeOn(Thread&);
  template <class T> Sink<T> &toTopic(const char *name) {
    return *(new ToMqtt<T>(name)) >> outgoing;
  }
  template <class T> Source<T> &fromTopic(const char *name) {
    auto newSource = new FromMqtt<T>(name);
    incoming >> *newSource;
    return *newSource;
  }
};

int freeMemory();
