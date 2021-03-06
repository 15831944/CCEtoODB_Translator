// $Header: /CAMCAD/4.5/Splash.h 11    10/10/06 3:20p Kurt Van Ness $

// CG: This file was added by the Splash Screen component.

#pragma once

/////////////////////////////////////////////////////////////////////////////
//   Splash Screen class

class CSplashWnd : public CWnd
{
// Construction
protected:
   CSplashWnd();


// Attributes:
public:
   CBitmap m_bitmap;
   BOOL canClose;
   static void SetClosable();

// Operations
public:
   static void EnableSplashScreen(BOOL bEnable = TRUE);
   static void ShowSplashScreen(CWnd* pParentWnd = NULL,CRect* monitorRect = NULL);
   static void PreTranslateAppMessage(MSG* pMsg);

// Overrides
   // ClassWizard generated virtual function overrides
   //{{AFX_VIRTUAL(CSplashWnd)
   //}}AFX_VIRTUAL

// Implementation
public:
   ~CSplashWnd();
   virtual void PostNcDestroy();

protected:
   BOOL Create(CWnd* pParentWnd = NULL,CRect* monitorRect = NULL);
   void HideSplashScreen();
   static BOOL c_bShowSplashWnd;
   static CSplashWnd* c_pSplashWnd;

// Generated message map functions
protected:
   //{{AFX_MSG(CSplashWnd)
   afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
   afx_msg void OnPaint();
   afx_msg void OnTimer(UINT nIDEvent);
   //}}AFX_MSG
   DECLARE_MESSAGE_MAP()
};

