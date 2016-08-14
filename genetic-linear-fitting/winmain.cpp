#include <windows.h>
#include <windowsx.h>

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>
#include <algorithm>
#include <random>
#include <string>
#include <list>
#include <array>
#include <limits>
#include <utility>

using namespace std::literals;

#include "resource.h"

template <typename T>
T pow2(T val)
{
    return val * val;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam);
void OnIdle(HWND hWnd);

LPCWSTR lpszAppName = L"genetic-linear-fitting";
constexpr int WIDTH = 1366, HEIGHT = 768;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASS wc;
    HWND hWnd;
    MSG msg;

    wc.style = CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = lpszAppName;
    RegisterClass(&wc);

    RECT rt = { 0, 0, WIDTH, HEIGHT };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&rt, style, FALSE);

    hWnd = CreateWindow(lpszAppName, lpszAppName, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, rt.right, rt.bottom,
        nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hWnd, nCmdShow);

    while (1)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            OnIdle(hWnd);
        }
    }

    return static_cast<int>(msg.wParam);
}

// window

BOOL OnCreate(HWND hWnd, LPCREATESTRUCT lpcs);
void OnPaint(HWND hWnd);
void OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
void OnContextMenu(HWND hWnd, HWND hWndContext, UINT xPos, UINT yPos);
void OnCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify);
void OnDestroy(HWND hWnd);

void Save(HWND hWnd);
void Load(HWND hWnd);

constexpr int POINT_RADIUS = 8;

HMENU hContextMenu;
HBITMAP hDbBuffer = nullptr;

HBRUSH hbrBack;
HBRUSH hbrPoint;

HPEN hpAxis;
HPEN hpPoint;
HPEN hpLine;

POINT prevClickPoint;
DWORD prevClickTick = 0;

// algorithm

union Chromosome
{
    struct
    {
        double dx, dy, x0, y0;
    } v;
    std::array<double, 4> ar;
};

POINT ClientToLogical(POINT client);
POINT LogicalToClient(POINT logical);

double ChromoFx(const Chromosome& chromo, double x);

void Start();
void NextGeneration();

constexpr int CHROMO_COUNT = 100;

std::mt19937 random_engine;

std::list<POINT> lstPoint;
std::list<POINT>::iterator selectedPoint;

bool bAlgoStarted = false;

std::array<Chromosome, CHROMO_COUNT> arChromo1, arChromo2;
std::array<Chromosome, CHROMO_COUNT>* parChromo;
std::array<Chromosome, CHROMO_COUNT>* parNewGen;
int nGeneration;

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    switch (iMsg)
    {
        HANDLE_MSG(hWnd, WM_CREATE, OnCreate);
        HANDLE_MSG(hWnd, WM_PAINT, OnPaint);
        HANDLE_MSG(hWnd, WM_LBUTTONDOWN, OnLButtonDown);
        HANDLE_MSG(hWnd, WM_CONTEXTMENU, OnContextMenu);
        HANDLE_MSG(hWnd, WM_COMMAND, OnCommand);
        HANDLE_MSG(hWnd, WM_DESTROY, OnDestroy);
    }
    return DefWindowProc(hWnd, iMsg, wParam, lParam);
}

BOOL OnCreate(HWND hWnd, LPCREATESTRUCT lpcs)
{
    hContextMenu = LoadMenu(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU));

    HDC hdc = GetDC(hWnd);
    hDbBuffer = CreateCompatibleBitmap(hdc, WIDTH, HEIGHT);
    ReleaseDC(hWnd, hdc);

    hbrBack = CreateSolidBrush(RGB(255, 255, 255));
    hbrPoint = CreateSolidBrush(RGB(0, 255, 255));

    hpAxis = CreatePen(PS_DOT, 1, RGB(0, 0, 0));
    hpPoint = CreatePen(PS_SOLID, 1, RGB(0, 65, 255));
    hpLine = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));

    selectedPoint = lstPoint.end();

    random_engine = std::mt19937 { GetTickCount() };
    parChromo = &arChromo1;
    parNewGen = &arChromo2;

    return TRUE;
}

void OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    BeginPaint(hWnd, &ps);

    if (hDbBuffer)
    {
        HDC hMemDC = CreateCompatibleDC(ps.hdc);
        HBITMAP hOldBit = (HBITMAP)SelectObject(hMemDC, hDbBuffer);

        RECT rt = { 0, 0, WIDTH, HEIGHT };
        FillRect(hMemDC, &rt, hbrBack);

        HPEN hOldPen = (HPEN)SelectObject(hMemDC, hpAxis);
        {
            POINT x_pt1 = LogicalToClient({ -WIDTH, 0 });
            POINT x_pt2 = LogicalToClient({ WIDTH, 0 });
            POINT y_pt1 = LogicalToClient({ 0, -HEIGHT });
            POINT y_pt2 = LogicalToClient({ 0, HEIGHT });
            MoveToEx(hMemDC, x_pt1.x, x_pt1.y, nullptr);
            LineTo(hMemDC, x_pt2.x, x_pt2.y);
            MoveToEx(hMemDC, y_pt1.x, y_pt1.y, nullptr);
            LineTo(hMemDC, y_pt2.x, y_pt2.y);
        }
        SelectObject(hMemDC, hOldPen);

        HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, hbrPoint);
        hOldPen = (HPEN)SelectObject(hMemDC, hpPoint);
        for (const auto& pt : lstPoint)
        {
            POINT ptClient = LogicalToClient(pt);
            Ellipse(hMemDC,
                ptClient.x - POINT_RADIUS, ptClient.y - POINT_RADIUS,
                ptClient.x + POINT_RADIUS, ptClient.y + POINT_RADIUS);
        }
        SelectObject(hMemDC, hOldPen);
        SelectObject(hMemDC, hOldBrush);

        if (bAlgoStarted)
        {
            auto str = L"Generation : "s + std::to_wstring(nGeneration);
            TextOut(hMemDC, 5, 5, str.c_str(), str.size());

            hOldPen = (HPEN)SelectObject(hMemDC, hpLine);
            for (const auto& chromo : *parChromo)
            {
                int x0 = ClientToLogical({ 0, 0 }).x;
                int x1 = ClientToLogical({ WIDTH, 0 }).x;
                int y0 = static_cast<int>(ChromoFx(chromo, x0));
                int y1 = static_cast<int>(ChromoFx(chromo, x1));

                POINT pt0 = LogicalToClient({ x0, y0 });
                POINT pt1 = LogicalToClient({ x1, y1 });

                MoveToEx(hMemDC, pt0.x, pt0.y, nullptr);
                LineTo(hMemDC, pt1.x, pt1.y);
            }
            SelectObject(hMemDC, hOldPen);
        }

        BitBlt(ps.hdc, 0, 0, WIDTH, HEIGHT, hMemDC, 0, 0, SRCCOPY);

        SelectObject(hMemDC, hOldBit);
        DeleteDC(hMemDC);
    }

    EndPaint(hWnd, &ps);
}

void OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    if (!bAlgoStarted)
    {
        POINT logical = ClientToLogical({ x, y });

        if (GetTickCount() - prevClickTick <= static_cast<int>(GetDoubleClickTime()))
        {
            if (std::abs(prevClickPoint.x - x) <= GetSystemMetrics(SM_CXDOUBLECLK) &&
                std::abs(prevClickPoint.y - y) <= GetSystemMetrics(SM_CYDOUBLECLK))
            {
                // double click for creation

                lstPoint.emplace_back(logical);
                InvalidateRect(hWnd, nullptr, FALSE);

                prevClickTick = 0;
            }
        }

        // single click

        auto sel = lstPoint.end();
        int sel_dist = (std::numeric_limits<int>::max)();

        for (auto it = lstPoint.begin(); it != lstPoint.end(); ++it)
        {
            const auto& pt = *it;

            int dist = pow2(pt.x - logical.x) + pow2(pt.y - logical.y);
            if (dist < sel_dist)
            {
                sel = it;
                sel_dist = dist;
            }
        }

        if (sel != lstPoint.end() && sel_dist <= pow2(POINT_RADIUS))
        {
            selectedPoint = sel;
        }
        else
        {
            selectedPoint = lstPoint.end();

            prevClickPoint = POINT { x, y };
            prevClickTick = GetTickCount();
        }
    }
}

void OnContextMenu(HWND hWnd, HWND hWndContext, UINT xPos, UINT yPos)
{
    if (xPos == -1)
    {
        xPos = yPos = 0;
    }

    HMENU hSub;
    if (!bAlgoStarted)
        hSub = GetSubMenu(hContextMenu, 0);
    else
        hSub = GetSubMenu(hContextMenu, 1);

    TrackPopupMenu(hSub, TPM_LEFTALIGN, xPos, yPos, 0, hWnd, nullptr);
}

void OnCommand(HWND hWnd, int id, HWND hWndCtl, UINT codeNotify)
{
    switch (id)
    {
        case ID_CONTEXT_START:
            if (lstPoint.empty() || std::next(lstPoint.begin()) == std::prev(lstPoint.end()))
            {
                MessageBox(hWnd, L"the number of data must be bigger than 2 at least", L"Error", MB_OK | MB_ICONERROR);
            }
            else
            {
                selectedPoint = lstPoint.end();
                prevClickTick = 0;
                bAlgoStarted = true;
                Start();
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            break;
        case ID_CONTEXT_STOP:
            bAlgoStarted = false;
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        case ID_CONTEXT_RESTART:
            Start();
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        case ID_CONTEXT_SAVE:
            Save(hWnd);
            break;
        case ID_CONTEXT_LOAD:
            Load(hWnd);
            break;
    }
}

void OnDestroy(HWND hWnd)
{
    DestroyMenu(hContextMenu);

    DeleteObject(hDbBuffer);
    DeleteObject(hbrBack);
    DeleteObject(hbrPoint);

    PostQuitMessage(0);
}

void Save(HWND hWnd)
{
    wchar_t strFile[MAX_PATH] = L"";

    OPENFILENAME ofn;
    std::memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"genetic-linear-fitting 데이터 파일 (*.txt)\0*.txt\0모든 파일 (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = strFile;
    ofn.nMaxFile = sizeof(strFile) / sizeof(strFile[0]);
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (GetSaveFileName(&ofn))
    {
        std::ofstream f { strFile };
        f.exceptions(std::ios::failbit | std::ios::badbit);
        for (const auto& pt : lstPoint)
        {
            f << pt.x << ' ' << pt.y << '\n';
        }
        f.close();
    }
}

void Load(HWND hWnd)
{
    wchar_t strFile[MAX_PATH] = L"";

    OPENFILENAME ofn;
    std::memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"genetic-linear-fitting 데이터 파일 (*.txt)\0*.txt\0모든 파일 (*.*)\0*.*\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = strFile;
    ofn.nMaxFile = sizeof(strFile) / sizeof(strFile[0]);
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileName(&ofn))
    {
        std::list<POINT> lstRead;

        std::ifstream f { strFile };
        f.exceptions(std::ios::failbit | std::ios::badbit);
        while (!f.eof())
        {
            POINT pt;
            f >> pt.x >> pt.y >> std::ws;
            lstRead.emplace_back(pt);
        }
        f.close();

        lstPoint = std::move(lstRead);
    }

    InvalidateRect(hWnd, nullptr, FALSE);
}

void OnIdle(HWND hWnd)
{
    if (!bAlgoStarted)
    {
        WaitMessage();
    }
    else
    {
        NextGeneration();
        InvalidateRect(hWnd, nullptr, FALSE);
    }
}

POINT ClientToLogical(POINT client)
{
    return POINT {
        client.x - WIDTH / 2,
        HEIGHT / 2 - client.y
    };
}

POINT LogicalToClient(POINT logical)
{
    return POINT {
        logical.x + WIDTH / 2,
        HEIGHT / 2 - logical.y
    };
}

double ChromoFx(const Chromosome& chromo, double x)
{
    return (chromo.v.dx / chromo.v.dy) * (x - chromo.v.x0) + chromo.v.y0;
}

void Start()
{
    nGeneration = 0;

    std::uniform_real_distribution<double> grad_dist { -100, 100 };
    std::uniform_real_distribution<double> x0_dist { -WIDTH / 2.0, WIDTH / 2.0 };
    std::uniform_real_distribution<double> y0_dist { -HEIGHT / 2.0, HEIGHT / 2.0 };
    for (auto& chromo : *parChromo)
    {
        chromo.v.dx = grad_dist(random_engine);
        chromo.v.dy = grad_dist(random_engine);
        chromo.v.x0 = x0_dist(random_engine);
        chromo.v.y0 = y0_dist(random_engine);
    }
}

void NextGeneration()
{
    static std::array<double, CHROMO_COUNT> arValue;

    // cost
    double cost_worst = 0;
    double cost_best = (std::numeric_limits<double>::max)();
    for (unsigned idx = 0; idx < CHROMO_COUNT; ++idx)
    {
        for (const auto& pt : lstPoint)
        {
            double cost = pow2(ChromoFx((*parChromo)[idx], pt.x) - pt.y);
            if (cost > cost_worst)
                cost_worst = cost;
            if (cost < cost_best)
                cost_best = cost;
            arValue[idx] = cost;
        }
    }

    // fitness
    double sumFitness = 0;
    for (unsigned idx = 0; idx < CHROMO_COUNT; ++idx)
    {
        double fitness = (cost_worst - arValue[idx]) + (cost_worst - cost_best) / (CHROMO_COUNT - 1);
        arValue[idx] = fitness;
        sumFitness += fitness;
    }

    // weight
    for (unsigned idx = 0; idx < CHROMO_COUNT; ++idx)
    {
        arValue[idx] /= sumFitness;
    }

    std::discrete_distribution<int> sel_dist { arValue.begin(), arValue.end() };
    std::uniform_int_distribution<unsigned> cross_dist { 0, 1 };

    std::uniform_real_distribution<double> cross_mix_dist { 0, 1 };
    const double CROSS_MIX_PROBABILITY = 0.1;

    std::uniform_real_distribution<double> mutation_occur_dist { 0, 1 };
    const double MUTATION_PROBABILITY = 0.03;

    for (unsigned idx = 0; idx < 100; ++idx)
    {
        unsigned idxFather = sel_dist(random_engine);
        unsigned idxMother;
        do
        {
            idxMother = sel_dist(random_engine);
        } while (idxMother == idxFather);

        const Chromosome* father = &(*parChromo)[idxFather];
        const Chromosome* mother = &(*parChromo)[idxMother];
        const std::array<const Chromosome*, 2> parents = { father, mother };

        std::array<std::array<double, 2>, 4> mutation_parameter = {
            50, 40,
            50, 40,
            125, 100,
            125, 100,
        };
        for (unsigned j = 0; j < 4; ++j)
        {
            double val;

            if (cross_mix_dist(random_engine) <= CROSS_MIX_PROBABILITY)
            {
                val = parents[cross_dist(random_engine)]->ar[j];
            }
            else
            {
                val = (father->ar[j] + mother->ar[j]) / 2;
            }

            if (mutation_occur_dist(random_engine) <= MUTATION_PROBABILITY)
            {
                const auto& param = mutation_parameter[j];
                double range = param[0] - param[1] * std::tanh((nGeneration - 5000.f) / 12000.0f);
                std::normal_distribution<double> range_dist { range, 10 };
                double range_noised = std::abs(range_dist(random_engine));
                std::uniform_real_distribution<double> mutation_val_dist { val - range_noised, val + range_noised };
                val = mutation_val_dist(random_engine);
            }

            (*parNewGen)[idx].ar[j] = val;
        }
    }

    std::swap(parChromo, parNewGen);
    nGeneration++;
}
