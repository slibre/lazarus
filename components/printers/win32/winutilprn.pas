{
  Author: Olivier Guilbaud

 *****************************************************************************
 *                                                                           *
 *  This file is part of the Lazarus Component Library (LCL)                 *
 *                                                                           *
 *  See the file COPYING.modifiedLGPL.txt, included in this distribution,    *
 *  for details about the copyright.                                         *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                           *
 *****************************************************************************

  Abstract:
    This unit provide an access at Printers spool and other functions for manage
    the printers on Win32

  Documentations
    - Wine project
    - Microsoft MSDN Web
}

unit WinUtilPrn;

{$mode objfpc}{$H+}

interface

{$IFNDEF MSWindows}
{$FATAL This unit is reserved to Win32/Win64}
{$ENDIF}

uses
  Windows, Types, Classes, SysUtils, LCLType, Printers;

const
  {$i winutilprnconst.inc}

  LibWinSpool = 'winspool.drv';

const
  Win32Orientations: array [TPrinterOrientation] of SHORT = (
    DMORIENT_PORTRAIT, DMORIENT_LANDSCAPE, DMORIENT_PORTRAIT, DMORIENT_LANDSCAPE);

type
  TFcntHook = function(Wnd: HWND; uiMsg: UINT; wParam: WPARAM; lParam: LPARAM): UINT_PTR; stdcall;

// todo: remove when fpc will be released with them
{$ifndef win64}
  {$packrecords 1}
{$endif}

  tagPSD = record
    lStructSize: DWORD;
    hWndOwner: HWND;
    hDevMode: HGLOBAL;
    hDevNames: HGLOBAL;
    Flags: DWORD;
    ptPaperSize: TPOINT;
    rtMinMargin: TRECT;
    rtMargin: TRECT;
    hInstance: HINST;
    lCustData: LPARAM;
    lpfnPageSetupHook: TFcntHook;
    lpfnPagePaintHook: TFcntHook;
    lpPageSetupTemplateName: PChar;
    hPageSetupTemplate: HGLOBAL;
  end;
  PtagPSD = ^tagPSD;

  tagPD = record
    lStructSize: DWORD;
    hWndOwner: HWND;
    hDevMode: HGLOBAL;
    hDevNames: HGLOBAL;
    hDC: HDC;
    Flags: DWORD;
    nFromPage: word;
    nToPage: word;
    nMinPage: word;
    nMaxPage: word;
    nCopies: word;
    hInstance: HINST;
    lCustData: LPARAM;
    lpfnPrintHook: TFcntHook;
    lpfnSetupHook: TFcntHook;
    lpPrintTemplateName: PChar;
    lpSetupTemplateName: PChar;
    hPrintTemplate: HGLOBAL;
    hSetupTemplate: HGLOBAL;
  end;
  PtagPD = ^tagPD;

  {$IFDEF USEUNICODE}
  LPTSTR = PWidechar;
  {$ELSE}
  LPTSTR = PChar;
  {$ENDIF}

  _PRINTER_DEFAULTSA = record
       pDatatype : LPSTR;
       pDevMode : LPDEVMODE;
       DesiredAccess : ACCESS_MASK;
    end;
  PRINTER_DEFAULTSA = _PRINTER_DEFAULTSA;
  PPRINTER_DEFAULTSA = ^_PRINTER_DEFAULTSA;
  LPPRINTER_DEFAULTSA = ^_PRINTER_DEFAULTSA;

  _PRINTER_DEFAULTSW = record
    pDatatype: pwidechar;
    pDevMode: LPDEVMODEW;
    DesiredAccess: ACCESS_MASK;
  end;
  LPPRINTER_DEFAULTSW = ^_PRINTER_DEFAULTSW;

  _PRINTER_INFO_2A = record
       pServerName : LPTSTR;
       pPrinterName : LPTSTR;
       pShareName : LPTSTR;
       pPortName : LPTSTR;
       pDriverName : LPTSTR;
       pComment : LPTSTR;
       pLocation : LPTSTR;
       {$IFDEF USEUNICODE}
       pDevMode : LPDEVMODEW;
       {$ELSE}
       pDevMode : LPDEVMODE;
       {$ENDIF}
       pSepFile : LPTSTR;
       pPrintProcessor : LPTSTR;
       pDatatype : LPTSTR;
       pParameters : LPTSTR;
       pSecurityDescriptor : PSECURITY_DESCRIPTOR;
       Attributes : DWORD;
       Priority : DWORD;
       DefaultPriority : DWORD;
       StartTime : DWORD;
       UntilTime : DWORD;
       Status : DWORD;
       cJobs : DWORD;
       AveragePPM : DWORD;
    end;
  PRINTER_INFO_2A = _PRINTER_INFO_2A;
  PPRINTER_INFO_2A = ^_PRINTER_INFO_2A;
  LPPRINTER_INFO_2A = ^_PRINTER_INFO_2A;

  PRINTER_INFO_2 = PRINTER_INFO_2A;
  PPRINTER_INFO_2 = ^PRINTER_INFO_2;
  LPPRINTER_INFO_2 = ^PRINTER_INFO_2;

  _PRINTER_INFO_4A = record
       pPrinterName : LPSTR;
       pServerName : LPSTR;
       Attributes : DWORD;
    end;
  PRINTER_INFO_4A = _PRINTER_INFO_4A;
  PPRINTER_INFO_4A = ^_PRINTER_INFO_4A;
  LPPRINTER_INFO_4A = ^_PRINTER_INFO_4A;

  PRINTER_INFO_4 = PRINTER_INFO_4A;
  PPRINTER_INFO_4 = ^PRINTER_INFO_4;
  LPPRINTER_INFO_4 = ^PRINTER_INFO_4;

type
  { TPrinterDevice }

  TPrinterDevice = class
  public
    Name: string;
    Driver: string;
    Device: string;
    Port: string;

    DefaultPaper: Short;
    {$IFDEF USEUNICODE}
    DevMode: PDeviceModeW;
    {$ELSE}
    DevMode: PDeviceMode;
    {$ENDIF}
    DevModeSize: integer;
    destructor Destroy; override;
  end;

function DeviceCapabilities(pDevice, pPort: PChar; fwCapability: word;
  pOutput: PChar; DevMode: PDeviceMode): integer; stdcall; external LibWinSpool Name 'DeviceCapabilitiesA';
function DeviceCapabilitiesW(pDevice, pPort: PWideChar; fwCapability: word;
  pOutput: PWideChar; DevMode: PDeviceModeW): integer; stdcall; external LibWinSpool Name 'DeviceCapabilitiesW';

function GetProfileString(lpAppName: PChar; lpKeyName: PChar; lpDefault: PChar;
  lpReturnedString: PChar; nSize: DWORD): DWORD; stdcall; external 'kernel32' Name 'GetProfileStringA';

function PrintDlg(lppd: PtagPD): BOOL; stdcall; external 'comdlg32.dll' Name 'PrintDlgA';
function PrintDlgW(lppd: PTagPD): BOOL; stdcall; external 'comdlg32.dll' name 'PrintDlgW';
function PageSetupDlg(lppd: PtagPSD): BOOL; stdcall; external 'comdlg32.dll' Name 'PageSetupDlgA';
function PageSetupDlgW(lppd: PtagPSD): BOOL; stdcall; external 'comdlg32.dll' Name 'PageSetupDlgW';
function CommDlgExtendedError: DWORD; stdcall; external 'comdlg32.dll' Name 'CommDlgExtendedError';

function CreateIC(lpszDriver, lpszDevice, lpszOutput: PChar; lpdvmInit: PDeviceMode): HDC; stdcall; external 'gdi32.dll' Name 'CreateICA';
function CreateICW(lpszDriver, lpszDevice, lpszOutput: pwidechar; lpdvmInit: PDeviceModeW): HDC; stdcall; external 'gdi32.dll' Name 'CreateICW';
function CreateDC(lpszDriver, lpszDevice, lpszOutput: PChar; lpdvmInit: PDeviceMode): HDC; stdcall; external 'gdi32.dll' Name 'CreateDCA';
function CreateDCW(lpszDriver, lpszDevice, lpszOutput: pwidechar; lpdvmInit: PDeviceModeW): HDC; stdcall; external 'gdi32.dll' Name 'CreateDCW';

function DeleteDC(DC: HDC): BOOL; stdcall; external 'gdi32.dll' Name 'DeleteDC';
function StartDoc(DC: HDC; Inf: PDocInfo): integer; stdcall; external 'gdi32.dll' Name 'StartDocA';
function EndDoc(DC: HDC): integer; stdcall; external 'gdi32.dll' Name 'EndDoc';
function StartPage(DC: HDC): integer; stdcall; external 'gdi32.dll' Name 'StartPage';
function EndPage(DC: HDC): integer; stdcall; external 'gdi32.dll' Name 'EndPage';
function AbortDoc(DC: HDC): integer; stdcall; external 'gdi32.dll' Name 'AbortDoc';
function GlobalFree(HMem: HGlobal): HGlobal; stdcall; external 'kernel32.dll' Name 'GlobalFree';

// todo: remove when WinSpool.pp will be released with fpc
function OpenPrinter(_para1:LPSTR; _para2:PHANDLE; _para3:LPPRINTER_DEFAULTSA):BOOL;stdcall; external LibWinSpool name 'OpenPrinterA';
function OpenPrinterW(_para1:pwidechar; _para2:PHANDLE; _para3:LPPRINTER_DEFAULTSW):BOOL;stdcall; external LibWinSpool name 'OpenPrinterW';
function ClosePrinter(_para1:HANDLE):BOOL;stdcall; external LibWinSpool Name 'ClosePrinter';
function DocumentProperties(_para1:HWND; _para2:HANDLE; _para3:LPSTR; _para4:PDEVMODE; _para5:PDEVMODE; _para6:DWORD):LONG;stdcall; external LibWinSpool name 'DocumentPropertiesA';
function DocumentPropertiesW(_para1:HWND; _para2:HANDLE; _para3:pwidechar; _para4:PDEVMODEW; _para5:PDEVMODEW; _para6:DWORD):LONG;stdcall; external LibWinSpool name 'DocumentPropertiesW';
function EnumPrinters(_para1:DWORD; _para2:LPSTR; _para3:DWORD; _para4:PBYTE; _para5:DWORD; _para6:PDWORD; _para7:PDWORD):BOOL;stdcall; external LibWinSpool name 'EnumPrintersA';
function EnumPrintersW(_para1:DWORD; _para2:Pwidechar; _para3:DWORD; _para4:PBYTE; _para5:DWORD; _para6:PDWORD; _para7:PDWORD):BOOL;stdcall; external LibWinSpool name 'EnumPrintersW';
function GetPrinter(_para1:HANDLE; _para2:DWORD; _para3:PBYTE; _para4:DWORD; _para5:PDWORD):BOOL;stdcall; external LibWinSpool name 'GetPrinterA';
function StartDocPrinter(hPrinter: THANDLE; Level: DWORD; DocInfo: PByte): DWORD; stdcall; external LibWinSpool Name 'StartDocPrinterA';
function StartPagePrinter(_para1:HANDLE):BOOL;stdcall; external LibWinSpool name 'StartPagePrinter';
function EndDocPrinter(hprinter: THANDLE): BOOL; stdcall; external LibWinSpool Name 'EndDocPrinter';
function EndPagePrinter(hprinter: THANDLE): BOOL; stdcall; external LibWinSpool Name 'EndPagePrinter';
function AbortPrinter(hPrinter: THANDLE): BOOL; stdcall; external LibWinSpool Name 'AbortPrinter';
function WritePrinter(hPrinter: THANDLE; Buffer: Pointer; Count: DWord; Written: PDWORD): BOOL; stdcall; external LibWinSpool Name 'WritePrinter';

implementation

{ TPrinterDevice }

destructor TPrinterDevice.Destroy;
begin
  ReallocMem(DevMode, 0);
  inherited Destroy;
end;

end.

