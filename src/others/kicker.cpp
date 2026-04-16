#include "config/config.h"
#include "others.h"

#include <Arduino.h>

void kicker_kick(){
    digitalWrite(KICKER_PIN, HIGH);   
    delay(40);               
    digitalWrite(KICKER_PIN, LOW); 
}