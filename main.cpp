// ================================================================
// A46 COMPUTER EMULATOR - Win32 GUI
// Custom 16-bit CPU - 64KB RAM - 30x40 Pixel Display - D-Pad Input
// ================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

#include "cpu.h"
#include "assembler.h"

// ================================================================
// PALETTE
// ================================================================
static COLORREF g_palette[256];

static void InitPalette() {
    g_palette[0]  = RGB(0,0,0);
    g_palette[1]  = RGB(255,255,255);
    g_palette[2]  = RGB(220,50,47);
    g_palette[3]  = RGB(100,200,80);
    g_palette[4]  = RGB(38,139,210);
    g_palette[5]  = RGB(255,220,50);
    g_palette[6]  = RGB(42,161,152);
    g_palette[7]  = RGB(211,54,130);
    g_palette[8]  = RGB(255,140,50);
    g_palette[9]  = RGB(130,100,210);
    g_palette[10] = RGB(88,88,88);
    g_palette[11] = RGB(170,170,170);
    g_palette[12] = RGB(140,50,40);
    g_palette[13] = RGB(40,110,55);
    g_palette[14] = RGB(30,70,140);
    g_palette[15] = RGB(190,180,60);
    int idx = 16;
    for (int r = 0; r < 6; r++)
        for (int g = 0; g < 6; g++)
            for (int b = 0; b < 6; b++)
                g_palette[idx++] = RGB(r?55+r*40:0, g?55+g*40:0, b?55+b*40:0);
    for (int i = 0; i < 24; i++) {
        int v = 8 + i * 10;
        g_palette[idx++] = RGB(v,v,v);
    }
}

// ================================================================
// THEME COLORS
// ================================================================
#define CLR_BG          RGB(22, 22, 28)
#define CLR_PANEL       RGB(28, 30, 36)
#define CLR_PANEL_EDGE  RGB(50, 55, 65)
#define CLR_TEXT         RGB(195, 200, 210)
#define CLR_TEXT_DIM     RGB(100, 105, 120)
#define CLR_ACCENT       RGB(80, 160, 255)
#define CLR_ACCENT2      RGB(130, 230, 160)
#define CLR_DISP_BORDER  RGB(60, 65, 80)
#define CLR_BTN          RGB(42, 46, 56)
#define CLR_BTN_HOV      RGB(55, 62, 78)
#define CLR_BTN_TEXT     RGB(200, 205, 218)
#define CLR_ERR          RGB(255, 90, 80)
#define CLR_VRAM         RGB(40, 50, 70)
#define CLR_IP_ROW       RGB(60, 55, 30)
#define CLR_EDIT_BG      RGB(18, 18, 24)
#define CLR_EDIT_TEXT    RGB(210, 215, 225)
#define CLR_DPAD_BG      RGB(38, 40, 50)
#define CLR_DPAD_PRESS   RGB(60, 80, 120)
#define CLR_DPAD_ARROW   RGB(160, 168, 185)
#define CLR_DPAD_ARR_PR  RGB(120, 200, 255)

// ================================================================
// GLOBALS
// ================================================================
static A46_CPU g_cpu;
static bool    g_running = false;
static int     g_speed   = 50000;
static int     g_memScroll = 0;
static char    g_statusMsg[512] = "Ready - write some A46 assembly and hit Assemble";

// D-pad state
static bool g_btnUp = false, g_btnDown = false;
static bool g_btnLeft = false, g_btnRight = false;

static HWND g_hMain;
static HWND g_hCodeEdit;
static HWND g_hBtnAssemble, g_hBtnRun, g_hBtnStep, g_hBtnReset, g_hBtnStop;
static HWND g_hMemPanel;
static HFONT g_hFontMono, g_hFontMonoSm, g_hFontUI;
static HBRUSH g_hBrBg, g_hBrPanel, g_hBrEdit;

// IDs
#define IDC_CODE_EDIT   2001
#define IDC_BTN_ASM     2010
#define IDC_BTN_RUN     2011
#define IDC_BTN_STEP    2012
#define IDC_BTN_RESET   2013
#define IDC_BTN_STOP    2014
#define IDC_MEM_PANEL   2020
#define IDT_RUN         3001
#define IDM_FILE_OPEN   4001
#define IDM_HELP_AIREF  4002

static const int PXS = 10;  // pixel scale

// Layout (computed in WM_SIZE)
static int g_dispX, g_dispY, g_dispW, g_dispH;
static int g_toolbarH = 44;
static int g_dpadCX, g_dpadCY;  // D-pad center
static const int DPAD_SZ = 30;  // button cell size

// D-pad hit rects
static RECT g_dpadUp, g_dpadDown, g_dpadLeft, g_dpadRight;

static void CalcDpadRects() {
    int cx = g_dpadCX, cy = g_dpadCY;
    int S = DPAD_SZ;
    // Cross arrangement: Up top-center, Down bottom-center, Left mid-left, Right mid-right
    g_dpadUp    = { cx - S/2, cy - S - S/2, cx + S/2, cy - S/2 };
    g_dpadDown  = { cx - S/2, cy + S/2,     cx + S/2, cy + S/2 + S };
    g_dpadLeft  = { cx - S - S/2, cy - S/2, cx - S/2, cy + S/2 };
    g_dpadRight = { cx + S/2, cy - S/2,     cx + S/2 + S, cy + S/2 };
}

static bool HitTest(RECT& rc, int x, int y) {
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}

// ================================================================
// DISPLAY RENDERING
// ================================================================
static void PaintDisplay(HDC hdc, int x, int y, int w, int h) {
    HPEN pen = CreatePen(PS_SOLID, 2, CLR_DISP_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x-2, y-2, x+w+2, y+h+2);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = A46_DISP_W;
    bmi.bmiHeader.biHeight = -A46_DISP_H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    uint32_t* pixels = nullptr;
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);

    for (int py = 0; py < A46_DISP_H; py++)
        for (int px = 0; px < A46_DISP_W; px++) {
            uint8_t ci = g_cpu.mem[A46_DISP_START + py * A46_DISP_W + px];
            COLORREF c = g_palette[ci];
            pixels[py * A46_DISP_W + px] = GetRValue(c)<<16 | GetGValue(c)<<8 | GetBValue(c);
        }

    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(hdc, x, y, w, h, memDC, 0, 0, A46_DISP_W, A46_DISP_H, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);

    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(40,42,50));
    HPEN oldGP = (HPEN)SelectObject(hdc, gridPen);
    int sx = w / A46_DISP_W, sy = h / A46_DISP_H;
    for (int i = 1; i < A46_DISP_W; i++) { MoveToEx(hdc, x+i*sx, y, NULL); LineTo(hdc, x+i*sx, y+h); }
    for (int i = 1; i < A46_DISP_H; i++) { MoveToEx(hdc, x, y+i*sy, NULL); LineTo(hdc, x+w, y+i*sy); }
    SelectObject(hdc, oldGP);
    DeleteObject(gridPen);
}

// ================================================================
// REGISTER PANEL (compact single-column)
// ================================================================
static void PaintRegisters(HDC hdc, int x, int y) {
    SetBkMode(hdc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(hdc, g_hFontMonoSm);
    char buf[256];
    int ly = y, lineH = 16;

    SetTextColor(hdc, CLR_ACCENT);
    TextOutA(hdc, x, ly, "REGISTERS", 9); ly += lineH + 2;

    SetTextColor(hdc, CLR_TEXT);
    for (int i = 0; i < REG_COUNT; i++) {
        sprintf(buf, "%s:%08X", RegNames[i], g_cpu.reg[i]);
        TextOutA(hdc, x, ly, buf, (int)strlen(buf));
        ly += lineH;
    }
    ly += 2;
    sprintf(buf, "IP:%08X SP:%08X", g_cpu.ip, g_cpu.sp);
    SetTextColor(hdc, CLR_ACCENT2);
    TextOutA(hdc, x, ly, buf, (int)strlen(buf)); ly += lineH;

    sprintf(buf, "Z:%d S:%d HLT:%d", g_cpu.flagZ?1:0, g_cpu.flagS?1:0, g_cpu.halted?1:0);
    SetTextColor(hdc, g_cpu.halted ? CLR_ERR : CLR_TEXT_DIM);
    TextOutA(hdc, x, ly, buf, (int)strlen(buf));

    SelectObject(hdc, old);
}

// ================================================================
// D-PAD RENDERING (PS-style cross)
// ================================================================
static void DrawArrowBtn(HDC hdc, RECT rc, int dir, bool pressed) {
    // Button background
    HBRUSH bg = CreateSolidBrush(pressed ? CLR_DPAD_PRESS : CLR_DPAD_BG);
    HPEN edge = CreatePen(PS_SOLID, 1, CLR_PANEL_EDGE);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, bg);
    HPEN oldPen = (HPEN)SelectObject(hdc, edge);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 5, 5);
    SelectObject(hdc, oldBr); SelectObject(hdc, oldPen);
    DeleteObject(bg); DeleteObject(edge);

    // Arrow triangle
    int cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
    int r = (rc.right - rc.left) / 4;
    POINT pts[3];
    switch (dir) {
    case 0: pts[0]={cx,cy-r}; pts[1]={cx-r,cy+r}; pts[2]={cx+r,cy+r}; break; // Up
    case 1: pts[0]={cx,cy+r}; pts[1]={cx-r,cy-r}; pts[2]={cx+r,cy-r}; break; // Down
    case 2: pts[0]={cx-r,cy}; pts[1]={cx+r,cy-r}; pts[2]={cx+r,cy+r}; break; // Left
    case 3: pts[0]={cx+r,cy}; pts[1]={cx-r,cy-r}; pts[2]={cx-r,cy+r}; break; // Right
    }
    COLORREF ac = pressed ? CLR_DPAD_ARR_PR : CLR_DPAD_ARROW;
    HBRUSH abr = CreateSolidBrush(ac);
    HPEN apen = CreatePen(PS_SOLID, 1, ac);
    SelectObject(hdc, abr); SelectObject(hdc, apen);
    Polygon(hdc, pts, 3);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    SelectObject(hdc, GetStockObject(NULL_PEN));
    DeleteObject(abr); DeleteObject(apen);
}

static void PaintDpad(HDC hdc) {
    // Label
    SetBkMode(hdc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(hdc, g_hFontMonoSm);
    SetTextColor(hdc, CLR_TEXT_DIM);
    const char* lbl = "D-PAD";
    int lblW = 5 * 7; // approx
    TextOutA(hdc, g_dpadCX - lblW/2, g_dpadCY - (DPAD_SZ * 3 / 2) - 16, lbl, 5);
    SelectObject(hdc, old);

    DrawArrowBtn(hdc, g_dpadUp,    0, g_btnUp);
    DrawArrowBtn(hdc, g_dpadDown,  1, g_btnDown);
    DrawArrowBtn(hdc, g_dpadLeft,  2, g_btnLeft);
    DrawArrowBtn(hdc, g_dpadRight, 3, g_btnRight);
}

// ================================================================
// MEMORY PANEL
// ================================================================
static int g_memVisibleRows = 30;

static void UpdateMemScrollbar(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    int h = rc.bottom;
    int rowH = 16;
    g_memVisibleRows = (h - 4) / rowH;
    int totalRows = A46_MEM_ROWS;
    if (g_memScroll > totalRows - g_memVisibleRows) g_memScroll = totalRows - g_memVisibleRows;
    if (g_memScroll < 0) g_memScroll = 0;
    SCROLLINFO si = {}; si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0; si.nMax = totalRows - 1;
    si.nPage = g_memVisibleRows; si.nPos = g_memScroll;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

static void PaintMemoryPanel(HWND hwnd, HDC hdc) {
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;
    HBRUSH br = CreateSolidBrush(CLR_PANEL);
    FillRect(hdc, &rc, br); DeleteObject(br);

    SetBkMode(hdc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(hdc, g_hFontMonoSm);
    int rowH = 16;
    int totalRows = A46_MEM_ROWS;
    if (g_memScroll > totalRows - g_memVisibleRows) g_memScroll = totalRows - g_memVisibleRows;
    if (g_memScroll < 0) g_memScroll = 0;

    SetTextColor(hdc, CLR_ACCENT);
    char hdr[128];
    sprintf(hdr, "ADDR     "); // Adjusted for 5-digit hex
    for (int i = 0; i < A46_MEM_ROW_BYTES; i++) { char t[8]; sprintf(t, "%02X ", i); strcat(hdr, t); }
    TextOutA(hdc, 6, 2, hdr, (int)strlen(hdr));

    int ipRow = g_cpu.ip / A46_MEM_ROW_BYTES;
    char buf[200];
    for (int vi = 0; vi < g_memVisibleRows; vi++) {
        int row = g_memScroll + vi;
        if (row >= totalRows) break;
        int addr = row * A46_MEM_ROW_BYTES;
        int y = 2 + rowH + vi * rowH;
        if (row == ipRow) {
            RECT rr = { 0, y, w, y + rowH };
            HBRUSH hb = CreateSolidBrush(CLR_IP_ROW); FillRect(hdc, &rr, hb); DeleteObject(hb);
        } else if (addr >= A46_DISP_START && addr <= (int)A46_DISP_END) {
            RECT rr = { 0, y, w, y + rowH };
            HBRUSH hb = CreateSolidBrush(CLR_VRAM); FillRect(hdc, &rr, hb); DeleteObject(hb);
        }
        sprintf(buf, "%05X:  ", addr);
        SetTextColor(hdc, CLR_TEXT_DIM);
        TextOutA(hdc, 6, y, buf, (int)strlen(buf));
        int xoff = 6 + 9 * 8;
        for (int b = 0; b < A46_MEM_ROW_BYTES; b++) {
            uint8_t val = g_cpu.mem[addr + b];
            sprintf(buf, "%02X", val);
            SetTextColor(hdc, val ? CLR_TEXT : CLR_TEXT_DIM);
            TextOutA(hdc, xoff + b * 24, y, buf, 2);
        }
    }
    SelectObject(hdc, old);
}

static LRESULT CALLBACK MemPanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                      UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdcS = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HDC hdc = CreateCompatibleDC(hdcS);
        HBITMAP bb = CreateCompatibleBitmap(hdcS, rc.right, rc.bottom);
        HBITMAP ob = (HBITMAP)SelectObject(hdc, bb);
        PaintMemoryPanel(hwnd, hdc);
        BitBlt(hdcS, 0, 0, rc.right, rc.bottom, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, ob); DeleteObject(bb); DeleteDC(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_VSCROLL: {
        int a = LOWORD(wp);
        switch (a) {
        case SB_LINEUP: g_memScroll--; break;
        case SB_LINEDOWN: g_memScroll++; break;
        case SB_PAGEUP: g_memScroll -= g_memVisibleRows; break;
        case SB_PAGEDOWN: g_memScroll += g_memVisibleRows; break;
        case SB_THUMBTRACK: 
        case SB_THUMBPOSITION: {
            g_memScroll = HIWORD(wp);
            break;
        }
        }
        if (g_memScroll > A46_MEM_ROWS - g_memVisibleRows) g_memScroll = A46_MEM_ROWS - g_memVisibleRows;
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateMemScrollbar(hwnd);
        UpdateWindow(hwnd);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        g_memScroll -= (GET_WHEEL_DELTA_WPARAM(wp) / 120) * 3;
        if (g_memScroll > A46_MEM_ROWS - g_memVisibleRows) g_memScroll = A46_MEM_ROWS - g_memVisibleRows;
        if (g_memScroll < 0) g_memScroll = 0;
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateMemScrollbar(hwnd);
        UpdateWindow(hwnd);
        return 0;
    }
    case WM_SIZE: {
        UpdateMemScrollbar(hwnd);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// ================================================================
// STATUS BAR
// ================================================================
static void PaintStatusBar(HDC hdc, int x, int y, int w, int h) {
    RECT rc = { x, y, x+w, y+h };
    HBRUSH br = CreateSolidBrush(RGB(18,18,22));
    FillRect(hdc, &rc, br); DeleteObject(br);
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_PANEL_EDGE);
    HPEN oldP = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x, y, NULL); LineTo(hdc, x+w, y);
    SelectObject(hdc, oldP); DeleteObject(pen);
    SetBkMode(hdc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(hdc, g_hFontMonoSm);
    SetTextColor(hdc, CLR_TEXT_DIM);
    TextOutA(hdc, x+10, y+6, g_statusMsg, (int)strlen(g_statusMsg));
    SelectObject(hdc, old);
}

// ================================================================
// TOOLBAR
// ================================================================
static void PaintToolbar(HDC hdc, int x, int y, int w, int h) {
    RECT rc = { x, y, x+w, y+h };
    HBRUSH br = CreateSolidBrush(RGB(25,26,32));
    FillRect(hdc, &rc, br); DeleteObject(br);
    HPEN pen = CreatePen(PS_SOLID, 1, CLR_PANEL_EDGE);
    HPEN oldP = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x, y+h-1, NULL); LineTo(hdc, x+w, y+h-1);
    SelectObject(hdc, oldP); DeleteObject(pen);
}

// ================================================================
// DEFAULT CODE
// ================================================================
static const char* DEFAULT_CODE =
    "; ================================\r\n"
    "; A46 DEMO: D-Pad Controlled Pixel\r\n"
    "; Use the D-Pad buttons to move!\r\n"
    "; ================================\r\n"
    "; B=X, C=Y\r\n"
    "\r\n"
    "    MOV B, 15       ; start X\r\n"
    "    MOV C, 20       ; start Y\r\n"
    "    MOV D, 2\r\n"
    "    STORE [0x102], D ; color\r\n"
    "\r\n"
    "loop:\r\n"
    "    ; Erase old pixel\r\n"
    "    CALL calc_addr\r\n"
    "    MOV D, 0\r\n"
    "    STORE [A], D\r\n"
    "\r\n"
    "    ; Read D-pad input\r\n"
    "    LOAD D, [0x58EB0] ; Up (32-bit addr)\r\n"
    "    CMP D, 0\r\n"
    "    JNZ go_up\r\n"
    "    LOAD D, [0x58EB1] ; Down\r\n"
    "    CMP D, 0\r\n"
    "    JNZ go_down\r\n"
    "    LOAD D, [0x58EB2] ; Left\r\n"
    "    CMP D, 0\r\n"
    "    JNZ go_left\r\n"
    "    LOAD D, [0x58EB3] ; Right\r\n"
    "    CMP D, 0\r\n"
    "    JNZ go_right\r\n"
    "    JMP draw\r\n"
    "\r\n"
    "go_up:\r\n"
    "    CMP C, 0\r\n"
    "    JLE draw\r\n"
    "    DEC C\r\n"
    "    JMP draw\r\n"
    "go_down:\r\n"
    "    CMP C, 39\r\n"
    "    JGE draw\r\n"
    "    INC C\r\n"
    "    JMP draw\r\n"
    "go_left:\r\n"
    "    CMP B, 0\r\n"
    "    JLE draw\r\n"
    "    DEC B\r\n"
    "    JMP draw\r\n"
    "go_right:\r\n"
    "    CMP B, 29\r\n"
    "    JGE draw\r\n"
    "    INC B\r\n"
    "\r\n"
    "draw:\r\n"
    "    CALL calc_addr\r\n"
    "    LOAD D, [0x102]\r\n"
    "    STORE [A], D\r\n"
    "    \r\n"
    "    ; Wait for next frame (VSync)\r\n"
    "    MOV D, 1\r\n"
    "    STORE [0x58EB4], D\r\n"
    "    JMP loop\r\n"
    "\r\n"
    "; --- VRAM addr ---\r\n"
    "calc_addr:\r\n"
    "    MOV A, 0x58A00  ; VRAM start\r\n"
    "    PUSH B\r\n"
    "    PUSH C\r\n"
    "ca_m:\r\n"
    "    CMP C, 0\r\n"
    "    JZ ca_a\r\n"
    "    ADD A, 30\r\n"
    "    DEC C\r\n"
    "    JMP ca_m\r\n"
    "ca_a:\r\n"
    "    ADD A, B\r\n"
    "    POP C\r\n"
    "    POP B\r\n"
    "    RET\r\n";

// ================================================================
// ACTIONS
// ================================================================
static void DoAssemble() {
    int len = GetWindowTextLengthA(g_hCodeEdit);
    std::string code(len + 1, '\0');
    GetWindowTextA(g_hCodeEdit, &code[0], len + 1);
    code.resize(len);
    g_cpu.reset(); g_running = false;
    KillTimer(g_hMain, IDT_RUN);
    AsmResult res = assemble(code, g_cpu);
    if (res.success) {
        sprintf(g_statusMsg, "Assembled OK - program loaded into memory");
    } else {
        char errBuf[1024];
        if (res.errorLine > 0) {
            sprintf(g_statusMsg, "ERROR line %d: %s", res.errorLine, res.error.c_str());
            sprintf(errBuf, "Assembly Error on line %d:\n\n%s\n\n(You can press Ctrl+C to copy this message)", res.errorLine, res.error.c_str());
        } else {
            sprintf(g_statusMsg, "ERROR: %s", res.error.c_str());
            sprintf(errBuf, "Assembly Error:\n\n%s\n\n(You can press Ctrl+C to copy this message)", res.error.c_str());
        }
        MessageBoxA(g_hMain, errBuf, "Assembly Error", MB_OK | MB_ICONERROR);
    }
    InvalidateRect(g_hMain, NULL, FALSE);
    InvalidateRect(g_hMemPanel, NULL, FALSE);
}

static void DoStep() {
    if (g_cpu.halted) { sprintf(g_statusMsg, "CPU halted - press Reset"); }
    else {
        g_cpu.step();
        if (g_cpu.halted) sprintf(g_statusMsg, "Halted at IP=%04X", g_cpu.ip);
        else sprintf(g_statusMsg, "Step - IP=%04X A=%04X B=%04X C=%04X D=%04X",
                     g_cpu.ip, g_cpu.reg[0], g_cpu.reg[1], g_cpu.reg[2], g_cpu.reg[3]);
    }
    InvalidateRect(g_hMain, NULL, FALSE);
    InvalidateRect(g_hMemPanel, NULL, FALSE);
}

static void DoRun() {
    if (g_cpu.halted) { sprintf(g_statusMsg, "CPU halted - press Reset first"); InvalidateRect(g_hMain, NULL, FALSE); return; }
    g_running = true;
    SetTimer(g_hMain, IDT_RUN, 16, NULL);
    sprintf(g_statusMsg, "Running...");
    InvalidateRect(g_hMain, NULL, FALSE);
}

static void DoStop() {
    g_running = false; KillTimer(g_hMain, IDT_RUN);
    sprintf(g_statusMsg, "Stopped at IP=%04X", g_cpu.ip);
    InvalidateRect(g_hMain, NULL, FALSE);
}

static void DoReset() {
    g_running = false; KillTimer(g_hMain, IDT_RUN);
    g_cpu.reset();
    sprintf(g_statusMsg, "Reset - memory cleared");
    InvalidateRect(g_hMain, NULL, FALSE);
    InvalidateRect(g_hMemPanel, NULL, FALSE);
}

// Write D-pad state into CPU memory
static void SyncInputToMemory() {
    g_cpu.mem[A46_INPUT_UP]    = g_btnUp    ? 1 : 0;
    g_cpu.mem[A46_INPUT_DOWN]  = g_btnDown  ? 1 : 0;
    g_cpu.mem[A46_INPUT_LEFT]  = g_btnLeft  ? 1 : 0;
    g_cpu.mem[A46_INPUT_RIGHT] = g_btnRight ? 1 : 0;
}

// ================================================================
// FILE > OPEN
// ================================================================
static void DoFileOpen() {
    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMain;
    ofn.lpstrFilter = "A46 Assembly Files\0*.asm;*.a46;*.txt\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        std::ifstream f(path);
        if (f.is_open()) {
            std::string content((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
            SetWindowTextA(g_hCodeEdit, content.c_str());
            sprintf(g_statusMsg, "Loaded: %s", path);
            InvalidateRect(g_hMain, NULL, FALSE);
        }
    }
}

// ================================================================
// HELP > AI CODING REFERENCE
// ================================================================
static void DoAIReference() {
    // Build reference path next to exe
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    strcat(exePath, "A46_AI_Reference.txt");

    // Open the file
    ShellExecuteA(NULL, "open", exePath, NULL, NULL, SW_SHOWNORMAL);
    sprintf(g_statusMsg, "Opened AI Reference - copy and paste into any AI chat");
    InvalidateRect(g_hMain, NULL, FALSE);
}

// ================================================================
// WINDOW PROCEDURE
// ================================================================
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hMain = hwnd;
        g_cpu.reset();

        // Menu bar
        HMENU hMenu = CreateMenu();
        HMENU hFile = CreatePopupMenu();
        AppendMenuA(hFile, MF_STRING, IDM_FILE_OPEN, "Open ASM File...");
        AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hFile, "File");
        HMENU hHelp = CreatePopupMenu();
        AppendMenuA(hHelp, MF_STRING, IDM_HELP_AIREF, "Open AI Coding Reference");
        AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hHelp, "Help");
        SetMenu(hwnd, hMenu);

        // Fonts
        g_hFontMono   = CreateFontA(16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, "Consolas");
        g_hFontMonoSm = CreateFontA(14, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, "Consolas");
        g_hFontUI     = CreateFontA(16, 0, 0, 0, FW_BOLD, 0, 0, 0,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");

        g_hBrBg    = CreateSolidBrush(CLR_BG);
        g_hBrPanel = CreateSolidBrush(CLR_PANEL);
        g_hBrEdit  = CreateSolidBrush(CLR_EDIT_BG);

        // Toolbar buttons
        struct BtnInfo { HWND* h; int id; const char* text; };
        BtnInfo btns[] = {
            { &g_hBtnAssemble, IDC_BTN_ASM,   "Assemble" },
            { &g_hBtnRun,      IDC_BTN_RUN,   "Run" },
            { &g_hBtnStep,     IDC_BTN_STEP,  "Step" },
            { &g_hBtnReset,    IDC_BTN_RESET, "Reset" },
            { &g_hBtnStop,     IDC_BTN_STOP,  "Stop" },
        };
        int bx = 14;
        for (auto& b : btns) {
            *b.h = CreateWindowA("BUTTON", b.text,
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                bx, 8, 80, 28, hwnd, (HMENU)(INT_PTR)b.id, NULL, NULL);
            bx += 88;
        }

        // Code editor
        g_hCodeEdit = CreateWindowExA(0, "EDIT", DEFAULT_CODE,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            0, 0, 100, 100, hwnd, (HMENU)IDC_CODE_EDIT, NULL, NULL);
        SendMessageA(g_hCodeEdit, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
        SendMessageA(g_hCodeEdit, EM_SETTABSTOPS, 1, (LPARAM)(int[]){16});

        // Memory panel (Custom generic class for standard scrolling)
        g_hMemPanel = CreateWindowExA(0, "A46_MEM_WRAP", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL,
            0, 0, 100, 100, hwnd, (HMENU)IDC_MEM_PANEL, NULL, NULL);
        SetWindowSubclass(g_hMemPanel, MemPanelProc, 0, 0);

        return 0;
    }

    case WM_SIZE: {
        int cw = LOWORD(lp), ch = HIWORD(lp);
        int statusH = 28;
        int leftW = A46_DISP_W * PXS + 40;   // 340

        // Display
        g_dispX = 20;
        g_dispY = g_toolbarH + 10;
        g_dispW = A46_DISP_W * PXS;
        g_dispH = A46_DISP_H * PXS;

        // D-pad below display, centered (with extra gap for its label)
        int dpadArea = DPAD_SZ * 3;
        g_dpadCX = g_dispX + g_dispW / 2;
        g_dpadCY = g_dispY + g_dispH + 30 + dpadArea / 2;
        CalcDpadRects();

        // Registers next to D-pad (to the right)
        // (they get painted at fixed position in WM_PAINT)

        // Code editor below D-pad + registers
        int regEnd = g_dpadCY + dpadArea / 2 + 130;  // bottom of registers
        int editY = regEnd + 24;                     // plenty of space for "ASSEMBLY" label
        int editH = ch - editY - statusH - 6;
        if (editH < 60) editH = 60;
        MoveWindow(g_hCodeEdit, 10, editY, leftW - 20, editH, TRUE);

        // Memory panel
        MoveWindow(g_hMemPanel, leftW, g_toolbarH, cw - leftW, ch - g_toolbarH - statusH, TRUE);

        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdcS = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int cw = rc.right, ch = rc.bottom;

        HDC hdc = CreateCompatibleDC(hdcS);
        HBITMAP bb = CreateCompatibleBitmap(hdcS, cw, ch);
        HBITMAP ob = (HBITMAP)SelectObject(hdc, bb);

        FillRect(hdc, &rc, g_hBrBg);
        PaintToolbar(hdc, 0, 0, cw, g_toolbarH);
        PaintDisplay(hdc, g_dispX, g_dispY, g_dispW, g_dispH);
        PaintDpad(hdc);

        // Registers below D-pad
        int regY = g_dpadCY + DPAD_SZ * 3 / 2 + 10;
        PaintRegisters(hdc, 20, regY);

        // "ASSEMBLY" label
        RECT erc; GetWindowRect(g_hCodeEdit, &erc);
        POINT ep = { erc.left, erc.top }; ScreenToClient(hwnd, &ep);
        SetBkMode(hdc, TRANSPARENT);
        HFONT old = (HFONT)SelectObject(hdc, g_hFontMonoSm);
        SetTextColor(hdc, CLR_ACCENT);
        TextOutA(hdc, 12, ep.y - 16, "ASSEMBLY", 8);
        SelectObject(hdc, old);

        PaintStatusBar(hdc, 0, ch - 28, cw, 28);

        BitBlt(hdcS, 0, 0, cw, ch, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, ob); DeleteObject(bb); DeleteDC(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lp;
        HDC hdc = di->hDC;
        RECT rc = di->rcItem;
        bool hover = (di->itemState & ODS_SELECTED) || (di->itemState & ODS_FOCUS);
        HBRUSH br = CreateSolidBrush(hover ? CLR_BTN_HOV : CLR_BTN);
        FillRect(hdc, &rc, br); DeleteObject(br);
        HPEN pen = CreatePen(PS_SOLID, 1, CLR_PANEL_EDGE);
        HPEN oldP = (HPEN)SelectObject(hdc, pen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
        SelectObject(hdc, oldP); DeleteObject(pen);
        char text[64]; GetWindowTextA(di->hwndItem, text, 64);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_BTN_TEXT);
        HFONT old = (HFONT)SelectObject(hdc, g_hFontMonoSm);
        DrawTextA(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
        return TRUE;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdcE = (HDC)wp;
        SetTextColor(hdcE, CLR_EDIT_TEXT);
        SetBkColor(hdcE, CLR_EDIT_BG);
        return (LRESULT)g_hBrEdit;
    }

    // D-pad mouse input
    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        bool changed = false;
        if (HitTest(g_dpadUp, mx, my))    { g_btnUp = true; changed = true; }
        if (HitTest(g_dpadDown, mx, my))  { g_btnDown = true; changed = true; }
        if (HitTest(g_dpadLeft, mx, my))  { g_btnLeft = true; changed = true; }
        if (HitTest(g_dpadRight, mx, my)) { g_btnRight = true; changed = true; }
        if (changed) { SyncInputToMemory(); SetCapture(hwnd); InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    }
    case WM_LBUTTONUP: {
        bool was = g_btnUp || g_btnDown || g_btnLeft || g_btnRight;
        g_btnUp = g_btnDown = g_btnLeft = g_btnRight = false;
        SyncInputToMemory();
        if (was) { ReleaseCapture(); InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {
        case IDC_BTN_ASM:   DoAssemble(); break;
        case IDC_BTN_RUN:   DoRun();      break;
        case IDC_BTN_STEP:  DoStep();     break;
        case IDC_BTN_RESET: DoReset();    break;
        case IDC_BTN_STOP:  DoStop();     break;
        case IDM_FILE_OPEN: DoFileOpen(); break;
        case IDM_HELP_AIREF: DoAIReference(); break;
        }
        return 0;
    }

    case WM_TIMER: {
        if (wp == IDT_RUN && g_running) {
            SyncInputToMemory();
            g_cpu.yielded = false;
            for (int i = 0; i < g_speed; i++) {
                if (!g_cpu.step()) {
                    if (g_cpu.halted) {
                        g_running = false;
                        KillTimer(hwnd, IDT_RUN);
                        sprintf(g_statusMsg, "Halted at IP=%04X", g_cpu.ip);
                    }
                    break;
                }
                if (g_cpu.yielded) {
                    break; // Wait for next frame tick
                }
            }
            InvalidateRect(hwnd, NULL, FALSE);
            InvalidateRect(g_hMemPanel, NULL, FALSE);
        }
        return 0;
    }

    case WM_ERASEBKGND: return 1;

    case WM_DESTROY:
        DeleteObject(g_hFontMono);
        DeleteObject(g_hFontMonoSm);
        DeleteObject(g_hFontUI);
        DeleteObject(g_hBrBg);
        DeleteObject(g_hBrPanel);
        DeleteObject(g_hBrEdit);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

// ================================================================
// ENTRY POINT
// ================================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    InitPalette();
    InitCommonControls();

    WNDCLASSEXA wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "A46_EMU_CLASS";
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExA(&wc);

    WNDCLASSEXA mwc = {};
    mwc.cbSize        = sizeof(mwc);
    mwc.lpfnWndProc   = DefWindowProcA;
    mwc.hInstance      = hInst;
    mwc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    mwc.lpszClassName = "A46_MEM_WRAP";
    RegisterClassExA(&mwc);

    int winW = 1100, winH = 900;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;

    HWND hwnd = CreateWindowExA(
        0, "A46_EMU_CLASS", "A46 Computer Emulator",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        sx, sy, winW, winH,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        // Route mouse wheel to window under cursor for hover-scrolling
        if (msg.message == WM_MOUSEWHEEL) {
            POINT pt; pt.x = GET_X_LPARAM(msg.lParam); pt.y = GET_Y_LPARAM(msg.lParam);
            HWND hHover = WindowFromPoint(pt);
            if (hHover == g_hMemPanel) {
                SendMessageA(g_hMemPanel, WM_MOUSEWHEEL, msg.wParam, msg.lParam);
                continue;
            }
        }
        
        // Ctrl+A for code editor
        if (msg.message == WM_KEYDOWN && msg.hwnd == g_hCodeEdit) {
            if ((GetKeyState(VK_CONTROL) & 0x8000) && msg.wParam == 'A') {
                SendMessageA(g_hCodeEdit, EM_SETSEL, 0, -1);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
