#include <NTPClient.h>
#include "Relay.h"

extern NTPClient timeClient;

Relay::Relay( uint16 id )
{
    m_Id = id;
    m_State = "on";
    m_Name = "";
    m_Mode = "manual";     
}


int Relay::processTimers( )
{
  int iTargetRelayState = 0;

  if ( m_Id  != -1 )
  {

    for ( uint t = 0; t < TIMER_PER_RELAY; t++ )
    {

      if ( m_Timers[t].active != 0 )
      {
        
        // time_t tmTimer = relays[i].timers[t].parseISO8601();
        time_t ntpTime = timeClient.getEpochTime();
        //Serial.println( timeClient.getFormattedTime() );

        struct tm* localTime = localtime( &ntpTime );
        //Serial.printf( "Start Hour: %02d : %02d\t",  localTime->tm_hour, (relays[ nmbRelay ].timers[t].sStarttime >> 8 ) & 0xFF );
        //Serial.printf( "Start Min: %02d : %02d\t",  localTime->tm_min, (relays[ nmbRelay ].timers[t].sStarttime )  & 0xFF );
        //Serial.printf( "Stop Hour: %02d : %02d\t",  localTime->tm_hour, (relays[ nmbRelay ].timers[t].sStoptime >> 8 ) & 0xFF );
        //Serial.printf( "Stop Min: %02d : %02d\n",  localTime->tm_min, (relays[ nmbRelay ].timers[t].sStoptime )  & 0xFF );

        bool bStartH = localTime->tm_hour >= (( m_Timers[t].sStarttime >> 8 )  & 0xFF );
        bool bStopH  = localTime->tm_hour <= (( m_Timers[t].sStoptime >> 8  ) & 0xFF );
        bool bStartM =  localTime->tm_min >= (( m_Timers[t].sStarttime )  & 0xFF );
        bool bStopM =  localTime->tm_min <= (( m_Timers[t].sStoptime )  & 0xFF );

        Serial.printf( "stH: %d; stM: %d; spH: %d; spM: %d\n", bStartH, bStartM, bStopH, bStopM );
        if ( bStartH    &&  bStartM && bStopH && bStopM )
        {
          //Serial.println( "timecheck done...");
          if ( localTime->tm_wday != 0 )
          {
            iTargetRelayState |= ( 1 << (localTime->tm_wday - 1) ) & ( m_Timers[t].days.allDays ) ? 1 : 0;
          }
          else
          {
            iTargetRelayState |= m_Timers[t].days.su;
          }

        }
        else
        {
          iTargetRelayState |= 0;
        }
      }
    }
  }

  return iTargetRelayState;   
}
