#include <Arduino.h>

#ifndef _TIMER_H
#define _TIMER_H


typedef enum enmDayOfWeek {
  monday = 0x01,
  tuesday = 0x02,
  wednesday = 0x04,
  thursday = 0x08,
  friday = 0x10,
  saturday = 0x20,
  sunday = 0x40
} eDoW;

/// @brief 
typedef class daysOfWeek {
public:
  union
  {
    uint8_t mo : 1,
            tu : 1,
            we : 1,
            th : 1,
            fr : 1,
            sa : 1,
            su : 1,
            pad : 1;
    uint8_t allDays;
  };

  daysOfWeek() { this->allDays = 0; };
  daysOfWeek( uint8_t defVal ) : allDays( defVal ) {  };

} daysOfWeek_t;

// Timer-Datenstruktur
// Timer id '-1' means not set; ignore this Timer
struct Timer {
    
//  int id;
  byte active;
  short sStarttime;   // the format is Hi-byte means the hour and the Low-byte means the minute e.g. 0D34 means time of Day 13:52
  short sStoptime;    // the format is Hi-byte means the hour and the Low-byte means the minute e.g. 0D34 means time of Day 13:52 
  daysOfWeek_t days;  // bit is set which day of the week the time shall be used
  // String repeatDays[7];
  // String interval;

  Timer() { active = 0; sStarttime = 0; sStoptime = 0; days.allDays = 0; };
  
  void setStartTime( String& strTime ) { sStarttime = timeStringToShort( strTime); };
  void setStopTime( String& strTime ) { sStoptime = timeStringToShort( strTime); };
  String getStartTime( ) { return shortToTimeString( sStarttime ); };
  String getStopTime( ) { return shortToTimeString( sStoptime ); };

  short timeStringToShort( String& srTime );
  String shortToTimeString( short encodedTime );

  time_t parseISO8601(const char* iso8601 /*= NULL*/ );
};

#endif //_TIMER_H