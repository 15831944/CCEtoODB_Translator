// $Header: /CAMCAD/4.3/Status.cpp 7     8/12/03 7:25p Kurt Van Ness $

/*****************************************************************************/
/*  Project CAMCAD
    Router Solutions Inc.
    Copyright � 1994-97. All Rights Reserved.
*/           

#include "stdafx.h"
#include "status.h"
//#include "Debug.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/******************************************************************************
* CProgressBar
*/
IMPLEMENT_DYNCREATE(CProgressBar, CProgressCtrl)

BEGIN_MESSAGE_MAP(CProgressBar, CProgressCtrl)
    //{{AFX_MSG_MAP(CProgressBar)
    ON_WM_ERASEBKGND()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

CProgressBar::CProgressBar()
{
    m_Rect.SetRect(0,0,0,0);
}

CProgressBar::CProgressBar(LPCTSTR strMessage, int nSize /* = 100 */, 
                           int MaxValue /* = 100 */, BOOL bSmooth /* = FALSE */)
{
    Create(strMessage, nSize, MaxValue, bSmooth);
}

CProgressBar::~CProgressBar()
{
    Clear();
}

CStatusBar* CProgressBar::GetStatusBar()
{
    CStatusBar* pStatusBar = NULL;

    CWnd *pMainWnd = AfxGetMainWnd();
    if (!pMainWnd)
        return NULL;

    pStatusBar = (CStatusBar*) pMainWnd->GetDescendantWindow(AFX_IDW_STATUS_BAR);
    
    // - OR -
    // if (!pMainWnd->IsKindOf(RUNTIME_CLASS(CFrameWnd)))
    // pStatusBar = (CStatusBar*) ((CFrameWnd*) pFrame)->GetMessageBar();

    if (!pStatusBar || !pStatusBar->IsKindOf(RUNTIME_CLASS(CStatusBar))) 
        return NULL;

    return pStatusBar;
}

// Create the CProgressCtrl as a child of the status bar positioned
// over the first pane, extending "nSize" percentage across pane.
// Sets the range to be 0 to MaxValue, with a step of 1.
BOOL CProgressBar::Create(LPCTSTR strMessage, int nSize, int MaxValue, 
                          BOOL bSmooth /*=FALSE*/)
{
    BOOL bSuccess = FALSE;
    
    CStatusBar *pStatusBar = GetStatusBar();
    if (!pStatusBar) return FALSE;

    DWORD dwStyle = WS_CHILD|WS_VISIBLE;
#ifdef PBS_SMOOTH    
    if (bSmooth)
       dwStyle |= PBS_SMOOTH;
#endif

    // Create the progress bar
    bSuccess = CProgressCtrl::Create(dwStyle, CRect(0,0,0,0), pStatusBar, 1);
    ASSERT(bSuccess);
    if (!bSuccess) return FALSE;

    // Set range and step
    SetRange(0, MaxValue);
    SetStep(1);

    m_strMessage = strMessage;
    m_nSize      = nSize;

    // Resize the control to its desired width
    Resize();

    return TRUE;
}

void CProgressBar::Clear()
{
    if (!IsWindow(GetSafeHwnd()))
        return;

    // Hide the window. This is necessary so that a cleared
    // window is not redrawn if "Resise" is called
    ModifyStyle(WS_VISIBLE, 0);

    // Get the IDLE_MESSAGE
    CString str;
    str.LoadString(AFX_IDS_IDLEMESSAGE);

    // Place the IDLE_MESSAGE in the status bar 
    CStatusBar *pStatusBar = GetStatusBar();
    if (pStatusBar)
       pStatusBar->SetWindowText(str);
}

BOOL CProgressBar::SetText(LPCTSTR strMessage)
{ 
    m_strMessage = strMessage; 
    return Resize(); 
}

BOOL CProgressBar::SetSize(int nSize)
{
    m_nSize = nSize; 
    return Resize();
}

COLORREF CProgressBar::SetBarColour(COLORREF clrBar)
{
#ifdef PBM_SETBKCOLOR
    if (!IsWindow(GetSafeHwnd()))
        return 0;

    return SendMessage(PBM_SETBARCOLOR, 0, (LPARAM) clrBar);
#else
    UNUSED(clrBar);
    return CLR_DEFAULT;
#endif
}

COLORREF CProgressBar::SetBkColour(COLORREF clrBk)
{
#ifdef PBM_SETBKCOLOR
    if (!IsWindow(GetSafeHwnd()))
        return 0;

    return SendMessage(PBM_SETBKCOLOR, 0, (LPARAM) clrBk);
#else
    UNUSED(clrBk);
    return CLR_DEFAULT;
#endif
}

BOOL CProgressBar::SetRange(int nLower, int nUpper, int nStep /* = 1 */)    
{    
    if (!IsWindow(GetSafeHwnd()))
        return FALSE;

    // To take advantage of the Extended Range Values we use the PBM_SETRANGE32
    // message intead of calling CProgressCtrl::SetRange directly. If this is
    // being compiled under something less than VC 5.0, the necessary defines
    // may not be available.

#ifdef PBM_SETRANGE32
    ASSERT(-0x7FFFFFFF <= nLower && nLower <= 0x7FFFFFFF);
    ASSERT(-0x7FFFFFFF <= nUpper && nUpper <= 0x7FFFFFFF);
    SendMessage(PBM_SETRANGE32, (WPARAM) nLower, (LPARAM) nUpper);
#else
    ASSERT(0 <= nLower && nLower <= 65535);
    ASSERT(0 <= nUpper && nUpper <= 65535);
    CProgressCtrl::SetRange(nLower, nUpper);
#endif
    
    CProgressCtrl::SetStep(nStep);
    return TRUE;
}

int CProgressBar::SetPos(int nPos)     
{
    if (!IsWindow(GetSafeHwnd()))
        return 0;

#ifdef PBM_SETRANGE32
    ASSERT(-0x7FFFFFFF <= nPos && nPos <= 0x7FFFFFFF);
#else
    ASSERT(0 <= nPos && nPos =< 65535);
#endif

    ModifyStyle(0,WS_VISIBLE);
    return CProgressCtrl::SetPos(nPos);
}

int  CProgressBar::OffsetPos(int nPos) 
{ 
    if (!IsWindow(GetSafeHwnd()))
        return 0;

    ModifyStyle(0,WS_VISIBLE);
    return CProgressCtrl::OffsetPos(nPos);
}

int  CProgressBar::SetStep(int nStep)
{ 
    if (!IsWindow(GetSafeHwnd()))
        return 0;

    ModifyStyle(0,WS_VISIBLE);
    return CProgressCtrl::SetStep(nStep);    
}

int  CProgressBar::StepIt()             
{ 
    if (!IsWindow(GetSafeHwnd()))
        return 0;

    ModifyStyle(0,WS_VISIBLE);
    return CProgressCtrl::StepIt();    
}

BOOL CProgressBar::Resize() 
{
    if (!IsWindow(GetSafeHwnd()))
        return FALSE;

    CStatusBar *pStatusBar = GetStatusBar();
    if (!pStatusBar) return FALSE;

    // Redraw the window text
    if (IsWindowVisible()) {
        pStatusBar->SetWindowText(m_strMessage);
        pStatusBar->UpdateWindow();
    }

    // Calculate how much space the text takes up
    CClientDC dc(pStatusBar);
    CFont *pOldFont = dc.SelectObject(pStatusBar->GetFont());
    CSize size = dc.GetTextExtent(m_strMessage);        // Length of text
    int margin = dc.GetTextExtent(_T(" ")).cx * 2;      // Text margin
    dc.SelectObject(pOldFont);

    // Now calculate the rectangle in which we will draw the progress bar
    CRect rc;
    pStatusBar->GetItemRect (0, rc);
    rc.left = size.cx + 2*margin;
    rc.right = rc.left + (rc.right - rc.left) * m_nSize / 100;
    if (rc.right < rc.left) rc.right = rc.left;
    
    // Leave a litle vertical margin (10%) between the top and bottom of the bar
    int Height = rc.bottom - rc.top;
    rc.bottom -= Height/10;
    rc.top    += Height/10;

    // If the window size has changed, resize the window
    if (rc != m_Rect) {
        MoveWindow(&rc);
      m_Rect = rc;
    }

    return TRUE;
}

BOOL CProgressBar::OnEraseBkgnd(CDC* pDC) 
{
    Resize();
    return CProgressCtrl::OnEraseBkgnd(pDC);
}


/******************************************************************************
* CEditBar
*/
IMPLEMENT_DYNCREATE(CEditBar, CEdit)

BEGIN_MESSAGE_MAP(CEditBar, CEdit)
    //{{AFX_MSG_MAP(CEditBar)
    ON_WM_ERASEBKGND()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

CEditBar::CEditBar()
: m_Rect(0,0,0,0)
, m_nSize(0)
{
}

CEditBar::CEditBar(LPCTSTR strMessage, int nSize /* = 100 */)
: m_Rect(0,0,0,0)
, m_nSize(nSize)
{
   Create(strMessage, nSize);
}

CEditBar::~CEditBar()
{
   Clear();
   DestroyWindow();
}

CStatusBar* CEditBar::GetStatusBar()
{
   CStatusBar* pStatusBar = NULL;

   CWnd *pMainWnd = AfxGetMainWnd();
   if (!pMainWnd)
     return NULL;

   pStatusBar = (CStatusBar*) pMainWnd->GetDescendantWindow(AFX_IDW_STATUS_BAR);

   // - OR -
   // if (!pMainWnd->IsKindOf(RUNTIME_CLASS(CFrameWnd)))
   // pStatusBar = (CStatusBar*) ((CFrameWnd*) pFrame)->GetMessageBar();

   if (!pStatusBar || !pStatusBar->IsKindOf(RUNTIME_CLASS(CStatusBar))) 
      return NULL;

   return pStatusBar;
}

// Create the CProgressCtrl as a child of the status bar positioned
// over the first pane, extending "nSize" percentage across pane.
// Sets the range to be 0 to MaxValue, with a step of 1.
BOOL CEditBar::Create(LPCTSTR strMessage, int nSize)
{
   BOOL bSuccess = FALSE;

   CStatusBar *pStatusBar = GetStatusBar();
   if (!pStatusBar) return FALSE;

   DWORD dwStyle = WS_CHILD|WS_VISIBLE|WS_BORDER|WS_DISABLED;

   // Create the progress bar
   bSuccess = CEdit::Create(dwStyle, CRect(0,0,0,0), pStatusBar, 1);
   ASSERT(bSuccess);

   //CDebug::writefTimeStamp("%s m_hWnd=0x%08x\n",__FUNCTION__,this->m_hWnd);

   if (!bSuccess) return FALSE;

   m_strMessage = strMessage;
   m_nSize      = nSize;

   // Resize the control to its desired width
   Resize();

   return TRUE;
}

void CEditBar::Clear()
{
   if (!IsWindow(GetSafeHwnd()))
      return;

   // Hide the window. This is necessary so that a cleared
   // window is not redrawn if "Resise" is called
   ModifyStyle(WS_VISIBLE, 0);

   // Get the IDLE_MESSAGE
   CString str;
   str.LoadString(AFX_IDS_IDLEMESSAGE);

   // Place the IDLE_MESSAGE in the status bar 
   CStatusBar *pStatusBar = GetStatusBar();
   if (pStatusBar)
   {
      pStatusBar->SetWindowText(str);
      pStatusBar->Invalidate();
   }
}

BOOL CEditBar::SetText(LPCTSTR strMessage)
{
   m_strMessage = strMessage; 
   return Resize(); 
}

BOOL CEditBar::SetSize(int nSize)
{
   m_nSize = nSize; 
   return Resize();
}

BOOL CEditBar::Resize() 
{
   if (!IsWindow(GetSafeHwnd()))
      return FALSE;

   CStatusBar *pStatusBar = GetStatusBar();
   if (!pStatusBar) return FALSE;

   // Redraw the window text
   if (IsWindowVisible())
   {
      pStatusBar->SetWindowText(m_strMessage);
      pStatusBar->UpdateWindow();
   }

   // Calculate how much space the text takes up
   CClientDC dc(pStatusBar);
   CFont *pOldFont = dc.SelectObject(pStatusBar->GetFont());
   CSize size = dc.GetTextExtent(m_strMessage);        // Length of text
   int margin = dc.GetTextExtent(_T(" ")).cx * 2;      // Text margin
   dc.SelectObject(pOldFont);

   // Now calculate the rectangle in which we will draw the progress bar
   CRect rc;
   pStatusBar->GetItemRect (0, rc);
   rc.left = size.cx + 2*margin;
   rc.right = rc.left + (rc.right - rc.left) * m_nSize / 100;
   if (rc.right < rc.left) rc.right = rc.left;

   // Leave a litle vertical margin (10%) between the top and bottom of the bar
/* int Height = rc.bottom - rc.top;
   rc.bottom -= Height/10;
   rc.top    += Height/10;*/

   // If the window size has changed, resize the window
   if (rc != m_Rect)
   {
      MoveWindow(&rc);
      m_Rect = rc;
   }

   return TRUE;
}

BOOL CEditBar::OnEraseBkgnd(CDC* pDC) 
{
   Resize();
   return CEdit::OnEraseBkgnd(pDC);
}

// end STATUS.CPP
