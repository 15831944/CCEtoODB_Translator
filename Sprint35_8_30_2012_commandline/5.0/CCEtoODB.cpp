// $Header: /CAMCAD/5.0/CAMCAD.CPP 156   6/14/07 1:18p Kurt Van Ness $

/*****************************************************************************/
/*  Project CAMCAD
    Router Solutions Inc.
    Copyright � 1994-2001. All Rights Reserved.
*/           

#include "StdAfx.h"
#include "CCEtoODB.h"
#include "mainfrm.h"
#include "CCdoc.h"
#include "CCview.h"
#include "Splash.h"
#include "font.h"
#include "license.h"
#include "attrib.h"
#include <direct.h>
#include "SAX.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <io.h>
#include <fcntl.h>
#include "edit.h"
#include "wrldview.h"
#include "api_hide.h"
#include "api.h"
#include <ctype.h>
#include "childfrm.h"
#include "TreeListFrame.h"
#include "CAMCADNavigator.h"
#include "CCEtoODB.h"
#include "SoftwareExpiration.h"
#include "ViewSynchronizer.h"
#include <afxpriv.h>
#include "olhapi.h"
#include "ProfileLib.h"
#include "DcaLib.h"
#include "DcaFileType.h"
#include "DcaEnumIterator.h"
#include "ODBC_Lib.h"
#include "element.h"
#include "DFT.h"
#include "MergeFiles.h"
#include "Encryption/InfrasecUtility.h"
#include <iostream>

#include "RwUiLib.h" // for GetLogfilePath()
#include <string>



#ifdef _DEBUG
	//*rcf This no longer works, is unresolved reference: void AFXAPI _AfxTraceMsg(LPCTSTR lpszPrefix, const MSG* pMsg);
	//*rcf This is apparantly unused:  BOOL AFXAPI _AfxCheckDialogTemplate(LPCTSTR lpszResource,
	//                              	BOOL bInvisibleChild);
#endif

// Building Block includes
// needed for BuildingBlock software
extern "C"
{
#include "bbsdefs.h"
}

#include "staticlabel.h"
#include "afxwin.h"
#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif   

//*rcf BUG this no longer works, need replacement:
//*rcf AFX_STATIC BOOL AFXAPI _AfxSetRegKey(LPCTSTR lpszKey, LPCTSTR lpszValue, LPCTSTR lpszValueName = NULL);

//_____________________________________________________________________________
#define CcMruFileCount  10  // must match number of IDs defined in Resource.h
#define CadMruFileCount 10  // must match number of IDs defined in Resource.h

static BOOL CALLBACK CcFileRecognizerFunction(LPCTSTR lpszPathName)
{
   CFilePath filePath(lpszPathName);

   bool retval = (filePath.getExtension().CompareNoCase("cc" ) == 0 ||
                  filePath.getExtension().CompareNoCase("ccz") == 0    );

   return retval;
}

static BOOL CALLBACK CadFileRecognizerFunction(LPCTSTR lpszPathName)
{
   return !CcFileRecognizerFunction(lpszPathName);
}

//_____________________________________________________________________________
extern BOOL    LoadingDataFile; // CCDOC.CPP
extern BOOL    LoadingProjectFile; // from PORT.CPP
extern License *licenses;
//extern CString user, company, serialNum;  // from LICENSE.CPP
extern BOOL    SecurityKeyUser; // 
extern CView   *activeView; // from CCVIEW.CPP
extern CEditDialog *editDlg; // SELECT.CPP
extern WorldView *worldView; // from WRLDVIEW.CPP
extern HideCAMCADDlg *hideCAMCADDlg; // from API_HIDE.CPP
                    
CMultiDocTemplate*   pDocTemplate;
CCEtoODBView          *mouseView = NULL;
CString              CAMCAD_File; 
CString              NotePadProgram;
CString              LogReaderProgram;
SettingsStruct       GlSettings;
BOOL                 NoUI = -1;

char           realpartPathName[_MAX_PATH]; // this is the full path the the realpart library !!!

void init_Units();
void SetProductHelpMenuItem();
BOOL CheckWin32();

int camCadNewHandler(size_t numBytes)
{
   static bool inHandlerFlag = false;

   if (!inHandlerFlag)
   {
      inHandlerFlag = true;

      try
      {
         formatMessageBox("No memory available for request size of %I64d, fragmented or out of memory.",(__int64)numBytes);
      }
      catch (CMemoryException* memoryException)
      {
         CRuntimeClass* rtc = memoryException->GetRuntimeClass();
      }
   }

   inHandlerFlag = false;

   return 0;
}

/******************************************************************************
* MemErrorMessage
*/
void MemErrorMessage(const char* file, int line)
{
   CCEtoODBApp *pApp = (CCEtoODBApp*)AfxGetApp();
   CString buf;
   buf.Format("Not enough Memory\nFile:%s, Line:%d", file, line);

   if (pApp->getUseDialogsFlag())
      AfxMessageBox(buf, MB_OK | MB_ICONEXCLAMATION);

   exit(EXIT_FAILURE);
}

/******************************************************************************
* ErrorMessage
*/
int ErrorMessage(const char *text, const char *caption, UINT type)
{ 
   CCEtoODBApp *pApp = (CCEtoODBApp*)AfxGetApp();
   CString buf;
   buf.Format("%s\n%s", caption, text);
   
   int retval = IDOK;
   
   // If message type gets an actual choice from user, as opposed to informational
   // message that is just dismissed with OK, then we want to log the response.
   // Mainly this is for "silent running" mode, e.g. Read?Write aka ImportExport jobs,
   // such as used for vPlan DFT export. For other message types there OK is used
   // to just dismiss the popup, we don't need to log the response, it would look silly.
   bool includeResponseInLog = false;  // Assume we won't.

   // If dialogs are in use then issue message, otherwise determine default answer
   if (pApp->getUseDialogsFlag())
   {
      retval = AfxMessageBox(buf, type);
   }
   else
   {
      // If not using dialogs then auto-answer affirmative (OK or YES)
      if (type & MB_OK || type & MB_OKCANCEL)
         retval = IDOK;
      else if (type & MB_YESNOCANCEL || type & MB_YESNO)
         retval = IDYES;

      includeResponseInLog = (type & MB_OKCANCEL) || (type & MB_YESNOCANCEL) || (type & MB_YESNO); // But not MB_OK.
   }

   // Append text version of response to message.
   // The LogMessage func checks for itself if log is open, here just send the message.
   if (includeResponseInLog)
   {
      CString response;
      switch (retval) 
      {
      case IDOK:        response = "OK";     break;
      case IDCANCEL:    response = "CANCEL"; break;
      case IDABORT:     response = "ABORT";  break;
      case IDRETRY:     response = "RETRY";  break;
      case IDIGNORE:    response = "IGNORE"; break;
      case IDYES:       response = "YES";    break;
      case IDNO:        response = "NO";     break;
      }
      if (!response.IsEmpty())
         buf += "\n(Answered: " + response + ")";
   }

   // For log, add a final newline plus newline for white space before possible future message.
   buf += "\n\n";
   pApp->LogMessage(buf);

   return retval;
}

/*******************************************************************************
* Notepad()
*/
void Notepad(const char *file)
{
   CCEtoODBApp *pApp = (CCEtoODBApp*)AfxGetApp();

   if (NotePadProgram.IsEmpty() || !NotePadProgram.CompareNoCase("NONE") || !pApp->getUseDialogsFlag())
      return;

   CString commandLine = NotePadProgram;
   commandLine += " ";
   commandLine += "\"";
   commandLine += file; // make a quote for long filenames
   commandLine += "\"";

   int res = WinExec(commandLine, SW_SHOW);
   switch (res)
   {
   case 0:
      MessageBox(NULL, "The system is out of memory or resources!","LogReader", MB_OK | MB_ICONHAND);
      break;
   case ERROR_BAD_FORMAT:
      MessageBox(NULL, "The .EXE file is invalid (non-Win32 .EXE or error in .EXE image)!","Notepad", MB_OK | MB_ICONHAND);
      break;
   case ERROR_FILE_NOT_FOUND:
      {
         CString buf;
         buf.Format("The specified file [%s] was not found!", commandLine);
         MessageBox(NULL, buf, "Notepad", MB_OK | MB_ICONHAND);
      }
      break;
   case ERROR_PATH_NOT_FOUND:
      {
         CString buf;
         buf.Format("The specified path [%s] was not found!", commandLine);
         MessageBox(NULL, buf, "Notepad", MB_OK | MB_ICONHAND);
      }
      break;
   }
}

/*******************************************************************************
* Logreader()
*/
void Logreader(const char *file)
{
   CCEtoODBApp *pApp = (CCEtoODBApp*)AfxGetApp();
   if (LogReaderProgram.IsEmpty() || !LogReaderProgram.CompareNoCase("NONE") || !pApp->getUseDialogsFlag())
      return;

   int done = 0;
   CString commandLine = LogReaderProgram;
   commandLine += " ";
   commandLine += file;

   while (done < 3) // 0 try in general path, 1 = try in exe path, 2 = try in userpath
   {
      int res = WinExec(commandLine, SW_SHOW);

      switch (res)
      {
      case 0:
         MessageBox(NULL, "The system is out of memory or resources!","LogReader", MB_OK | MB_ICONHAND);
         return;

      case ERROR_BAD_FORMAT:
         MessageBox(NULL, "The .EXE file is invalid (non-Win32 .EXE or error in .EXE image)!","LogReader", MB_OK | MB_ICONHAND);
         return;

      case ERROR_FILE_NOT_FOUND:
         {
            // here if the command could not be found, see if the current CAMCAD EXE path contains it.
            CString  t;
            done++;

            if (done == 1)
            {
               t = pApp->getCamcadExeFolderPath();
               t += LogReaderProgram;
               t += " ";
               t += file;
               commandLine = t;
               break;
            }
            else if (done == 2)
            {
               t = pApp->getUserPath();
               t += LogReaderProgram;
               t += " ";
               t += file;
               commandLine = t;
               break;
            }
   
            t.Format("The specified file [%s] was not found!",commandLine);
            MessageBox(NULL, t, "LogReader", MB_OK | MB_ICONHAND);
            return;
         }
         break;

      case ERROR_PATH_NOT_FOUND:
         {  
            CString buf;
            buf.Format("The specified path [%s] was not found!", commandLine);
            MessageBox(NULL, buf, "LogReader", MB_OK | MB_ICONHAND);
            return;
         }

      default:
         return;
      }
   }
}

//_____________________________________________________________________________
CDocument* CMyMultiDocTemplate::OpenDocumentFile(LPCTSTR lpszPathName, BOOL bMakeVisible)
{
   CCEtoODBApp *pApp = (CCEtoODBApp*)AfxGetApp();

   if (pApp->getRunMode() == runModeDataProtectedModelessDialog)
   {
      return NULL;
   }

   if (!pApp->getSchematicLinkController().requestDeleteOfSchematicLink())
   {
      return NULL;
   }

   return CMultiDocTemplate::OpenDocumentFile(lpszPathName, bMakeVisible);
}

/////////////////////////////////////////////////////////////////////////////
// CCEtoODBApp

BEGIN_MESSAGE_MAP(CCEtoODBApp, CWinApp)
   /*ON_COMMAND(CG_IDS_TIPOFTHEDAY, ShowTipOfTheDay)
   //{{AFX_MSG_MAP(CCEtoODBApp)
   //ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
   ON_COMMAND(ID_SETTINGS_LOADGLOBALSETTINGS, OnSettingsLoadGlobal)
   ON_COMMAND(ID_LOAD_DATAFILE, OnLoadDataFile)
   ON_COMMAND(ID_LOADPROJECTFILE, OnLoadProjectFile)
   ON_COMMAND(ID_LOAD_FONT, OnLoadFont)
   ON_COMMAND(ID_HELP_INDEX, OnHelpIndex)
   ON_COMMAND(ID_MACROS, OnMacros)
   ON_COMMAND(ID_RUN_MACRO, OnRunMacro)
   ON_COMMAND(ID_SET_PROJECT_PATH, OnSetProjectPath)
   ON_COMMAND(ID_SETATTRIBUTE_EDIT,OnEditAttrTemplate)
   ON_COMMAND(ID_SETTEMPLATEDIRECTORY,OnSetTemplateDirectory)
   ON_COMMAND(ID_Menu33879,OnEditCustomTemplate)
   ON_COMMAND(ID_CAMCAD_COM, OnCamcadCom)
   ON_COMMAND(ID_RSI_COM, OnRsiCom)
   //}}AFX_MSG_MAP
   // Standard file based document commands
   ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
   ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
   // Standard print setup command
   ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
   ON_COMMAND(ID_ONLINE_HELP, OnOnlineHelp)
   ON_COMMAND(ID_OPEN_CAD_FILE, OnOpenCADFile)
   //ON_COMMAND(ID_SCHEMATICLINK_SCHEMATICLINK, OnToolsSchematiclink)
   //ON_COMMAND(ID_LINK_NETS, OnLinkNets)
   //ON_COMMAND(ID_LINK_COMPS, OnLinkComps)
   //ON_COMMAND(ID_SCHEMATIC_SETTINGS, OnSchematicSettings)
   //ON_UPDATE_COMMAND_UI(ID_LINK_NETS, OnUpdateLinkNets)
   //ON_UPDATE_COMMAND_UI(ID_LINK_COMPS, OnUpdateLinkComps)
   //ON_UPDATE_COMMAND_UI(ID_SCHEMATIC_SETTINGS, OnUpdateSchematicSettings)
   ON_COMMAND(ID_DFM_RUNNER, OnDfmRunner)
   ON_COMMAND(ID_DFM_SCRIPTER, OnDfmScripter)
   //ON_COMMAND(ID_TOOLS_RSI_EXCHANGE, OnToolsRsiExchange)
   //ON_COMMAND(ID_TOOLS_BOM_EXP, OnToolsBomExplorer)
   //ON_COMMAND(ID_TOOLS_REALPART, OnToolsRealpart)
   ON_COMMAND(ID_HELP_INFOHUB, OnHelpInfoHub)*/
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCEtoODBApp construction
void InitTrace();
void FreeTrace();

CCEtoODBApp::CCEtoODBApp()
: m_mruFileManager(this)
, m_docTemplate(NULL)
, m_messageFilterTypeMessage(messageFilterTypeMessage)
, m_messageFilterTypeFormat(messageFilterTypeFormat)
, m_viewSynchronizer(NULL)
, m_logFp(NULL)
, m_cceDecryptionAllowed(false)
, m_exitCode(ExitCodeSuccess)
{
   m_hwndDialog = NULL; // TOOLTIPS
   m_gpToolTip = NULL;  // TOOLTIPS
   schLinkState = FALSE;   // Schematic linking
   MostlyHideCamcad = FALSE;
   SilentRunning = FALSE;
   CheckoutLics = TRUE;
   LicenseTimerEnabled = FALSE;
   UsingAutomation = FALSE;
   OnlyRegister = FALSE;
	CopyToTestFixLocation = false;

   setUseDialogsFlag(true);

	// ViewMode will only be set throught command line in the function CMyCommandLineInfo::ParseParam().
	// Therefore initilize to -1 instead of SW_SHOWMAXIMIZED so later on the function
	// CMainFrame::restoreWindowState() will be able to tell that ViewMode is set by commandline if 
	// it is greater than -1
   ViewMode = -1;	

   m_runMode = runModeNormal;
}

CCEtoODBApp::~CCEtoODBApp()
{
   CloseCamcadLogFile();

   delete m_viewSynchronizer;
}

CViewSynchronizer& CCEtoODBApp::getViewSynchronizer()
{
   if (m_viewSynchronizer == NULL)
   {
      m_viewSynchronizer = new CViewSynchronizer();
   }

   return *m_viewSynchronizer;
}

void CCEtoODBApp::releaseViewSynchronizer()
{
   delete m_viewSynchronizer;
   m_viewSynchronizer = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CCEtoODBApp object

CCEtoODBApp theApp;

CCEtoODBApp& getApp()
{
   return theApp;
}

CWnd* getMainWnd()
{
   return getApp().m_pMainWnd;
}

CMainFrame* getMainFrame()
{
   return ((CMainFrame*)getMainWnd());
}

static bool s_showHiddenAttributes = true;

bool getShowHiddenAttributes()
{
   return s_showHiddenAttributes;
}

bool setShowHiddenAttributes(bool flag)
{
   bool retval = s_showHiddenAttributes;

   s_showHiddenAttributes = flag;

   //getApp().WriteProfileInt("Settings","ShowHiddenAttributes",s_showHiddenAttributes);

   return retval;
}

// This identifier was generated to be statistically unique for your app.
// You may change it if you prefer to choose a specific identifier.

// {5BECA79B-EF3B-11D1-BA40-0080ADB36DBB}
static const CLSID clsid =
{ 0x5beca79b, 0xef3b, 0x11d1, { 0xba, 0x40, 0x0, 0x80, 0xad, 0xb3, 0x6d, 0xbb } };


/////////////////////////////////////////////////////////////////////////////
// CCEtoODBApp initialization
LRESULT CALLBACK LocWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void RegisterWindow(CWinApp *pApp)
{
   WNDCLASS   wndClass;
   ATOM wndAtom;
   CString clsName;
   clsName.LoadString(IDR_MAINFRAME);

   wndClass.lpfnWndProc   = (WNDPROC)LocWndProc;
   wndClass.style         = 0;   //CS_HREDRAW | CS_VREDRAW;
   wndClass.cbClsExtra    = 0;
   wndClass.cbWndExtra    = 0;
   wndClass.hInstance     = pApp->m_hInstance;
   wndClass.hIcon         = NULL;   //pApp->LoadIcon( "IDR_MAINFRAME" );
   wndClass.hCursor       = NULL;   //LoadCursor( NULL, IDC_ARROW);
   wndClass.hbrBackground = NULL;
   wndClass.lpszMenuName  = (LPSTR)NULL;
   wndClass.lpszClassName = clsName;         /* Class Name is App Name*/

   /* register the window class */
   wndAtom = RegisterClass( &wndClass );
   if (!wndAtom)
   {
      CString msg;
      LPVOID lpMsgBuf;
      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, GetLastError(), 0, (LPTSTR) &lpMsgBuf, 0, NULL);
      msg.Format("Error registering application window.\n\n%s", (LPCTSTR)lpMsgBuf);
      LocalFree(lpMsgBuf);

      AfxMessageBox(msg);
   }
}

/******************************************************************************
* AlreadyRunning
*/
BOOL AlreadyRunning()
{
   // this function only works on NT and 2000, it does not work on 95, 98, ME !!!
   if (Platform != WINNT)
      return FALSE;

   HWND wnd;
   CString clsName;

   clsName.LoadString(IDR_MAINFRAME);
   if (!(wnd = FindWindow(clsName, NULL)))
   {
      DWORD errCode = GetLastError();
      return !errCode;
   }
   return TRUE;
}

//_____________________________________________________________________________
RunModeTag CCEtoODBApp::getRunMode() const
{
   return m_runMode;
}

void CCEtoODBApp::setRunMode(RunModeTag runMode)
{
   m_runMode = runMode;
}

bool CCEtoODBApp::getUseDialogsFlag() const
{
   return m_useDialogsFlag;
}

void CCEtoODBApp::setUseDialogsFlag(bool flag)
{
   m_useDialogsFlag = flag;

   CMessageFilter::setUseDialogsFlag(m_useDialogsFlag);
}

BOOL CCEtoODBApp::ProcessMessageFilter(int code, LPMSG lpMsg)   // TOOLTIPS
{
   if (m_hwndDialog != NULL)
      if (lpMsg->hwnd == m_hwndDialog || ::IsChild(m_hwndDialog, lpMsg->hwnd))
         if (NULL != m_gpToolTip)
            m_gpToolTip->RelayEvent(lpMsg);

   return CWinApp::ProcessMessageFilter(code, lpMsg);
}

/******************************************************************************
* GetCAMCADTitle
*/
CString CCEtoODBApp::getCamCadSubtitle() const
{
   // The one and only original app name, now the default app name.
   CString title = "CAMCAD ";

   // For vPlan integration we're wanting the final title bar to be
   // "vPlan Test and Inspection Engineering".
   // We might shorten that, e.g. 
   // "Test and Inspection Session" was suggested. It is a little shorter.
   // The goofy type casting is because this is a const function. Maybe should get rid of that.
   if (((CTXPJob&)(this->m_txpJob)).IsActivated())
   {
      title = "vPlan Test and Inspection Engineering "; // Don't forget space after name.
   }
   else
   {
      // Normal CAMCAD, get specific CAMCAD product.

#ifdef SHAREWARE // Shareware or Professional in Frame Title
      title += "Shareware ";
#else
      switch (Product)
      {
      case PRODUCT_PROFESSIONAL:
         title += "Professional ";
         break;
      case PRODUCT_PCB_TRANSLATOR:
         title += "PCB Translator ";
         break;
      case PRODUCT_VISION:
         title += "Vision ";  // no CAMCAD in title
         break;
      default:
         title += "Graphic ";
         break;
      }
#endif
   }

	CString curVersion = getVersionString().Mid(0, getApp().getVersionString().ReverseFind('.'));
   title += curVersion;
   
   return title;
}

CString CCEtoODBApp::getCamCadTitle() const
{
   CString title = getCamCadSubtitle();

   for (int index = 0;index < m_titleSuffixes.GetCount();index++)
   {
      title += m_titleSuffixes.GetAt(index);
   }

   return title;
}

void CCEtoODBApp::setTitleSuffix(const CString& suffix)
{
   m_titleSuffixes.RemoveAll();
   pushTitleSuffix(suffix);
}

void CCEtoODBApp::pushTitleSuffix(const CString& suffix)
{
   m_titleSuffixes.Add(suffix);
}

CString CCEtoODBApp::popTitleSuffix()
{
   CString suffix;
   int count = m_titleSuffixes.GetCount();

   if (count > 0)
   {
      suffix = m_titleSuffixes.GetAt(count - 1);
      m_titleSuffixes.RemoveAt(count - 1);
   } 

   return suffix;
}

CString CCEtoODBApp::getUserPath() const
{
   return m_userPath;
}

CString CCEtoODBApp::getCamcadExeFolderPath() const
{
   return m_camcadExeFolderPath;
}

CString CCEtoODBApp::getCmdLineImportSettingsFilePath() const
{
   return m_cmdlineImportSettingsFilePath;
}

CString CCEtoODBApp::getCmdLineExportSettingsFilePath() const
{
   return m_cmdlineExportSettingsFilePath;
}

void CCEtoODBApp::setCmdLineImportSettingsFilePath(CString filepath)
{
   m_cmdlineImportSettingsFilePath = filepath;
}

void CCEtoODBApp::setCmdLineExportSettingsFilePath(CString filepath)
{
   m_cmdlineExportSettingsFilePath = filepath;
}

CString CCEtoODBApp::getImportSettingsFilePath(CString defaultFilename)
{
   // Convenience function that performs the standard pattern of using command line settings file
   // setting if present, otherwise default to camcad install path plus default file name.

	// First try app setting, was optionally set using command line param.
   CString settingsFile = getApp().getCmdLineImportSettingsFilePath();

   // If blank then do standard system file folder lookup
   if (settingsFile.IsEmpty())
      settingsFile = getApp().getSystemSettingsFilePath(defaultFilename);

   return settingsFile;
}
// Keep
CString CCEtoODBApp::getExportSettingsFilePath(CString defaultFilename)
{
   // Convenience function that performs the standard pattern of using command line settings file
   // setting if present, otherwise default to camcad install path plus default file name.

	// First try app setting, was optionally set using command line param.
   CString settingsFile = getApp().getCmdLineExportSettingsFilePath();

   // If blank then do standard system file folder lookup
   if (settingsFile.IsEmpty())
      settingsFile = getApp().getSystemSettingsFilePath(defaultFilename);

   return settingsFile;
}


// Keep
CString CCEtoODBApp::getSystemSettingsFilePath(CString settingsFilename)
{
   // Favor userpath, user might have set something up.
   // If desired file is not there then set to camcad path.

   // This is the form most places in CAMCAD used before this function
   // existed. Use it as the first try.
   CString settingsFile = getUserPath() + settingsFilename;

   if (!fileExists(settingsFile))
   {
      // Build a path that goes to camcad exe location.
      // Return it without bothering to check, this is the last ditch, exists or not.
      settingsFile = getCamcadExeFolderPath() + settingsFilename;
   }

   return settingsFile;
}

/******************************************************************************
* CCEtoODBApp::CopyCAMCADToLocation
*/
void CCEtoODBApp::CopyCAMCADToLocation()
{
#if defined NDEBUG
   CString releaseDirectory;
   releaseDirectory.Format("\\\\Cob\\dfs\\sms-build\\Developers\\CamCad\\%d.%d",CamCadMajorVersion,CamCadMinorVersion);

   CopyCAMCADToLocation(releaseDirectory);

   //if (!CopyCAMCADToLocation("\\\\Debug\\c$\\Development\\CAMCADTestFix"))
   //{
   //   //CopyCAMCADToLocation("\\\\Rsi001\\M\\CAMCADTestFix");
   //}
#endif
}

DWORD CALLBACK camCadCopyProgressRoutine(
  LARGE_INTEGER TotalFileSize,
  LARGE_INTEGER TotalBytesTransferred,
  LARGE_INTEGER StreamSize,
  LARGE_INTEGER StreamBytesTransferred,
  DWORD dwStreamNumber,
  DWORD dwCallbackReason,
  HANDLE hSourceFile,
  HANDLE hDestinationFile,
  LPVOID lpData
)
{
   if (lpData != NULL)
   {
      COperationProgress* progress = (COperationProgress*)lpData;

      double position = ((double)TotalBytesTransferred.QuadPart)/TotalFileSize.QuadPart;
      progress->updateProgress(position);
   }

   return PROGRESS_CONTINUE;
}


bool CCEtoODBApp::CopyCAMCADToLocation(const char* destinationDirectory)
{
   bool retval = false;

#if defined NDEBUG
   //formatMessageBox("Copying CamCad to '%s'",destinationDirectory);

   CFileStatus fileStatus;
   retval = (CFile::GetStatus(destinationDirectory,fileStatus) != 0);

   if (retval)
   {
	   CString toPath, fromPath;

      // find executable's path
	   char fullPath[_MAX_PATH], drive[_MAX_DRIVE], path[_MAX_DIR], filename[_MAX_FNAME], ext[_MAX_EXT], tempPath[_MAX_PATH];
	   strcpy(fullPath, m_pszHelpFilePath);
	   _splitpath(fullPath, drive, path, filename, ext);
	   _makepath(tempPath, drive, path, NULL, NULL);
   	
	   fromPath = tempPath;
	   toPath.Format("%s\\%s",destinationDirectory,getApp().getVersionString());

	   CFileFind findFile;
	   CString cmd;

	   // make sure the path exists
	   if (!findFile.FindFile(toPath))
	   {
		   SECURITY_ATTRIBUTES sAtt;
		   sAtt.nLength = sizeof(SECURITY_ATTRIBUTES);
		   sAtt.lpSecurityDescriptor = NULL;
		   sAtt.bInheritHandle = FALSE;

		   // create the directory on test fix location per version
		   if (!CreateDirectory(toPath, &sAtt))
			   return false;
	   }
	   toPath += "\\";

	   CString fromFile, toFile;
      COperationProgress progress;
      CString progressMessage;
      BOOL copyCancel=0;
      DWORD copyFlags=0;

	   // Copy camcad exe to destination
	   fromFile.Format("%s%s.%s", fromPath, m_pszExeName, "EXE");
	   toFile.Format("%s%s.%s", toPath, m_pszExeName, "EXE");

      progressMessage.Format("Copying to '%s'",toFile);
      progress.updateStatus(progressMessage,1.);

	   CopyFileEx(fromFile, toFile,camCadCopyProgressRoutine,&progress,&copyCancel,copyFlags);

	   // Copy the tlb to destination
	   fromFile.Format("%s%s.%s", fromPath, m_pszExeName, "TLB");
	   toFile.Format("%s%s.%s", toPath, m_pszExeName, "TLB");

      progressMessage.Format("Copying to '%s'",toFile);
      progress.updateStatus(progressMessage,1.);

	   CopyFileEx(fromFile, toFile,camCadCopyProgressRoutine,&progress,&copyCancel,copyFlags);

	   // Copy the version file to destination
	   CString curVersion, sourcePath;
#ifdef OLD_STYLE__BASED_ON_VERSION_IN_BUILD_NAME
      // Presumptuous, does not work if source/build folder is not named after build version.
		curVersion = getApp().getVersionString().Mid(0, getApp().getVersionString().ReverseFind('.'));
		curVersion = curVersion.Mid(0, curVersion.ReverseFind('.'));
	   sourcePath.Format("%s%s\\", fromPath, curVersion);
      fromFile.Format("%s%s.%s", sourcePath, "Version", "CPP");
#else
      // Look for it, also presumptuous, based on standard camcad source/build folder hierarchy.
      fromFile = RecursiveFindFilePath(fromPath, "Version.cpp");
#endif

      if (!fromFile.IsEmpty())
      {
         toFile.Format("%s%s.%s", toPath, "Version", "CPP");

         progressMessage.Format("Copying to '%s'",toFile);
         progress.updateStatus(progressMessage,1.);

         CopyFileEx(fromFile, toFile,camCadCopyProgressRoutine,&progress,&copyCancel,copyFlags);
      }
   }
#endif

   return retval;
}

/******************************************************************************
* CCEtoODBApp::SetCAMCADTitle
*/
void CCEtoODBApp::SetCAMCADTitle()
{
   CString title = getCamCadSubtitle();

   // Reset the title suffix
   getApp().setTitleSuffix("");

#if defined _RELTEST
   getApp().pushTitleSuffix(" (Release Test)");
#endif

   if (getApp().getCamcadLicense().hasBaseLicense())
   {
      getApp().pushTitleSuffix(" - Licensed");      
   }
   else
   {
      getApp().pushTitleSuffix(" - Unlicensed"); 
   }


	CString projectPath;
	CCEtoODBView* view = getActiveView();
	if (view != NULL)
		projectPath =  (view->GetDocument()!=NULL)?view->GetDocument()->GetProjectPath():"";

   if (projectPath.GetLength())
   {
      getApp().pushTitleSuffix(" - {" + projectPath + "}");
   }

   getMainFrame()->SetFrameTitle(getApp().getCamCadTitle());
   getMainWnd()->SetWindowText(getApp().getCamCadTitle());
}
 
/******************************************************************************
* CCEtoODBApp::GetProductKey
*/
CString CCEtoODBApp::GetProductKey()
{
   CString keyname = "Software\\" + (CString)REGISTRY_COMPANY + "\\CAMCAD\\Settings";

   HRESULT res;
   HKEY settingsKey;
   res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyname, 0, KEY_READ, &settingsKey);
   if (res != ERROR_SUCCESS)
      return "";  // no settings key in the local machine

   char product[15];

   DWORD bufLen = 15, lType;
   res = RegQueryValueEx(settingsKey, "Product", NULL, &lType, (LPBYTE)product, &bufLen);
   if (res != ERROR_SUCCESS)
   {
      RegCloseKey(settingsKey);
      return "";  // no product setting in the local machine
   }
   RegCloseKey(settingsKey);

	// push the value to current user
	res = RegCreateKeyEx(HKEY_CURRENT_USER, keyname, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &settingsKey, NULL);
	if (res == ERROR_SUCCESS)	// TypeLib found
	{
		res = RegSetValueEx(settingsKey, "Product", NULL, REG_SZ, (LPBYTE)product, bufLen);
	
		RegCloseKey(settingsKey);
	}

   return product;
}

/******************************************************************************
* CCEtoODBApp::GetCWDKey
* 
* CAMCAD reads directory setting from Local Machine and writes to Current User
* CAMCAD reads directory setting from Local Machine Product and write to Current User Product
* If CAMCAD cannot find Local Machine Product, then reads from Current User Product.
*/ //Keep
CString CCEtoODBApp::GetCWDKey()
{
   CString settingsLocation;
	HRESULT res;
   HKEY settingsKey;
   char dir[_MAX_PATH];
   DWORD bufLen = _MAX_PATH, lType;
	CString ccDir, ccProductDir;

	// CAMCAD reads directory setting from Local Machine and writes to Current User
	settingsLocation = "Software\\" + (CString)REGISTRY_COMPANY + "\\CAMCAD\\Settings";
   res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, settingsLocation, 0, KEY_READ, &settingsKey);
	if (res == S_OK)
	{
		// Read CAMCAD directory setting from Local Machine
		res = RegQueryValueEx(settingsKey, "Directory", NULL, &lType, (LPBYTE)dir, &bufLen);
		if (res != S_OK)
			dir[0] = '\0';
		dir[bufLen] = '\0';
		RegCloseKey(settingsKey);

		if (strlen(dir) > 0)
		{
			// push the value to current user under CAMCAD
			res = RegCreateKeyEx(HKEY_CURRENT_USER, settingsLocation, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &settingsKey, NULL);
			if (res == S_OK)
			{
				res = RegSetValueEx(settingsKey, "Directory", NULL, REG_SZ, (LPBYTE)dir, bufLen);
			
				RegCloseKey(settingsKey);
			}
		}
	}
	ccDir = dir;

	// CAMCAD reads directory setting from Local Machine Product and write to Current User Product
	settingsLocation = "Software\\" + (CString)REGISTRY_COMPANY + "\\" + m_pszProfileName + "\\Settings";
   res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, settingsLocation, 0, KEY_READ, &settingsKey);
	if (res == S_OK)
	{
		// Read CAMCAD product directory setting from Local Machine
		res = RegQueryValueEx(settingsKey, "Directory", NULL, &lType, (LPBYTE)dir, &bufLen);
		if (res != S_OK)
			dir[0] = '\0';
		dir[bufLen] = '\0';
		RegCloseKey(settingsKey);

		if (strlen(dir) > 0)
		{
			// push the value to current user under the specific CAMCAD product
			res = RegCreateKeyEx(HKEY_CURRENT_USER, settingsLocation, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &settingsKey, NULL);
			if (res == S_OK)
			{
				res = RegSetValueEx(settingsKey, "Directory", NULL, REG_SZ, (LPBYTE)dir, bufLen);
			
				RegCloseKey(settingsKey);
			}
		}
	}
	ccProductDir = dir;

	if (ccProductDir.IsEmpty())
		return ccDir;
	else
		return ccProductDir;
}

/******************************************************************************
* CCEtoODBApp::updateProductRegistry
*/
void CCEtoODBApp::updateProductRegistry()
{
   CString localMachineProduct;
   CString keyname = "Software\\" + (CString)REGISTRY_COMPANY + "\\CAMCAD\\Settings";

   HRESULT res;
   HKEY settingsKey;
   res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyname, 0, KEY_READ, &settingsKey);
   if (res != ERROR_SUCCESS)
      return;  // no settings key in the local machine

   char product[15];
   DWORD bufLen = 15;
   res = RegQueryValueEx(settingsKey, "Product", NULL, NULL, (LPBYTE)product, &bufLen);
   if (res != ERROR_SUCCESS)
   {
      RegCloseKey(settingsKey);
      return;  // no product setting in the local machine
   }

   localMachineProduct = product;
   WriteProfileString("Settings", "Product", localMachineProduct);

   RegCloseKey(settingsKey);
}

/******************************************************************************
* CCEtoODBApp::updateInterfaceRegistry
*/
void CCEtoODBApp::updateInterfaceRegistry(CString clsID, CString interfaceID)
{
   CString cwd, interfaceLoc;

   HRESULT res;
   HKEY interfaceKey;
   interfaceLoc.Format("Interface", m_pszProfileName);
   res = RegOpenKeyEx(HKEY_CLASSES_ROOT, interfaceLoc, 0, KEY_WRITE, &interfaceKey);
   if (res != ERROR_SUCCESS)
      return;  // no interface key in the classes root

   HKEY clsIDKey;
   res = RegCreateKeyEx(interfaceKey, clsID, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &clsIDKey, NULL);
   if (res == ERROR_SUCCESS)	// clsid found
   {
		CString value = interfaceID;
		DWORD bufLen = value.GetLength();
		res = RegSetValueEx(clsIDKey, "", NULL, REG_SZ, (LPBYTE)value.GetBuffer(0), bufLen);

		HKEY subKey;
		res = RegCreateKeyEx(clsIDKey, "TypeLib", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &subKey, NULL);
		if (res == ERROR_SUCCESS)	// TypeLib found
		{
			value = clsID;
			bufLen = value.GetLength();
			res = RegSetValueEx(subKey, "", NULL, REG_SZ, (LPBYTE)value.GetBuffer(0), bufLen);

			value = "1.0";
			bufLen = value.GetLength();
			res = RegSetValueEx(subKey, "Version", NULL, REG_SZ, (LPBYTE)value.GetBuffer(0), bufLen);

			RegCloseKey(subKey);
		}

		res = RegCreateKeyEx(clsIDKey, "ProxyStubClsid32", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &subKey, NULL);
		if (res == ERROR_SUCCESS)	// ProxyStubClsid32 found
		{
			value = "{00020420-0000-0000-C000-000000000046}";
			bufLen = value.GetLength();
			res = RegSetValueEx(subKey, "", NULL, REG_SZ, (LPBYTE)value.GetBuffer(0), bufLen);

			RegCloseKey(subKey);
		}

		res = RegCreateKeyEx(clsIDKey, "ProxyStubClsid", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &subKey, NULL);
		if (res == ERROR_SUCCESS)	// ProxyStubClsid found
		{
			value = "{00020420-0000-0000-C000-000000000046}";
			bufLen = value.GetLength();
			res = RegSetValueEx(subKey, "", NULL, REG_SZ, (LPBYTE)value.GetBuffer(0), bufLen);

			RegCloseKey(subKey);
		}

		RegCloseKey(clsIDKey);
   }
   
   RegCloseKey(interfaceKey);
}

/******************************************************************************
* CCEtoODBApp::showMainWindow
*/
void CCEtoODBApp::showMainWindow()
{
   if (!MostlyHideCamcad && !SilentRunning)
   {
		((CMainFrame*)m_pMainWnd)->restoreWindowState();
      m_pMainWnd->UpdateWindow();
   }
   else
   {
      if (hideCAMCADDlg)
         return;

      hideCAMCADDlg = new HideCAMCADDlg;

      m_pMainWnd->ShowWindow(SW_MINIMIZE);

      hideCAMCADDlg->Create(IDD_HIDE_CAMCAD);
      hideCAMCADDlg->ShowWindow( SilentRunning ? SW_HIDE : SW_SHOW);
      hideCAMCADDlg->SetWindowPos(&hideCAMCADDlg->wndTopMost, 0, 0, 1, 1, SWP_NOMOVE | SWP_NOSIZE);

      FlushQueue();
   }
}

BOOL CCEtoODBApp::OnCmdMsg(UINT nID,int nCode,void* pExtra,AFX_CMDHANDLERINFO* pHandlerInfo)
{
   BOOL retval;

   if (m_mruFileManager.OnCmdMsg(nID,nCode,pExtra,pHandlerInfo))
   {
      retval = TRUE;
   }
   else
   {
      retval = CWinApp::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
   }

   return retval;
}

void CCEtoODBApp::AddToRecentFileList(LPCTSTR lpszPathName)
{
   if (!m_mruFileManager.AddToRecentFileList(lpszPathName))
   {
      CWinApp::AddToRecentFileList(lpszPathName);
   }
}

void CCEtoODBApp::CloseCamcadLogFile()
{
   // m_logFp might be to stdout, but that is okay. One can't actually close stdout
   // but also it does no harm to try it (right?) so we don't need to check.

   if (m_logFp != NULL)
   {
      fclose(m_logFp);
      m_logFp = NULL;  // So destructor does not try to do it again.
   }

}

//Keep
void CCEtoODBApp::OpenCamcadLogFile()
{
   if (m_logFp == NULL)
   {
      // If log file name is set (by command line) then if it specifies a full path use it as-is.
      // UNLESS it is the special name "stdout", in which case we just use stdout as the log file fp.
      // Otherwise apply the standard log file path location using name for leaf.
      // If not set then default to "camcad.log".
      // If user log file name is set but fails to open, try default log file.

      CString defaultLogFilePath( GetLogfilePath("cce2odbb.log") );
      CString userLogFilePath;

      // Get the fully resolved log file path unless the name is "stdout".
      if (!this->m_logfileName.IsEmpty() && this->m_logfileName.CompareNoCase("stdout") != 0)
      {
         CFilePath filepath(this->m_logfileName);
         if (filepath.isAbsolutePath())
            userLogFilePath = this->m_logfileName; // is absolute path, use as-is
         else
            userLogFilePath = GetLogfilePath(filepath.getFileName()); // use file name leaf and default log location

      }
      
      FILE *userLogFp = NULL; // used to track success of user log file open

      // Special case of stdout
      if (this->m_logfileName.CompareNoCase("stdout") == 0)
         m_logFp = userLogFp = stdout;

      // If not stdout then try user log file
      if (m_logFp == NULL && !userLogFilePath.IsEmpty())
      {
         // Ensure directory path to location of log exists.
         CString logDirPath = dirname(userLogFilePath);
         if (!fileExists(logDirPath))
            mkdirtree(logDirPath);
         // Open it
         m_logFp = userLogFp = fopen(userLogFilePath, "wt");
      }

      // If log fp is still NULL then user path is blank, not stdout, or failed, try default.
      if (m_logFp == NULL)
      {
         // Ensure directory path to location of log exists.
         CString logDirPath = dirname(defaultLogFilePath);
         if (!fileExists(logDirPath))
            mkdirtree(logDirPath);
         // Open it
         m_logFp = m_logFp = fopen(defaultLogFilePath, "wt");
      }

      // If log managed to open then write header
      if (m_logFp != NULL)
      {
         CTime time = CTime::GetCurrentTime();
         CString createdBy;
         createdBy.Format("Created by %s v.%s", getApp().m_pszProfileName, getApp().getVersionString());

         fprintf(m_logFp, "\n"); // Initial blank line to make it look better if sent to vPlan system log
         fprintf(m_logFp, "*--------------------------------------------------------*\n");
         fprintf(m_logFp, "*  %-53s *\n", "CCE to ODB++ Log File");
         //fprintf(m_logFp, "*  %-53s *\n", createdBy);
         fprintf(m_logFp, "*  %-53s *\n", time.Format("%A, %B %d, %Y"));
         fprintf(m_logFp, "*--------------------------------------------------------*\n\n");

         // If command line is not blank then write it to the log file
         CString cmdLine( GetCommandLine() );
         if (!cmdLine.IsEmpty())
         {
            fprintf(m_logFp, "Command Line:\n");
            fprintf(m_logFp, "%s\n\n", cmdLine);
         }

         // If userLogFilePath is not empty and userLogFp is NULL, then user specified
         // log file failed to open and we are writing in default log file.
         // Make note of this failure.
         if (!userLogFilePath.IsEmpty() && userLogFp == NULL)
         {
            fprintf(m_logFp, "Could not open user log file for writing.\n[%s]\n\n", userLogFilePath);
         }
      }
   }
}

// Keep
FILE * CCEtoODBApp::OpenOperationLogFile(CString filename, CString& filepath)
{
   // Open log file for operations like CAD Import, CAD Export, or anything else
   // that opens a log file, with the exception of the overall system log file, which
   // is opened above in OpenCamcadLogFile().

   // If OpenCamcadLogFile has previously been called, then this function returns
   // a ptr to that same FP, so all goes into the same log file. If it has not, then
   // the "traditional" log file is opened.
   // The traditional log file name is passed in here in filename.
   // If it turns out that gets used, then the resulting complete file path is
   // returned in filepath. If filename doesn't get used then filepath is returned empty.

   // Clear the result filepath.
   filepath.Empty();

   // Try system logfile fp. If not NULL then system log file is in play already, just use it.
   FILE *fp = getApp().GetSystemLogFp();

   // If fp still NULL then use older classic traditional (etc) style log.
   if (fp == NULL && !filename.IsEmpty())
   {
      filepath = GetLogfilePath(filename);

      if ((fp = fopen(filepath,"wt")) == NULL)
      {
         CString t;
         t.Format("Error opening log file [%s].", filepath);
         ErrorMessage(t, "Log File Error");
      }
   }

   return fp;
}

/******************************************************************************
* CCEtoODBApp::InitInstance
*/ // Keep
BOOL CCEtoODBApp::InitInstance()
{
   _set_new_handler(camCadNewHandler);
   _set_new_mode(1);

   CMyCommandLineInfo cmdInfo;
   ParseCommandLine(cmdInfo);

   this->OpenCamcadLogFile();

   if ( !cmdInfo.validParameters())
   {
       this->CloseCamcadLogFile();
	   return false;
   }

   m_camcadLicense.initializeLicenses();
   if (m_camcadLicense.checkOutLicenses() == false )
   {
	   getApp().LogMessage("\nLicense to translate from CCE to ODB++ is missing.\n\n");
       this->CloseCamcadLogFile();
	   return false;
   }

   this->SilentRunning = TRUE;

   // Working Directory from Registry
   CString cwd = GetCWDKey();	//GetProfileString("Settings", "Directory");
   if (!cwd.IsEmpty())
      _chdir(cwd);

   // find executable's path
   char fullPath[_MAX_PATH], drive[_MAX_DRIVE], path[_MAX_DIR], exePathName[_MAX_PATH]; 
   strcpy(fullPath, m_pszHelpFilePath);
   _splitpath(fullPath, drive, path, NULL, NULL);
   _makepath(exePathName, drive, path, NULL, NULL);
   m_camcadExeFolderPath = exePathName;

	char userPathName[_MAX_PATH];
   // Set userPathName to cwd if not in TXP mode. Note that if regisstry Directory was set
   // then cwd is already set to that registry setting.
   // If in TXP mode then set userPathName to the CAMCAD exe location.
   // The main use of this setting is to find auxiliary CAMCAD files, like default.fnt, as
   // well as things like import/export settings files.
   if (!this->m_txpJob.IsActivated())
   {
      _getcwd(userPathName, _MAX_PATH);
   }
   else
   {
      strncpy(userPathName, m_camcadExeFolderPath, _MAX_PATH);
   }

	if (userPathName[strlen(userPathName)-1] != '\\')
		strcat(userPathName, "\\");	
	m_userPath = userPathName;

   realpartPathName[0] = 0;   // 

   // Miten -- Check if we need this
   m_docTemplate = new CMyMultiDocTemplate(
      IDR_CAMCADTYPE,
      RUNTIME_CLASS(CCEtoODBDoc),
      RUNTIME_CLASS(CChildFrame),          // standard MDI child frame
      RUNTIME_CLASS(CCEtoODBView));
   AddDocTemplate(m_docTemplate);

   pDocTemplate = m_docTemplate;

   // create main MDI Frame window
   CMainFrame* pMainFrame = new CMainFrame;

   if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
      return FALSE;

   m_pMainWnd = pMainFrame;

   init_Units();

   GlSettings.initializeSettings();

   // Load default font file
   CString fontFile = getApp().getSystemSettingsFilePath("default.fnt");
   CFontList::initFontList(fontFile);

   // CCE Encrypted Data Access, Init support utility.
   // Initialize the XML, XSLT, and XML Sec subsystems.
   if ( !CInfrasecUtility::InitializeXMLSec() )
   {
      int jj = 0;  // Init failed !
   }

   if (m_readWriteJob.IsActivated())
   {
	   if (this->getCamcadLicense().hasBaseLicense())
	   {
		   // Do the job
		   m_readWriteJob.PerformImportAndExport(); // Miten -- call the import export function
	   }
	   else
	   {
		   // No base license
		   this->SetExitCode(ExitCodeNoBaseLicense);
	   }
	   
	   // That is the full job, send a quit message
	   m_pMainWnd->PostMessage(WM_QUIT);
	   
	   // That should be it for the log, go ahead and close it now.
	   this->CloseCamcadLogFile();
	   
	   // Already done, don't need the rest of InitInstance
	   return TRUE;
   }
   else
   {
	   m_pMainWnd->PostMessage(WM_QUIT);
	   return FALSE;
   }
   return TRUE;
}

static IUnknown *punk = NULL; // IUnknown of automation object

//remove
void CCEtoODBApp::RegisterInstanceInROT()
{
   /*if( punk != NULL )
   {
      return;
   }

   CLSID clsid;
   HRESULT hr( CLSIDFromProgID(L"CAMCAD.Application", &clsid) );
   if( FAILED(hr) )
   {
      return;
   }

   // Create an instance of the automation object
   LPCLASSFACTORY pClassFactory( NULL );
   hr = CoGetClassObject( clsid, CLSCTX_INPROC_SERVER, NULL, IID_IClassFactory, (void**)&pClassFactory );
   if( FAILED(hr) )
   {
      return;
   }
   ASSERT(pClassFactory != NULL);

   IID IIID_API = { 0xc3db3a2f, 0xb71, 0x11d2, { 0xba, 0x40, 0x0, 0x80, 0xad, 0xb3, 0x6d, 0xbb } };
   hr = pClassFactory->CreateInstance( NULL, IIID_API, (void**) &punk );
   pClassFactory->Release();


   // extended registration in ROT ensures that all objects are uniquely named    
   if( SUCCEEDED(hr) && punk != NULL )
   {
      LPRUNNINGOBJECTTABLE pROT = NULL;

      HRESULT hRes = GetRunningObjectTable(0, &pROT);
      if ( hRes == S_OK && pROT != NULL )
      {                                                                       
         CString sMonikerName;
         sMonikerName.Format("%s:%d", "CAMCAD.Application", GetCurrentProcessId());

         // create a moniker for ROT
         USES_CONVERSION;

         hRes = CreateItemMoniker(T2OLE("!"), T2OLE(sMonikerName.GetBuffer(39)), &m_pMoniker);
         if( hRes == S_OK )
         {
            // register application object in ROT
            // m_pMoniker and m_dwRegisterMoniker need to be store, they are needed to unregister application from ROT
            hRes = pROT->Register(0, punk, m_pMoniker, &m_dwRegisterMoniker);
            if( hRes != S_OK && hRes != MK_S_MONIKERALREADYREGISTERED )
            {                             
               m_pMoniker->Release();                          
               m_pMoniker = NULL;
               m_dwRegisterMoniker = NULL;
            }
            else
            {
               // Set date/time for new object in ROT
               FILETIME FileTime;
               CoFileTimeNow(&FileTime);
               pROT->NoteChangeTime(m_dwRegisterMoniker, &FileTime);
            }
         }
         pROT->Release();
      }
   }*/
}


/******************************************************************************
* CCEtoODBApp::ExitInstance
*/

//remove
int CCEtoODBApp::ExitInstance() 
{
   // Save schematic settings
   /*UnRegisterInstanceFromROT();
   schSettings.SaveSchSettings();

   CFontList::empty();

   if (editDlg)
   {
      editDlg->DestroyWindow();
      delete editDlg;
   }

   delete [] licenses;
   //free(licenses);

   //delete GlSettings.Fileextensions.Gerber;
   //delete GlSettings.Fileextensions.HPGL;
   //delete GlSettings.Fileextensions.IGES;
   //delete GlSettings.Fileextensions.APERTURE;

   // Devin -- Why is this here ??????? (and not where it belongs)
   FreeTrace();

   int defaultExitCode = CWinApp::ExitInstance();

   int finalAnswer = (this->GetExitCode() != ExitCodeSuccess) ? this->GetExitCode() : defaultExitCode;

   return finalAnswer;*/
	return 0;
}

void CCEtoODBApp::UnRegisterInstanceFromROT()
{
   if( m_dwRegisterMoniker != 0 )
   {                 
      LPRUNNINGOBJECTTABLE pROT = NULL;
      HRESULT hRes = GetRunningObjectTable(0, &pROT);
      if ( hRes == S_OK && pROT != NULL )
      {
         // remove moniker
         hRes = pROT->Revoke(m_dwRegisterMoniker);
         if ( hRes == S_OK )
         {                             
            m_pMoniker->Release();
            m_pMoniker = NULL;                              
            m_dwRegisterMoniker = 0;
         }
         pROT->Release();                    
      }                 
   }
}

/////////////////////////////////////////////////////////////////////////////
// CCEtoODBApp commands
BOOL CCEtoODBApp::PreTranslateMessage(MSG* pMsg)
{
   CSplashWnd::PreTranslateAppMessage(pMsg);

   if (pMsg->message == WM_MOUSEMOVE)
   {
      if (mouseView)
      {
         if (pMsg->hwnd != mouseView->m_hWnd)
         {
            //mouseView->EraseSearchCursor();
            mouseView = NULL;
         }
      }
   }

   return CWinApp::PreTranslateMessage(pMsg);
}

/******************************************************************************
* CheckWin32
*/
BOOL CheckWin32()
{
   OSVERSIONINFO ver;

   MAX_COORD = MAX_16BIT;

   ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
   if (GetVersionEx(&ver))
   {
      switch (ver.dwPlatformId)
      {
      case VER_PLATFORM_WIN32s:
         Platform = WIN32S;
         break;
      case VER_PLATFORM_WIN32_WINDOWS:
         Platform = WIN9x;
         break;
      case VER_PLATFORM_WIN32_NT:
         Platform = WINNT;
         MAX_COORD = MAX_32BIT;
         break;
      }

//CString platformString;
//platformString.Format("%d.%d.%d", ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber);
//ErrorMessage(ver.szCSDVersion, platformString);

   }
   return TRUE;
}

/******************************************************************************
* CCEtoODBApp::OnSettingsLoadGlobal
*/
void CCEtoODBApp::OnSettingsLoadGlobal() 
{
   CFileDialog FileDialog(TRUE, "SET", "*.set",
         OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST, 
         "CAMCAD Settings (*.set)|*.set|All Files (*.*)|*.*||", NULL);
   if (FileDialog.DoModal() != IDOK) return;
   
   GlSettings.LoadSettings(FileDialog.GetPathName());
}

/******************************************************************************
* CCEtoODBApp::OnLoadDataFile
*/
void CCEtoODBApp::OnLoadDataFile() 
{
   CString defExt("cc");
   CString extOptions( this->GetDecryptionAllowed() ? "*.cc;*.ccz;*.cce" : "*.cc;*.ccz" );
   CString prompt;
   prompt.Format("CAMCAD Data File (%s)|%s|All Files (*.*)|*.*||", extOptions, extOptions);
   // "CAMCAD Data File (*.cc;*.ccz)|*.cc;*.ccz|All Files (*.*)|*.*||"

   CFileDialog FileDialog(TRUE,
      defExt, extOptions, 
      OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
      prompt, NULL);

   if (FileDialog.DoModal() != IDOK) return;

   CAMCAD_File = FileDialog.GetPathName();
	CString path = CAMCAD_File.Left(CAMCAD_File.GetLength() - FileDialog.GetFileName().GetLength());

   if (!getSchematicLinkController().requestDeleteOfSchematicLink())
   {
      return;
   }

   LoadingDataFile = TRUE;
   OnFileNew();
   LoadingDataFile = FALSE;

   CCEtoODBView *view = getActiveView();
   if (view)
   {

      CCEtoODBDoc *doc = view->GetDocument();
      if (!doc || !doc->isFileLoaded())
      {
         doc->OnCloseDocument();
         return;
      }

      doc->SetTitle(CAMCAD_File);
      doc->docTitle = CAMCAD_File;
      doc->GenerateSmdComponentTechnologyAttribs(NULL,false);

      AddToRecentFileList(CAMCAD_File);

      CMainFrame *frame = (CMainFrame*)AfxGetMainWnd();
      frame->getNavigator().setDoc(doc);
      doc->SetProjectPathByFirstFile(path);
   }
}

/******************************************************************************
* OnLoadProjectFile
*/
void CCEtoODBApp::OnLoadProjectFile() 
{
   CFileDialog FileDialog(TRUE, "CCP", "*.CCP",
         OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, 
         "CAMCAD Project File (*.CCP)|*.CCP|All Files (*.*)|*.*||", NULL);
   if (FileDialog.DoModal() != IDOK) return;
   CAMCAD_File = FileDialog.GetPathName();

   if (!getSchematicLinkController().requestDeleteOfSchematicLink())
   {
      return;
   }

   LoadingProjectFile = TRUE;
   OnFileNew();
   LoadingProjectFile = FALSE;

   CCEtoODBView *view = getActiveView();
   if (view)
   {
      CCEtoODBDoc *doc = view->GetDocument();
      doc->SetTitle(CAMCAD_File);
      doc->docTitle = CAMCAD_File;
		doc->GenerateSmdComponentTechnologyAttribs(NULL,false);
   }
	CMainFrame *frame = (CMainFrame*)AfxGetMainWnd();
	if (view)
   {
		frame->getNavigator().setDoc(view->GetDocument());
   }
}

void CCEtoODBApp::OnFileNew()
{ CWinApp::OnFileNew(); }

void CCEtoODBApp::OnAppExit()
{  CWinApp::OnAppExit(); }

/////////////////////////////////////////////////////////////////////////////

CTXPJob::CTXPJob()
: m_inputFiletype(fileTypeCamcadData)  // Default is CCZ, may be set to ODB++ on command line.
, m_outputFiletype(fileTypeUnknown)    // Default is CCZ, no other option at present but ready should new options come down the road.
, m_activated(false)
{
}

//--------------------------------------------------------------------------

CString CTXPJob::GetResolvedDestinationFilePath()
{
   // Resolve destination file name.
   // If output is cc/ccz then ensure extension is cc or ccz.
   // If other format then leave it as-is.

   // First combine working dir and output filename (which may have additional relative folder names prpefixed).
   CFilePath resolvedPath( this->GetWorkingDir(), false );  // false = beautify off, to stop changes to upper/lower case, leave name as-is.
   resolvedPath.setFileName( this->GetDestinationFilename() );

   // Now ensure CCZ extension if CCZ file is to be output.
   CString resolvedName( resolvedPath.getPath() );

   if (this->GetRawOutputFileType() == fileTypeUnknown ||
      this->GetRawOutputFileType() == fileTypeCamcadData)
   {
      CFilePath path( resolvedName, false );
      CString extension = path.getExtension();

      if (extension.CompareNoCase("cc") == 0 || extension.CompareNoCase("ccz") == 0)
      {
         // Okay as-is
      }
      else
      {
         // File has other extension or no extension.
         // Default to ccz.
         // Leave what ever is there as-is, append new extension.

         // Add dot if it does not already end with dot
         if (resolvedName.Right(1).Compare(".") != 0)
            resolvedName += ".";

         // Add the ccz
         resolvedName += "ccz";
      }
   }

   return resolvedName;
}


//--------------------------------------------------------------------------

CString CTXPJob::GetResolvedSourceFilePath()
{
   // Resolve source file path.
   // Combine working dir and filename.
   // Note that filename may contain additional folders that are relative to working dir.

   CFilePath resolvedPath( this->GetWorkingDir() );
   resolvedPath.setFileName( this->GetSourceFilename() );

   return resolvedPath.getPath();
}

/////////////////////////////////////////////////////////////////////////////

CReadWriteJob::CReadWriteJob()
: m_inputFiletype(fileTypeUnknown)  // required to be set in command line param
, m_outputFiletype(fileTypeUnknown) // optional in command line, unknown defaults to ouput ccz
, m_activated(false)
, m_flippedPcbNameSuffix("_$$Flipped")
{
   // Not all readers available in CAMCAD are supported by this feature (by design).
   // E.g. some readers read formats that are not layout files.
   // E.g. some readers read test probe solutions that are intended to be appened to some other active CCZ file.
   // For what ever reason, some readers are exluded, see dts0100626598.
   // There are two choices to implement the limitation.
   // The first is to make a list of those that are to be excluded and allow ones not in the list through.
   // That has the future effect of automatically including support for some new reader that has not been excluded.
   // The other choice is to make list of what is accepted and exlude all others.
   // That will exclude future readers unless specifically added to supported list.
   // Which is better?  My inclination is to go with the former, so nothing extra has to be done to support a new reader.
   // BUT!  Something extra DOES have to be done... it has to be TESTED with this feature !!!
   // Since whether that will be thought of or not is not predictable, we are going with the safe route.
   // Only those tested and deemed appropriate are supported, anything new that comes along has to be
   // added to this list to become supported.

   m_supportedInputFileTypes.Add(1);  //	Autocad DXF
   m_supportedInputFileTypes.Add(2);  //	HPGL
   m_supportedInputFileTypes.Add(3);  //	Gerber
   m_supportedInputFileTypes.Add(5);  //	Unicam
   m_supportedInputFileTypes.Add(7);  //	PADS PCB
   m_supportedInputFileTypes.Add(8);  //	PDIF PCB
   m_supportedInputFileTypes.Add(10); //	Protel PCB
   m_supportedInputFileTypes.Add(12); //	Mentor Boardstation
   m_supportedInputFileTypes.Add(16); //	IGES V3
   m_supportedInputFileTypes.Add(17); //	IPC 350/356
   m_supportedInputFileTypes.Add(18); //	EDIF
   m_supportedInputFileTypes.Add(19); //	Expedition HKP ASCII
   m_supportedInputFileTypes.Add(21); //	GenCAD
   m_supportedInputFileTypes.Add(22); //	Barco
   m_supportedInputFileTypes.Add(23); //	OrCAD PCB
   m_supportedInputFileTypes.Add(25); //	Cadence Allegro
   m_supportedInputFileTypes.Add(26); //	Mentor Neutral
   m_supportedInputFileTypes.Add(29); //	Redac CADIF
   m_supportedInputFileTypes.Add(30); //	Accel EDA
   m_supportedInputFileTypes.Add(35); //	Theda
   m_supportedInputFileTypes.Add(36); //	HP EGS
   m_supportedInputFileTypes.Add(38); //	Scicards CII
   m_supportedInputFileTypes.Add(41); //	CR3000
   m_supportedInputFileTypes.Add(45); //	GenCAM
   m_supportedInputFileTypes.Add(48); //	ODB++
   m_supportedInputFileTypes.Add(52); //	Fabmaster
   m_supportedInputFileTypes.Add(57); //	DDE Supermax
   m_supportedInputFileTypes.Add(60); //	CR5000
   m_supportedInputFileTypes.Add(63); //	Calay Prisma
   m_supportedInputFileTypes.Add(64); //	Unidat
   // Don't put 106 in here, see special handling for the Schematic Netlist stuff.
   // For Schematic Netlist in here we put each specific Netlist File type.
   m_supportedInputFileTypes.Add(107); // CAMCAD Netlist
   m_supportedInputFileTypes.Add(108); // Keyin Netlist
   m_supportedInputFileTypes.Add(109); // Nets.nets Netlist
   m_supportedInputFileTypes.Add(110); // DxDesigner Netlist
   m_supportedInputFileTypes.Add(111); // PADS Logic Netlist
   // And of course we support CC/CCZ (useful for when output is not CC/CCZ, otherwise silly to import CCZ and write CCZ)
   m_supportedInputFileTypes.Add(122); // CAMCAD CC and CCZ


   // Supported Writers
   // The two originally supported formats.
   m_supportedOutputFileTypes.Add(fileTypeCamcadData);       // 122  aka CC and CCZ
   m_supportedOutputFileTypes.Add(fileTypeOdbPlusPlus);      // 48

   // And for vPlan 11.2 we have added a dozen test exports. But then it became vPlan 11.3.  
   m_supportedOutputFileTypes.Add(fileTypeAeroflexCb);       // 80
   m_supportedOutputFileTypes.Add(fileTypeHp3070);           // 28
   m_supportedOutputFileTypes.Add(fileTypeIpl);              // 73
   m_supportedOutputFileTypes.Add(fileTypeTeradyne228xCkt);  // 75
   m_supportedOutputFileTypes.Add(fileTypeTeradyne228xNav);  // 100
   m_supportedOutputFileTypes.Add(fileTypeTeradyneSpectrum); // 90
   m_supportedOutputFileTypes.Add(fileTypeAgilentI1000ATD);  // 101
   m_supportedOutputFileTypes.Add(fileTypeHiokiICT);         // 124
   m_supportedOutputFileTypes.Add(fileTypeScorpion);         // 84
   m_supportedOutputFileTypes.Add(fileTypeSeicaParNod);      // 87
   m_supportedOutputFileTypes.Add(fileTypeSpea4040);         // 72
   m_supportedOutputFileTypes.Add(fileTypeTakaya9);          // 67

   // After Jun 2012 DFT meeting in Yavne we added these.
   m_supportedOutputFileTypes.Add(fileTypeAcculogic);        // 99
   m_supportedOutputFileTypes.Add(fileTypeFixture);          // 76
   m_supportedOutputFileTypes.Add(fileTypeHuntron);          // 66
   m_supportedOutputFileTypes.Add(fileTypeKonradICT);        // 98
   m_supportedOutputFileTypes.Add(fileTypeTestronicsICT);    // 102
   m_supportedOutputFileTypes.Add(fileTypeTriMda);           // 53


   // Added for the "new definition" of vPlan 11.2.
   // DR 885673 added Unicam PDW export support.
   m_supportedOutputFileTypes.Add(fileTypeUnicam);           //  5


}

//--------------------------------------------------------------------------
 // Keep
bool CReadWriteJob::IsSupportedInputFileType(FileTypeTag filetype)
{
   for (int i = 0; i < m_supportedInputFileTypes.GetCount(); i++)
   {
      int candidate = m_supportedInputFileTypes.GetAt(i);
      if (candidate == (int)filetype)
         return true;
   }

   return false;
}

//--------------------------------------------------------------------------
 //Keep
bool CReadWriteJob::IsSupportedOutputFileType(FileTypeTag filetype)
{
   // I don't want to put this one in the supported types list, but when file type
   // is Unknown export defaults to CCZ, this is a leftover from the days when
   // CCZ was the only exported type and writer param didn't exist.
   if (filetype == fileTypeUnknown)
      return true;

   for (int i = 0; i < m_supportedOutputFileTypes.GetCount(); i++)
   {
      int candidate = m_supportedOutputFileTypes.GetAt(i);
      if (candidate == (int)filetype)
         return true;
   }

   return false;
}

//--------------------------------------------------------------------------

// A nicer solution would be to put the capability in the CamcadProduct to check
// multiple file types. But it is not there now and too much to add for now.
// So just handle it locally, right here.

bool CReadWriteJob::IsSchematicNetlistFileType(FileTypeTag filetype)
{
   if (
      filetype == 107 || // CAMCAD Netlist
      filetype == 108 || // Keyin Netlist
      filetype == 109 || // Nets.nets Netlist
      filetype == 110 || // DxDesigner Netlist
      filetype == 111    // PADS Logic Netlist
      )
   {
      return true;
   }

   return false;
}

//--------------------------------------------------------------------------

bool CReadWriteJob::InputDirectoryOnly(FileTypeTag filetype)
{
   // These readers can accept directory only as input specifier.
   // This is being checked/enforced in too many little separate code chunks.
   // (Search for "dironly" variable to see.)
   // Could use some centralization. Until that day comes, it is repeated here too.

   switch (filetype)
   {
   case fileTypeHp5dx:
   case fileTypeOdbPlusPlus:
   case fileTypeVb99Layout:
      return true;
   }

   return false;
}

//--------------------------------------------------------------------------

void CReadWriteJob::WriteUsage()
{
   // Write usage and reader symbols, aka FileTypeTag enum, to log file.

   CString commandUsed( GetCommandLine() );
   if (commandUsed.Find(" ") > -1)
      commandUsed.Truncate(commandUsed.Find(" "));

   CString usageStr;
   usageStr.Format("Usage:  %s /reader=n /input=<File or Folder Name> /output=<CC or CCZ File Name> /logfile=<logfile name>\n",
      commandUsed);

   getApp().LogMessage(usageStr);
   getApp().LogMessage("Where n for reader is one of the numbers from list below.\nExample: /reader=6\n");
   getApp().LogMessage("\n");
   getApp().LogMessage("The input file or folder and output file are absolute paths.\n");
   getApp().LogMessage("That means they should start with either the slash or the drive name.\n");
   getApp().LogMessage("File or folder names with embedded blanks must be enclosed in double quotes.\n");
   getApp().LogMessage("The input file may be a comma separated list of file names.\n");
   getApp().LogMessage("Examples: /input=c:/caddata/pads.asc\n");
   getApp().LogMessage("          /input=/caddata/pads.asc\n");
   getApp().LogMessage("          /input=\"c:/cad data/pads.asc\"\n");
   getApp().LogMessage("          /input=\"/test/Brd.txt,/test/Pad.txt,/test/Rte.txt,/test/Sym.txt\"\n");
   getApp().LogMessage("\n");
   getApp().LogMessage("The output file should be a single file, full absolute path specified.\n");
   getApp().LogMessage("If the file has extension \".cc\" then a CC file will be written.\n");
   getApp().LogMessage("Otherwise, a CCZ file is written. If the file name specified includes an\n");
   getApp().LogMessage("extension that is not \".cc\" or \".ccz\", the file name will be used as-is and\n");
   getApp().LogMessage("\".ccz\" extenstion will be added. Specifying any extension is optional.\n");
   getApp().LogMessage("Examples: /output=c:/result/myfile.ccz\n");
   getApp().LogMessage("          /output=/result/myfile\n");
   getApp().LogMessage("\n");
   getApp().LogMessage("The logfile parameter is optional. If not specified the log is written to \"camcad.log\"\n");
   getApp().LogMessage("The logfile value may be an absolute that that redirects the log file to some specific folder.\n");
   getApp().LogMessage("The logfile value may be only the log file name, in which case the log file is written to the standard log file folder.\n");
   getApp().LogMessage("If the log file specified can not be opened for writing, the standard log file \"camcad.log\" is used instead\n");
   getApp().LogMessage("and shall contain an error message about the specified log file.\n");
   getApp().LogMessage("Examples: /logfile=/myjob/mylog.txt\n");
   getApp().LogMessage("          /logfile=mylog.txt\n");
   getApp().LogMessage("\n");

   usageStr.Format("Usage:  %s /reader=n /input=<File or Folder Name> /writer=m /output=<CC or CCZ File Name> /logfile=<logfile name>\n",
      commandUsed);
   getApp().LogMessage(usageStr);
   getApp().LogMessage("When no /writer parameter is provided the output defaults to CCZ file. This is the form shown in first Usage above.\n");
   getApp().LogMessage("When /writer=m is supplied, m should be one of the writer numbers shown below.\n");
   getApp().LogMessage("\n");

   WriteReaderSymbols();
   getApp().LogMessage("\n");
   WriteWriterSymbols();
}


//--------------------------------------------------------------------------

void CReadWriteJob::WriteReaderSymbols()
{
   // Write reader symbols, aka FileTypeTag enum mapped to product names, to log file.

   // CAMCAD Readers have product numbers, but that is not the value this feature runs on.
   // This feature runs on the "file type" number, mainly because that is the number the API has always run on.
   // Hence, for consistency, we do the same.
   // But the difference it not particularly important to users, so we do not try to explain it here.
   // We just list the number and the importer product with which it is associated.

   getApp().LogMessage("Supported Readers (also known as Importers)\n");
   getApp().LogMessage("The integer values shown are used for n in the /reader=n command line parameter.\n");
   getApp().LogMessage("The associated Reader (Importer) product name is shown next to it's number.\n");
   getApp().LogMessage("\n");

   for (EnumIterator(FileTypeTag, filetypeIterator); filetypeIterator.hasNext(); )
   {
      FileTypeTag ft = filetypeIterator.getNext();

      if (IsSupportedInputFileType(ft))
      {
         FileTypeTag mappedFt = GetMappedFileType(ft);
         CString addendum;
         
         if (IsSchematicNetlistFileType(ft))
         {
            // Schematic Netlist individual file types all map to a "parent" (or "family" file type
            // for producr mapping purposes. In an addendum for the line in the logfile add
            // the specific file type's format name.
            addendum.Format("  [%s]", fileTypeTagToString(ft)); // original ft
         }
      }
   }

}

//--------------------------------------------------------------------------

void CReadWriteJob::WriteWriterSymbols()
{
   // Write writer symbols, aka FileTypeTag enum mapped to product names, to log file.

   // CAMCAD Writers have product numbers, but that is not the value this feature runs on.
   // This feature runs on the "file type" number, mainly because that is the number the API has always run on.
   // Hence, for consistency, we do the same.
   // But the difference it not particularly important to users, so we do not try to explain it here.
   // We just list the number and the exporter product with which it is associated.

   getApp().LogMessage("Supported Writers (also known as Exporters)\n");
   getApp().LogMessage("The integer values shown are used for n in the /writer=n command line parameter.\n");
   getApp().LogMessage("The associated Writer (Exporter) product name is shown next to it's number.\n");
   getApp().LogMessage("\n");

   for (EnumIterator(FileTypeTag, filetypeIterator); filetypeIterator.hasNext(); )
   {
      FileTypeTag ft = filetypeIterator.getNext();

      if (IsSupportedOutputFileType(ft))
      {
         FileTypeTag mappedFt = GetMappedFileType(ft);
         CString addendum;
         
         if (IsSchematicNetlistFileType(ft))
         {
            // Schematic Netlist individual file types all map to a "parent" (or "family" file type
            // for producer mapping purposes. In an addendum for the line in the logfile add
            // the specific file type's format name.
            addendum.Format("  [%s]", fileTypeTagToString(ft)); // original ft
         }
      }
   }
}

//--------------------------------------------------------------------------
// Keep
bool CReadWriteJob::ImportCadFiles()  
{
   //*rcf Pending - use expanded file list if /input=folder
	// Miten -- The start of import function
   FileTypeTag mappedFileType = fileTypeCamcadData; //GetMappedFileType(GetRawInputFileType());
   
   if (!GetSourceFilename().IsEmpty())
	   // Not clear whether OpenCadFile or OpenDocumentFile is better  here.
	   getApp().OpenCadFile(GetSourceFilename());

   return true;

   //if (mappedFileType == fileTypeCamcadData)
   //{
      // Read cc/ccz file if specified in /input, otherwise assume it was specified as
      // ordinary ccz file on command line.
      
      // else Should check that a CCZ was already loaded...
   //}
   /*else
   {
      // Import foreign CAD format
      API api;
      int status = api.Import(mappedFileType, GetResolvedSourceFilename(), "" /*LPCTSTR formatString*///);

      /*if (status == RC_NO_LICENSE)
      {
         getApp().LogMessage("No license for CAD Reader.\n");  // reader, importer, this feature tends to use reader instead of importer
         getApp().UpdateExitCode(ExitCodeNoImporterLicense);
         return false; // Import utterly failed
      }
      else if (status != RC_SUCCESS)
      {
         if (this->InputDirectoryOnly(GetMappedFileType(GetRawInputFileType())))
         {
            getApp().LogMessage("This reader accepts directories only in /input parameter, not individual files.\n");
         }
         getApp().LogMessage("Error(s) encountered during import.\n");
         ////can't do this yet:   getApp().UpdateExitCode(ExitCodeGeneralError);
         ////because vplan has no gray area, only catastrophic failure or success, no "success but with issues".
         ////if camcad returns non-zero exit code then vplan will not import the result.

         ////the following, on the other hand, is just within camcad, and remains correct
         // But don't make it fatal, don't stop ccz write with return false.
         // Let ccz write go ahead and try, might be useful partial result for user, might be minor errors (hopefully reported in log file)
      }
      else
      {
         // Apparant success from CAD import. Load any auxiliary data files we find present.
         // If file "DftSolution.xml" exists in working directory then load it.
         if (!this->GetWorkingDir().IsEmpty())
         {
            CFilePath dftXmlFilePath = this->GetWorkingDir();
            dftXmlFilePath.setFileName( "DftSolution.xml" ); // Possibly make this name controllable by command line param?
            CString filestr = dftXmlFilePath.getPath();

            if (fileExists(filestr))
            {
               getApp().LogMessage("\n"); // This is to put a blank line in log between what came before and DFT Solution processing.

               CCEtoODBDoc *doc = getActiveView()->GetDocument();
               if (doc != NULL)
               {
                  FileStruct *dftSlnFile = doc->getFileList().addNewFile(DFT_SOLUTION_XML_FILESTRUCT_NAME, fileTypeUnknown);
                  dftSlnFile->setShow(false); // Don't want this one visible when export is all done, hide it now.

                  bool setCurrentSolution = false;
                  doc->LoadDFTSolutionFile(filestr, *dftSlnFile, setCurrentSolution);

                  // Maybe this ought to be a CAMCADDoc method?
                  this->ApplyDftSolution(doc, dftSlnFile);
               }
            }
         }
      }
   }*/

}





//--------------------------------------------------------------------------
short CReadWriteJob::ExportCadFiles()
{
   if (this->GetRawOutputFileType() == fileTypeUnknown ||
      this->GetRawOutputFileType() == fileTypeCamcadData)
   {
      return ExportCamcadFile();
   }

   if (this->IsSupportedOutputFileType(this->GetRawOutputFileType()))
   {
      API api;
	  return api.Export(this->GetRawOutputFileType(), GetResolvedDestinationFilename()); // Miten -- file exporting function is called here
   }

   return RC_GENERAL_ERROR;
}

//--------------------------------------------------------------------------
short CReadWriteJob::ExportCamcadFile()
{
   // Save to cc if file extension is specifically cc, otherwise save to ccz

   API api;
   short res = 0;

   CFilePath outputFilePath( GetResolvedDestinationFilename() );
   CString extension = outputFilePath.getExtension();

   /*if (extension.CompareNoCase("CC") == 0)
      res = api.SaveDataFileAs( GetResolvedDestinationFilename() );
   else
      res = api.SaveCompressedDataFileAs( GetResolvedDestinationFilename() );*/
 
   return res;
}

//--------------------------------------------------------------------------
void CReadWriteJob::PerformImportAndExport()
{
   // Do not use isLicensed() check here. If we do, then we have to check both styles of
   // base license as well as CPCMD license, as this feature should run if any of these
   // are licensed. That seems like too much license thrashing, since licenses were already
   // thrashed once upon startup. During initial license checked the "is allowed" field is
   // set, to enable this feature for each license that should allow access.
   // 
   // So take a shortcut, just check if feature "is allowed"
   //

   /*CamcadProduct* camcadProduct = getApp().getCamcadLicense().findCamcadProduct(camcadProductCommandLineCPCMD);
   if (camcadProduct == NULL || !camcadProduct->getAllowed()) // if can't find or is not allowed
   {
      if (camcadProduct == NULL)
         getApp().LogMessage("Command line import/export product not found.\n");
      else
         getApp().LogMessage("Command line import/export not licensed.\n");

      return;
   }
   else
   {
      // Supported check here means we have a product we allow to be used, does not
      // mean it is licensed for particular user to use, that is tested elswhere.*/
      if (!this->IsSupportedInputFileType(GetRawInputFileType()) || !this->IsSupportedOutputFileType(GetRawOutputFileType()))
      {
         getApp().LogMessage("Error in command line parameter for /reader or /writer.\n\n");
         WriteUsage();
         getApp().UpdateExitCode(ExitCodeGeneralError);
      }
      else
      {
         // Write parameters to log file
         CString msg;

		 getApp().LogMessage("License found. Proceeding with Import and Export\n\n");

		 // Miten - We get the input file name and type at this point
         //msg.Format("Reader=(%d)[%s]\n", GetRawInputFileType(), fileTypeTagToString(GetRawInputFileType()));
		 msg.Format("Reader=[%s]\n",fileTypeTagToString(GetRawInputFileType()));
         getApp().LogMessage(msg);

         CString resolvedSource( GetSourceFilename() );  // Starts with basic name as given by user
         bool isDirectory = false;

         // Determine if resolved input is a directory
         CFileFind finder;
         BOOL found = finder.FindFile( resolvedSource );
         if (found)
         {
            BOOL more = finder.FindNextFile();
            isDirectory = finder.IsDirectory()?true:false;
         }

         // Echo parameter to log file, add designator note if it is a directory
         msg.Format("\nInput=(%s)", GetSourceFilename());
         if (isDirectory)
            msg += " [Directory]";
         msg += "\n";
         getApp().LogMessage(msg);

         bool requiresDirectory = InputDirectoryOnly(GetMappedFileType(GetRawInputFileType()));
         bool proceedWithOperation = true;

         if (requiresDirectory && !isDirectory)
         {
            // Importer requires a directory and name is not a directory, report an error
            getApp().LogMessage("\nThis reader requires a directory path for /input value.\nImport aborted.\n");
            proceedWithOperation = false;
         }
         else if (isDirectory && !requiresDirectory)
         {
            // User named a directory, but importer does not require a directory. Such importers (usually) want
            // to import each file individually. So resolve the input named to be all files in the directory.
            resolvedSource = GetResolvedSourceFilename();
            // This turns comma separate list into column with [ and ] start and end chars
            resolvedSource.Replace(",", "]\n[");
            msg.Format("[Files being imported ...]\n[%s]\n", resolvedSource);
            getApp().LogMessage(msg);
         }

         // We still write the output param, so it can be inspected in log file, even if operation is
         // already known to be aborted.
		 // Miten - We get the output file name and type at this point
         if (GetRawOutputFileType() != fileTypeUnknown)
         {
            //msg.Format("\nWriter=(%d)[%s]\n", GetRawOutputFileType(), fileTypeTagToString(GetRawOutputFileType()));
			msg.Format("\nWriter=[%s]\n", fileTypeTagToString(GetRawOutputFileType()));
            getApp().LogMessage(msg);
         }
         msg.Format("\nOutput=(%s)", GetDestinationFilename());
         if (GetDestinationFilename().Compare(GetResolvedDestinationFilename()) != 0)
            msg += " [" + GetResolvedDestinationFilename() + "]";
         msg += "\n\n";
         getApp().LogMessage(msg);

         // Import cad file(s) then export cc/ccz
		 // Miten -- first perform the import and on success perform the export
		 // Miten -- CCE reader would be form this point
         if (proceedWithOperation && ImportCadFiles())
         {
			 getApp().LogMessage("\nInternal data structure constructed Successfully. Ready to Export\n");

			 getApp().LogMessage("\nExporting.\n");

            // Put a separator in the log, to separate Import from Export records
            getApp().LogMessage("\n-------------------------------------------------------------------\n\n");

            if (ExportCadFiles() != RC_SUCCESS)
            {
               CString msg;
               msg.Format("CAD Export failed.\n\n");
               getApp().LogMessage(msg);
               //// probably don't want this fatal error sent until gray area
               //// error is supported in vplan.
               /////getApp().UpdateExitCode(ExitCodeGeneralError);
            }
			else
				getApp().LogMessage("\nExport complete.\n");
         }
         else
         {
            CString fileTypeStr("CC/CCZ"); // CC/CCZ is the default output type
            if (this->GetRawOutputFileType() != fileTypeUnknown)
            {
               // Output file type was specified on command line, update the msg string accordingly.
               fileTypeStr = fileTypeTagToString(this->GetRawOutputFileType());
            }
            CString msg;
            msg.Format("CAD Import failed, %s file creation has been aborted.\n\n", fileTypeStr);
            getApp().LogMessage(msg);
            getApp().UpdateExitCode(ExitCodeGeneralError);
         }
      }
   //}
}

//--------------------------------------------------------------------------
 // Keep
CString CReadWriteJob::GetResolvedDestinationFilename()
{
   // Resolve destination file name.
   // If output is cc/ccz then ensure extension is cc or ccz.
   // If other format then leave it as-is.

   CString resolvedName( GetDestinationFilename() );

   // If file type is not set to other than unknown, then output defaults to ccz.
   if (this->GetRawOutputFileType() == fileTypeUnknown)
   {
      CFilePath path( resolvedName );
      CString extension = path.getExtension();

      if (extension.CompareNoCase("cc") == 0 || extension.CompareNoCase("ccz") == 0)
      {
         // Okay as-is
      }
      else
      {
         // File has other extension or no extension.
         // Default to ccz.
         // Leave what ever is there as-is, append new extension.

         // Add dot if it does not already end with dot
         if (resolvedName.Right(1).Compare(".") != 0)
            resolvedName += ".";

         // Add the ccz
         resolvedName += "ccz";
      }
   }

   return resolvedName;
}


//--------------------------------------------------------------------------

CString CReadWriteJob::GetResolvedSourceFilename()
{
   // Resolve source file name.
   // If user specified a directory then resolved name is comma separated list of dir contents.

   CString resolvedName( GetSourceFilename() );

   // Nuke trailing folder delimiters
   while (resolvedName.Right(1) == "\\" || resolvedName.Right(1) == "/")
      resolvedName.Truncate(resolvedName.GetLength()-1);

   CFileFind finder;
   BOOL found = finder.FindFile( resolvedName );

   // Resolve directory to individual files unless reader accepts Dirs only as input
   if (found && !InputDirectoryOnly(GetMappedFileType(GetRawInputFileType())))
   {
      BOOL more = finder.FindNextFile();

      if (finder.IsDirectory())
      {
         resolvedName.Empty();  // Clear/Reset result
         CString fileDirectory = finder.GetFilePath() + "\\";  // Save full dir path
         BOOL moreFiles = finder.FindFile(fileDirectory + "*.*");  // Find all files in dir

         while (moreFiles)
         {
            moreFiles = finder.FindNextFile();
           CString nextFileName = finder.GetFileName();

           if (finder.IsDirectory())
           {
              // Skip, not supporting recursive drill-down of folders in folders
           }
           else if (finder.IsDots())
           {
              // Skip self (.) and parent (..)
           }
           else
           {
              // Looks like a keeper
              CString fullFilePath( fileDirectory + nextFileName );
              if (!resolvedName.IsEmpty())
                 resolvedName += ",";
              resolvedName += fullFilePath;
           }
         }
      }
      // else assume okay as-is
   }

   return resolvedName;
}



typedef int (APIENTRY FUNC)(int, int);


//******************************************************************************
 //keep
CString CMyCommandLineInfo::ParseParameterValue(CString cmdLineParam)
{
   CString val;

   int indx = cmdLineParam.Find("=");

   if (indx != -1)
   {
      val = cmdLineParam.Mid(indx+1);
   }

   return val;
}

// To check if both input and output parameters are valid
// Keep
BOOL CMyCommandLineInfo::validParameters()
{
	if (validInput && validOutput)
		return true;
	else
	{
		PrintErrorMessgeToLogFile();
		return false;
	}
}

// Error message to be printed to the log file
// Keep
void CMyCommandLineInfo::PrintErrorMessgeToLogFile()
{
	//CString log = "\\log.txt";
	CCEtoODBApp *pApp = (CCEtoODBApp*)AfxGetApp();
	CString buf ="Error - Invalid command line arguments.\n\nUsage : \ninput=< CC/CCZ/CCE filename > output=< Ouput Directory >\neg : input=C:\\CCE\\Example.cce output=C:\\OutputDirectory\n";
	buf.Format("%s", buf);
	AfxMessageBox(buf);

	//pApp->SetLogFileName(log);
	pApp->LogMessage("\n\nError - Invalid command line arguments.\n\n");
	pApp->LogMessage("Usage : \n");
	pApp->LogMessage("input=< CC/CCZ/CCE filename > output=< Ouput Directory >\n");
	pApp->LogMessage("eg : input=C:\\CCE\\Example.cce output=C:\\OutputDirectory\n");
}
//******************************************************************************
// Keep
void CMyCommandLineInfo::ParseParam(LPCTSTR lpszParam, BOOL bFlag, BOOL bLast)
{
   // to run camcad in flavored mode :
   // camcad /xxxx
   // this will diplay the correct title and select the right license.
   // this means our license generator needs a different license for 
   // PADS PCB read for ANSOFT and PROFESSIONAL.

   // Get the appliction
   CCEtoODBApp *pApp = (CCEtoODBApp*)AfxGetApp();

   Product = PRODUCT_PROFESSIONAL;
   //this->m_bShowSplash = false;

   
   //else if (!STRNICMP(lpszParam, "INPUT", 5))
   if (!STRNICMP(lpszParam, "INPUT", 5))
   {
      pApp->m_readWriteJob.SetActivated();
      CString val(this->ParseParameterValue(lpszParam));
      pApp->m_readWriteJob.SetSourceFilename(val);

	  FileTypeTag filetype = intToFileTypeTag(122);
      pApp->m_readWriteJob.SetInputFileType(filetype);


	  //CString val;  // Starts with basic name as given by user
      bool isDirectory = false;

      // Determine if resolved input is a directory
      CFileFind finder;
      BOOL found = finder.FindFile( val ); // Checking whether file exists

	  // Checking for valid extension of the file
	  int index = val.ReverseFind('.');
	  CString fileExtension;
	  if (index != -1)
		  fileExtension = val.Mid(index+1);
	  BOOL isValidExtension = false;
	  if ( ( fileExtension == "cce" ) || ( fileExtension == "ccz" ) || ( fileExtension == "cc" ) )
		  isValidExtension = true;

	  // Checking if the input param value is a directory
	  isDirectory = finder.IsDirectory()?true:false;

	  //Check if the input param
	  // 1) Exists 2) Is not a Directory 3) Has a valid Extension
	  if ( found && !isDirectory && isValidExtension )
      {
		  validInput = true;
      }
   }
   else if (!STRNICMP(lpszParam, "OUTPUT", 6))
   {
      pApp->m_readWriteJob.SetActivated();
      CString val(this->ParseParameterValue(lpszParam));
      pApp->m_readWriteJob.SetDestinationFilename(val);

	  pApp->m_txpJob.SetWorkingDir(val);
      pApp->m_readWriteJob.SetWorkingDir(val);

	  FileTypeTag filetype = intToFileTypeTag(48);
      pApp->m_readWriteJob.SetOutputFileType(filetype);

	  pApp->SetDecryptionAllowed(true);

	  if ( ! (val.IsEmpty()))
	  {
		  CString log = val;
		  log += "\\log.txt";
		  pApp->SetLogFileName(log);
		  validOutput = true;
	  }
   }

   // figure out which product (Graphic is default)
   // only one product, first parameter
   // Remove
   /*if (!STRCMPI(lpszParam, "GRAPHIC"))
   {
      Product = PRODUCT_GRAPHIC;
   }
   else if (!STRCMPI(lpszParam, "PROFESSIONAL"))
   {
      Product = PRODUCT_PROFESSIONAL;
   }
   else if (!STRCMPI(lpszParam, "PCB_TRANSLATOR"))
   {
      Product = PRODUCT_PCB_TRANSLATOR;
   }
   else if (!STRCMPI(lpszParam, "VISION"))
   {
      Product = PRODUCT_VISION;
   }
   else if (!STRCMPI(lpszParam, "NoUI") || !STRCMPI(lpszParam, "NoGUI"))   // wolf always gets it wrong GUI and UI
   {
      NoUI = TRUE;
   }
   else if (!STRCMPI(lpszParam, "UI") || !STRCMPI(lpszParam, "GUI"))
   {
      NoUI = FALSE;
   }
   else if (!STRCMPI(lpszParam, "NOLIC"))
   {
      pApp->CheckoutLics = FALSE;
   }
   else if (!STRCMPI(lpszParam, "LICTIME"))
   {
      pApp->LicenseTimerEnabled = TRUE;
   }
   else if (!STRCMPI(lpszParam, "HIDE"))
   {
      pApp->MostlyHideCamcad = TRUE;
   }
   else if (!STRNICMP(lpszParam, "LOGFILE", 7))
   {
      CString val(this->ParseParameterValue(lpszParam));
      pApp->SetLogFileName(val); // does not matter if it is empty string
   }
   else if (!STRNICMP(lpszParam, "READER", 6))
   {
      pApp->m_readWriteJob.SetActivated();
      CString val(this->ParseParameterValue(lpszParam));
      int ival = atoi(val);
      FileTypeTag filetype = intToFileTypeTag(ival);
      pApp->m_readWriteJob.SetInputFileType(filetype);
   }
   else if (!STRNICMP(lpszParam, "WRITER", 6))
   {
      pApp->m_readWriteJob.SetActivated();
      CString val(this->ParseParameterValue(lpszParam));
      int ival = atoi(val);
      FileTypeTag filetype = intToFileTypeTag(ival);
      pApp->m_readWriteJob.SetOutputFileType(filetype);
   }*/
   // Remove
   /*else if (!STRNICMP(lpszParam, "IMSETTINGS", 10))
   {
      CString val(this->ParseParameterValue(lpszParam));
      pApp->setCmdLineImportSettingsFilePath(val);
   }
   else if (!STRNICMP(lpszParam, "EXSETTINGS", 10))
   {
      CString val(this->ParseParameterValue(lpszParam));
      pApp->setCmdLineExportSettingsFilePath(val);
   }
   else if (!STRNICMP(lpszParam, "WDIR", 4))
   {
      CString val(this->ParseParameterValue(lpszParam));
      pApp->m_txpJob.SetWorkingDir(val);
      pApp->m_readWriteJob.SetWorkingDir(val);
   }
   else if (!STRNICMP(lpszParam, "TXPODB", 6))
   {
      CString val(this->ParseParameterValue(lpszParam));
      pApp->m_txpJob.SetSourceFilename(val);
      pApp->m_txpJob.SetInputFileType(fileTypeOdbPlusPlus);
   }
   else if (!STRNICMP(lpszParam, "TXPCCZ", 6))
   {
      CString val(this->ParseParameterValue(lpszParam));
      pApp->m_txpJob.SetDestinationFilename(val);
   }
   else if (!STRNICMP(lpszParam, "TXP", 3))  // DANGER! Make sure this test stays after longer params that start with TXP
   {
      pApp->m_txpJob.SetActivated();
   }
   else if (!STRNICMP(lpszParam, "REGISTERONLY", 5))
   {
      pApp->OnlyRegister = pApp->MostlyHideCamcad = TRUE;
   }
   else if (!STRNICMP(lpszParam, "{D959A3FC-B6D6-4c91-B1F9-B58A6DDA5D74}", 38))
   {
      pApp->MostlyHideCamcad = pApp->CopyToTestFixLocation = TRUE;
   }
   else if (!STRNICMP(lpszParam, "NoSplash",8))
   {
      this->m_bShowSplash = false;
   }
   else if (!STRNICMP(lpszParam, "VIEWMODE=", 9))
   {
      CString cmdMode = lpszParam;
      cmdMode = cmdMode.Mid(9);

      for (int i=0; i<cmdMode.GetLength(); i++)
      {
         if (!isdigit(cmdMode[i]))
            return;
      }

      if (cmdMode.IsEmpty())
         return;

      pApp->ViewMode = atoi(cmdMode);

      if (pApp->ViewMode < 0 || pApp->ViewMode > SW_MAX)
         pApp->ViewMode = SW_SHOWMAXIMIZED;
   }
   else if (!STRNICMP(lpszParam, "FILE:", 5))
   {
      CString filename = lpszParam;
      m_strFileName = filename.Mid(5);
      CFileFind findFile;

      if (!findFile.FindFile(m_strFileName))
      {
         CString buf;
         buf.Format("Cannot open file: %s [File not found]", m_strFileName);
         AfxMessageBox(buf);
         m_strFileName.Empty();
      }
   }
   else CCommandLineInfo::ParseParam(lpszParam, bFlag, bLast);*/
}

CProjectPath::CProjectPath()
{
	SetDefault();
}

CProjectPath::~CProjectPath()
{
}

//Keep
void CProjectPath::SetDefault()
{
	m_pathByFirstFile = "";
	m_pathByUserSelection = "";
	
	// 0 = Set project path by the directory of the first file loaded into CAMCAD
	// 1 = Set project path manually
	m_projectPathOption = 0;	
}
