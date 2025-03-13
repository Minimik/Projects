#include <Arduino.h>
#include "Timer.h"

#ifndef _RELAY_H
#define _RELAY_H

const int TIMER_PER_RELAY = 6;

// Relais-Datenstruktur
struct Relay {
    Relay() : m_Id(-1) {};
    Relay( uint16 id );

    int processTimers( );


    int m_Id;
    String m_Name;
    String m_State;
    String m_Mode;
    Timer m_Timers[TIMER_PER_RELAY]; // Maximal 5 Timer pro Relais
  };

#endif //_RELAY_H