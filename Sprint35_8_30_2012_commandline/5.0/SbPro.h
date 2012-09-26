// $Header: /CAMCAD/4.3/SbPro.h 7     8/12/03 9:06p Kurt Van Ness $

#if !defined(AFX_SBPRO_H__1D6410E5_98B7_11D2_9866_004005408E44__INCLUDED_)
#define AFX_SBPRO_H__1D6410E5_98B7_11D2_9866_004005408E44__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Machine generated IDispatch wrapper class(es) created by Microsoft Visual C++

// NOTE: Do not modify the contents of this file.  If this class is regenerated by
//  Microsoft Visual C++, your modifications will be overwritten.

// Dispatch interfaces referenced by this interface
class COleFont;

/////////////////////////////////////////////////////////////////////////////
// CSbpro wrapper class

class CSbpro : public CWnd
{
protected:
   DECLARE_DYNCREATE(CSbpro)
public:
   CLSID const& GetClsid()
   {
      static CLSID const clsid
         = { 0xad11711d, 0x847, 0x101a, { 0xa5, 0x80, 0x76, 0x74, 0x98, 0x9, 0x5f, 0xf0 } };
      return clsid;
   }
   virtual BOOL Create(LPCTSTR lpszClassName,
      LPCTSTR lpszWindowName, DWORD dwStyle,
      const RECT& rect,
      CWnd* pParentWnd, UINT nID,
      CCreateContext* pContext = NULL)
   { return CreateControl(GetClsid(), lpszWindowName, dwStyle, rect, pParentWnd, nID); }

    BOOL Create(LPCTSTR lpszWindowName, DWORD dwStyle,
      const RECT& rect, CWnd* pParentWnd, UINT nID,
      CFile* pPersist = NULL, BOOL bStorage = FALSE,
      BSTR bstrLicKey = NULL)
   { return CreateControl(GetClsid(), lpszWindowName, dwStyle, rect, pParentWnd, nID,
      pPersist, bStorage, bstrLicKey); }

// Attributes
public:
   CString GetAbout();
   void SetAbout(LPCTSTR);
   short GetActiveSheet();
   void SetActiveSheet(short);
   BOOL GetAlwaysSplit();
   void SetAlwaysSplit(BOOL);
   CString GetAttributeName();
   void SetAttributeName(LPCTSTR);
   CString GetBlockedKeywords();
   void SetBlockedKeywords(LPCTSTR);
   VARIANT GetBreakPoints();
   void SetBreakPoints(const VARIANT&);
   BOOL GetChanged();
   void SetChanged(BOOL);
   CString GetCode();
   void SetCode(LPCTSTR);
   short GetDebugHeight();
   void SetDebugHeight(short);
   CString GetDefaultDataType();
   void SetDefaultDataType(LPCTSTR);
   CString GetDefaultMacroName();
   void SetDefaultMacroName(LPCTSTR);
   CString GetDefaultObjectName();
   void SetDefaultObjectName(LPCTSTR);
   BOOL GetEditTools();
   void SetEditTools(BOOL);
   CString GetErrorFile();
   void SetErrorFile(LPCTSTR);
   long GetErrorHelpContext();
   void SetErrorHelpContext(long);
   CString GetErrorHelpFile();
   void SetErrorHelpFile(LPCTSTR);
   short GetErrorLine();
   void SetErrorLine(short);
   long GetErrorNumber();
   void SetErrorNumber(long);
   short GetErrorOffset();
   void SetErrorOffset(short);
   CString GetErrorSource();
   void SetErrorSource(LPCTSTR);
   CString GetErrorText();
   void SetErrorText(LPCTSTR);
   BOOL GetEventMode();
   void SetEventMode(BOOL);
   BOOL GetFileChangeDir();
   void SetFileChangeDir(BOOL);
   CString GetFileDesc();
   void SetFileDesc(LPCTSTR);
   CString GetFileExt();
   void SetFileExt(LPCTSTR);
   BOOL GetFileMenuVisible();
   void SetFileMenuVisible(BOOL);
   CString GetFileName();
   void SetFileName(LPCTSTR);
   BOOL GetFileTools();
   void SetFileTools(BOOL);
   BOOL GetFullPopupMenu();
   void SetFullPopupMenu(BOOL);
   short GetHeaderLineCount();
   void SetHeaderLineCount(short);
   BOOL GetHelpMenuVisible();
   void SetHelpMenuVisible(BOOL);
   CString GetHiddenCode();
   void SetHiddenCode(LPCTSTR);
   unsigned long GetHighlightBuiltin();
   void SetHighlightBuiltin(unsigned long);
   unsigned long GetHighlightComment();
   void SetHighlightComment(unsigned long);
   unsigned long GetHighlightError();
   void SetHighlightError(unsigned long);
   unsigned long GetHighlightExtension();
   void SetHighlightExtension(unsigned long);
   unsigned long GetHighlightReserved();
   void SetHighlightReserved(unsigned long);
   BOOL GetLocked();
   void SetLocked(BOOL);
   long GetModuleKind();
   void SetModuleKind(long);
   BOOL GetMultiSheet();
   void SetMultiSheet(BOOL);
   BOOL GetNegotiateMenus();
   void SetNegotiateMenus(BOOL);
   BOOL GetPause();
   void SetPause(BOOL);
   long GetProcDisplayMode();
   void SetProcDisplayMode(long);
   BOOL GetRun();
   void SetRun(BOOL);
   short GetSheetCount();
   void SetSheetCount(short);
   BOOL GetStatusVisible();
   void SetStatusVisible(BOOL);
   BOOL GetSyntaxCheck();
   void SetSyntaxCheck(BOOL);
   BOOL GetToolbarVisible();
   void SetToolbarVisible(BOOL);
   long GetSelLength();
   void SetSelLength(long);
   long GetSelStart();
   void SetSelStart(long);
   CString GetSelText();
   void SetSelText(LPCTSTR);
   unsigned long GetHighlightBreak();
   void SetHighlightBreak(unsigned long);
   unsigned long GetHighlightExec();
   void SetHighlightExec(unsigned long);
   BOOL GetTabAsSpaces();
   void SetTabAsSpaces(BOOL);
   short GetTabWidth();
   void SetTabWidth(short);
   CString GetText();
   void SetText(LPCTSTR);
   CString Get_Caption();
   void Set_Caption(LPCTSTR);
   short GetBorderStyle();
   void SetBorderStyle(short);
   CString GetCaption();
   void SetCaption(LPCTSTR);
   BOOL GetEnabled();
   void SetEnabled(BOOL);
   COleFont GetFont();
   void SetFont(LPDISPATCH);
   OLE_HANDLE GetHWnd();
   void SetHWnd(OLE_HANDLE);

// Operations
public:
   CString GetEvaluate(LPCTSTR Expression);
   void SetEvaluate(LPCTSTR Expression, LPCTSTR lpszNewValue);
   BOOL AddExtension(LPCTSTR Prefix, LPDISPATCH Ext);
   BOOL CloseSheet(short Index);
   LPDISPATCH CreateHandler(LPCTSTR Prototype);
   LPDISPATCH CreateHandlers(LPCTSTR Prefix, LPDISPATCH EventObj);
   BOOL Disconnect();
   BOOL ExecuteMenuCommand(short CmdId);
   void FindOrCreateProc(LPCTSTR ProcName);
   short FindSheet(LPCTSTR FileName);
   long IndexFromLine(short Line);
   BOOL IsMenuCommandChecked(short CmdId);
   BOOL IsMenuCommandEnabled(short CmdId);
   BOOL IsModuleLoaded(LPCTSTR FileName);
   short LineFromIndex(long Index);
   BOOL LoadModule(LPCTSTR FileName);
   CString MacroCaption(LPCTSTR FileName);
   LPDISPATCH ModuleInstance(LPCTSTR FileName, BOOL New);
   BOOL RemoveExtensions(LPCTSTR FileName);
   BOOL ReportError(LPDISPATCH Err);
   void RunFile(LPCTSTR FileName);
   void RunThis(LPCTSTR Code);
   BOOL SelectSheet(short Index);
   void SetDialogIdle(LPDISPATCH IdleObject);
   short Shutdown();
   void Trace(short Categories);
   BOOL UnloadModule(LPCTSTR FileName);
   void AboutBox();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SBPRO_H__1D6410E5_98B7_11D2_9866_004005408E44__INCLUDED_)
