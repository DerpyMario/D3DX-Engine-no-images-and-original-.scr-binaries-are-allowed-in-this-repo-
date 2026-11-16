//-----------------------------------------------------------------------------
// File: dxutil9.cpp
//
// Desc: Timer helper used by the D3DSaver9 framework.
//
//       Reconstructed from dxutil9.obj (486 bytes of code, identical in all
//       four Direct3D screen savers).
//
//       Original object:
//           shell\osshell\control\scrnsave\d3dsaver9\objfre\amd64\dxutil9.obj
//-----------------------------------------------------------------------------
#include <windows.h>
#include <mmsystem.h>
#include "dxutil9.h"




//-----------------------------------------------------------------------------
// Name: DXUtil_Timer()
// Desc: Performs timer operations.  Uses QueryPerformanceCounter() when it is
//       available and falls back to timeGetTime() otherwise.
//-----------------------------------------------------------------------------
FLOAT __cdecl DXUtil_Timer( TIMER_COMMAND command )
{
    static bool     m_bTimerInitialized = false;
    static bool     m_bUsingQPF         = false;
    static bool     m_bTimerStopped     = true;
    static LONGLONG m_llQPFTicksPerSec  = 0;

    // Initialize the timer
    if( !m_bTimerInitialized )
    {
        m_bTimerInitialized = true;

        // Use QueryPerformanceFrequency() to get frequency of timer.  If QPF is
        // not supported, we will timeGetTime() which returns milliseconds.
        LARGE_INTEGER qwTicksPerSec;
        m_bUsingQPF = ( QueryPerformanceFrequency( &qwTicksPerSec ) != 0 );
        if( m_bUsingQPF )
            m_llQPFTicksPerSec = qwTicksPerSec.QuadPart;
    }

    if( m_bUsingQPF )
    {
        static LONGLONG m_llStopTime        = 0;
        static LONGLONG m_llLastElapsedTime = 0;
        static LONGLONG m_llBaseTime        = 0;
        double fTime;
        double fElapsedTime;
        LARGE_INTEGER qwTime;

        // Get either the current time or the stop time, depending
        // on whether we're stopped and what command was sent
        if( m_llStopTime != 0 && command != TIMER_START && command != TIMER_GETABSOLUTETIME )
            qwTime.QuadPart = m_llStopTime;
        else
            QueryPerformanceCounter( &qwTime );

        // Return the elapsed time
        if( command == TIMER_GETELAPSEDTIME )
        {
            fElapsedTime = (double)( qwTime.QuadPart - m_llLastElapsedTime ) / (double)m_llQPFTicksPerSec;
            m_llLastElapsedTime = qwTime.QuadPart;
            return (FLOAT)fElapsedTime;
        }

        // Return the current time
        if( command == TIMER_GETAPPTIME )
        {
            double fAppTime = (double)( qwTime.QuadPart - m_llBaseTime ) / (double)m_llQPFTicksPerSec;
            return (FLOAT)fAppTime;
        }

        // Start the timer
        if( command == TIMER_START )
        {
            if( m_bTimerStopped )
                m_llBaseTime += qwTime.QuadPart - m_llStopTime;
            m_llStopTime = 0;
            m_llLastElapsedTime = qwTime.QuadPart;
            m_bTimerStopped = false;
            return 0.0f;
        }

        if( command == TIMER_GETABSOLUTETIME )
        {
            fTime = qwTime.QuadPart / (double)m_llQPFTicksPerSec;
            return (FLOAT)fTime;
        }

        return -1.0f; // Invalid command specified
    }
    else
    {
        // Get the time using timeGetTime()
        static double m_fLastElapsedTime = 0.0;
        static double m_fBaseTime        = 0.0;
        static double m_fStopTime        = 0.0;
        double fTime;
        double fElapsedTime;

        // Get either the current time or the stop time, depending
        // on whether we're stopped and what command was sent
        if( m_fStopTime != 0.0 && command != TIMER_START && command != TIMER_GETABSOLUTETIME )
            fTime = m_fStopTime;
        else
            fTime = timeGetTime() * 0.001;

        // Return the elapsed time
        if( command == TIMER_GETELAPSEDTIME )
        {
            fElapsedTime = (double)( fTime - m_fLastElapsedTime );
            m_fLastElapsedTime = fTime;
            return (FLOAT)fElapsedTime;
        }

        // Return the current time
        if( command == TIMER_GETAPPTIME )
        {
            return (FLOAT)( fTime - m_fBaseTime );
        }

        // Start the timer
        if( command == TIMER_START )
        {
            if( m_bTimerStopped )
                m_fBaseTime += fTime - m_fStopTime;
            m_fStopTime = 0.0;
            m_fLastElapsedTime = fTime;
            m_bTimerStopped = false;
            return 0.0f;
        }

        if( command == TIMER_GETABSOLUTETIME )
        {
            return (FLOAT)fTime;
        }

        return -1.0f; // Invalid command specified
    }
}
