//-----------------------------------------------------------------------------
// File: settings.h
//
// Desc: The settings of the My Pictures Slideshow screen saver, kept in
//       HKEY_CURRENT_USER\Control Panel\Screen Saver.Slideshow.  (0x2b0 bytes)
//-----------------------------------------------------------------------------
#ifndef _MYPICS_SETTINGS_H
#define _MYPICS_SETTINGS_H

#include "util.h"

#define REGSTR_PATH_SLIDESHOW   L"Control Panel\\Screen Saver.Slideshow"

class CSlideshowSettings
{
public:
    CSlideshowSettings( HKEY hRoot, const CStr& strKey );
    ~CSlideshowSettings() {}

    void    Read();
    void    Write();

    CStr    GetImageDirectory() const               { return m_strImageDirectory; }
    void    SetImageDirectory( const CStr& str )    { m_strImageDirectory = str; }

    UINT    GetPaintInterval() const                { return m_nPaintInterval; }
    UINT    GetChangeInterval() const               { return m_nChangeInterval; }
    void    SetChangeInterval( UINT n )             { m_nChangeInterval = n; }
    bool    GetDisplayFilename() const              { return m_bDisplayFilename; }
    void    SetDisplayFilename( bool b )            { m_bDisplayFilename = b; }
    int     GetMaxScreenPercent() const             { return m_nMaxScreenPercent; }
    void    SetMaxScreenPercent( int n )            { m_nMaxScreenPercent = n; }
    bool    GetDisableTransitions() const           { return m_bDisableTransitions; }
    void    SetDisableTransitions( bool b )         { m_bDisableTransitions = b; }
    bool    GetAllowStretching() const              { return m_bAllowStretching; }
    void    SetAllowStretching( bool b )            { m_bAllowStretching = b; }
    bool    GetAllowKeyboardControl() const         { return m_bAllowKeyboardControl; }
    void    SetAllowKeyboardControl( bool b )       { m_bAllowKeyboardControl = b; }
    int     GetMaxFailedFiles() const               { return m_nMaxFailedFiles; }
    int     GetMaxSuccessfulFiles() const           { return m_nMaxSuccessfulFiles; }
    int     GetMaxDirectories() const               { return m_nMaxDirectories; }

private:
    static CStr DefaultImageDirectory();

    HKEY    m_hRoot;                        // +0x000
    CStr    m_strKey;                       // +0x008
    CStr    m_strImageDirectory;            // +0x120
    UINT    m_nPaintInterval;               // +0x238 ms between transition steps
    UINT    m_nChangeInterval;              // +0x23c ms between pictures
    bool    m_bDisplayFilename;             // +0x240
    int     m_nMaxScreenPercent;            // +0x244
    bool    m_bDisableTransitions;          // +0x248
    bool    m_bAllowStretching;             // +0x249
    bool    m_bAllowKeyboardControl;        // +0x24a
    int     m_nMaxFailedFiles;              // +0x24c the search stops after so many
    int     m_nMaxSuccessfulFiles;          // +0x250 ... (0: no limit)
    int     m_nMaxDirectories;              // +0x254

    // value names
    LPCWSTR m_pszImageDirectory;            // +0x258
    LPCWSTR m_pszPaintInterval;
    LPCWSTR m_pszChangeInterval;
    LPCWSTR m_pszDisplayFilename;
    LPCWSTR m_pszMaxScreenPercent;
    LPCWSTR m_pszDisableTransitions;
    LPCWSTR m_pszAllowStretching;
    LPCWSTR m_pszAllowKeyboardControl;
    LPCWSTR m_pszMaxFailedFiles;
    LPCWSTR m_pszMaxSuccessfulFiles;
    LPCWSTR m_pszMaxDirectories;            // +0x2a8
};

#endif // _MYPICS_SETTINGS_H
