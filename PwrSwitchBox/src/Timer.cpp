#include "Timer.h"

time_t Timer::parseISO8601( const char* iso8601 = NULL )
{
  // struct tm t = {};
  // if (strptime( (iso8601)? this->time.c_str() : iso8601, "%Y-%m-%dT%H:%M:%S", &t ) )
  // {
  //     return mktime( &t );  // Konvertiere zu time_t
  // }
  return 0;  // Fehlerfall
}

short Timer::timeStringToShort( String& srTime )
{
  //Serial.println( srTime );
  int hours = srTime.substring(0, 2).toInt();   // "14" → 14
  int minutes = srTime.substring(3).toInt(); // "30" → 30
  //Serial.println( String( hours ) + ":" + String( minutes ) );
  return (hours << 8) | minutes;  // Stunden ins High-Byte, Minuten ins Low-Byte
}

String Timer::shortToTimeString( short encodedTime )
{
  int hours = (encodedTime >> 8) & 0xFF;  // High-Byte extrahieren
  int minutes = encodedTime & 0xFF;      // Low-Byte extrahieren
  return String( (unsigned char)hours ) + ":" + String( (unsigned char) minutes );
}
