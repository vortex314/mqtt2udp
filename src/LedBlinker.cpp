#include "LedBlinker.h"


LedBlinker::LedBlinker(uint32_t pin, uint32_t delay) : TimerSource(1,delay,true)
{
    _pin = pin;
    blinkSlow.handler([=](bool flag) {
        if ( flag ) interval(500);
        else interval(100);
    });
    *this >> *this; // consume my own TimerMsg
}
void LedBlinker::init()
{
   pinMode(LED, OUTPUT); // LED pin as output.
}
void LedBlinker::onNext(const TimerMsg& m)
{
digitalWrite(LED_BUILTIN, _on);
    _on = ! _on;
}
void LedBlinker::delay(uint32_t d)
{
    interval(d);
}


