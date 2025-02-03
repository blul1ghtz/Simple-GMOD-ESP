#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <vector>
#include <tchar.h>

struct Player {
    DWORD Address;
    float x; // 0x28
    float y; // 0x2C
    float z; // 0x30
};

typedef struct
{
    float x, y, z, w;
} Vec4;

typedef struct
{
    float x, y, z;
} Vec3;

typedef struct
{
    float x, y;
} Vec2;

#define PI 3.1415927f

HWND Window;
HANDLE HProcess;
DWORD PID;
HDC GameDC;
float ViewMatrix[16];
std::vector<Player> PlayerList;
int PlayerCount;
DWORD ServerBaseAddress;
DWORD EngineBaseAddress;

DWORD EntityListAddress = 0xA49AEC;    // server.dll + value
DWORD ViewMatrixAddress = 0x63FDF0;    // engine.dll + value
DWORD EntityCountAddress = 0x67822C;   // engine.dll + value
DWORD HealthOffset = 0xE0;
DWORD XPositionOffset = 0x28;
DWORD YPositionOffset = 0x2C;
DWORD ZPositionOffset = 0x30;
constexpr int SCREEN_WIDTH = 664;
constexpr int SCREEN_HEIGHT = 1176;
HBRUSH Color = CreateSolidBrush(RGB(0, 0, 255));
TCHAR ProcessName1[] = _T("server.dll");
TCHAR ProcessName2[] = _T("engine.dll");
char WindowName[] = "Garry's Mod";
bool ExtraSense = false;

// GETS A MODULE NAMES BASE ADDRESS FROM A PROCESS ID
DWORD GetModuleBaseAddress(TCHAR* ModuleName, DWORD PID) {
    // MAKES SNAPSHOT OF ALL MODULES WITHIN A PROCESS TO FIND THE ONE SPECIFIED
    DWORD ModuleBaseAddress = 0;
    HANDLE SnapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, PID);
    MODULEENTRY32 ModuleEntry32 = { 0 };
    ModuleEntry32.dwSize = sizeof(MODULEENTRY32);
    if (Module32First(SnapshotHandle, &ModuleEntry32))
    {
        do {
            // IF FOUND MODULE WITH SPECIFIED NAME THEN GET BASE ADDRESS
            if (_tcscmp(ModuleEntry32.szModule, ModuleName) == 0)
            {
                ModuleBaseAddress = (DWORD)ModuleEntry32.modBaseAddr;
                break;
            }
        } while (Module32Next(SnapshotHandle, &ModuleEntry32));
    }
    CloseHandle(SnapshotHandle);
    return ModuleBaseAddress;
}

int WorldToScreen(Vec3 pos, Vec2* screen, float matrix[16], int windowWidth, int windowHeight)
{
    Vec4 clipCoords;
    clipCoords.x = pos.x * matrix[0] + pos.y * matrix[1] + pos.z * matrix[2] + matrix[3];
    clipCoords.y = pos.x * matrix[4] + pos.y * matrix[5] + pos.z * matrix[6] + matrix[7];
    clipCoords.z = pos.x * matrix[8] + pos.y * matrix[9] + pos.z * matrix[10] + matrix[11];
    clipCoords.w = pos.x * matrix[12] + pos.y * matrix[13] + pos.z * matrix[14] + matrix[15];

    if (clipCoords.w < 0.1f)
        return 0;

    Vec3 NDC;
    NDC.x = clipCoords.x / clipCoords.w;
    NDC.y = clipCoords.y / clipCoords.w;
    NDC.z = clipCoords.z / clipCoords.w;

    screen->x = (windowWidth / 2 * NDC.x) + (NDC.x + windowWidth / 2);
    screen->y = -(windowHeight / 2 * NDC.y) + (NDC.y + windowHeight / 2);
    return 1;
}

// RECOMMEND 1 WIDTH OR IT LOOKS BODGY (HAVEN"T FIXED YET)
void ESP(HDC DC, HBRUSH Color, const Vec2 Position, int OuterWidth, int Size) {
    RECT RectRight = { Position.x + (Size / 2) + (OuterWidth / 2), Position.y + (Size / 2), Position.x + (Size / 2) - (OuterWidth / 2), Position.y - (Size / 2) };
    RECT RectLeft = { Position.x - (Size / 2) + (OuterWidth / 2), Position.y + (Size / 2), Position.x - (Size / 2) - (OuterWidth / 2), Position.y - (Size / 2) };
    RECT RectDown = { Position.x + (Size / 2), Position.y + (Size / 2) + (OuterWidth / 2), Position.x - (Size / 2), Position.y + (Size / 2) - (OuterWidth / 2) };
    RECT RectUp = { Position.x + (Size / 2), Position.y - (Size / 2) + (OuterWidth / 2), Position.x - (Size / 2), Position.y - (Size / 2) - (OuterWidth / 2) };
    FillRect(DC, &RectRight, Color);
    FillRect(DC, &RectLeft, Color);
    FillRect(DC, &RectDown, Color);
    FillRect(DC, &RectUp, Color);
}

void UpdatePlayerList() {
    PlayerList.clear();
    for (int x = 0; x < PlayerCount; x++) {
        DWORD Address;
        Player NewPlayer;
        ReadProcessMemory(HProcess, (LPVOID)(ServerBaseAddress + EntityListAddress + (x * 16)), &Address, sizeof(Address), NULL);
        ReadProcessMemory(HProcess, (LPVOID)(Address + XPositionOffset), &NewPlayer.x, sizeof(NewPlayer.x), NULL);
        ReadProcessMemory(HProcess, (LPVOID)(Address + YPositionOffset), &NewPlayer.y, sizeof(NewPlayer.y), NULL);
        ReadProcessMemory(HProcess, (LPVOID)(Address + ZPositionOffset), &NewPlayer.z, sizeof(NewPlayer.z), NULL);
        PlayerList.push_back(NewPlayer);
    }
}

int main() {
    Window = FindWindowA(NULL, WindowName);
    if (!Window) {
        std::cerr << "Game window not found" << std::endl;
        return 1;
    }
    PID;
    GetWindowThreadProcessId(Window, &PID);
    if (!PID) {
        std::cerr << "Failed to get process ID" << std::endl;
        return 1;
    }
    HProcess = OpenProcess(PROCESS_VM_READ, FALSE, PID);
    if (!HProcess) {
        std::cerr << "Failed to open process. Error: " << GetLastError() << std::endl;
        return 1;
    }
    HDC GameDC = GetDC(Window);
    if (!GameDC) {
        std::cerr << "Failed to get DC. Error: " << GetLastError() << std::endl;
        CloseHandle(HProcess);
        return 1;
    }
    ServerBaseAddress = GetModuleBaseAddress(ProcessName1, PID);
    EngineBaseAddress = GetModuleBaseAddress(ProcessName2, PID);
    std::cout << "SERVER BASE ADDRESS 1 : 0x" << std::hex << ServerBaseAddress << std::dec << "\n";
    std::cout << "ENGINE BASE ADDRESS 2 : 0x" << std::hex << EngineBaseAddress << std::dec << "\n";
    while (true) {
        Sleep(10);
        ReadProcessMemory(HProcess, (LPVOID)(EngineBaseAddress + EntityCountAddress), &PlayerCount, sizeof(PlayerCount), NULL);
        ReadProcessMemory(HProcess, (LPVOID)(EngineBaseAddress + ViewMatrixAddress), ViewMatrix, sizeof(ViewMatrix), NULL);
        UpdatePlayerList();
        std::cout << PlayerCount << "\n";
        if (ExtraSense) {
            for (int x = 1; x < PlayerCount; x++) {
                Vec3 Position;
                Position.x = PlayerList[x].x;
                Position.y = PlayerList[x].y;
                Position.z = PlayerList[x].z;
                Vec2 Screen;
                if (WorldToScreen(Position, &Screen, ViewMatrix, SCREEN_WIDTH, SCREEN_HEIGHT)) {
                    ESP(GameDC, Color, { Screen.x,Screen.y }, 2, 50);
                }
            }
        }
        if (GetAsyncKeyState(VK_NUMPAD1) & 1) {
            if (ExtraSense) {
                ExtraSense = false;
                std::cout << "ESP DISABLED\n";
            }
            else if (!ExtraSense) {
                ExtraSense = true;
                std::cout << "ESP ENABLED\n";
            }
        }
    }

    return 0;
}
