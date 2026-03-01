#include "Client.h"

#include <NetworkUtil.h>
#include <Windows.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.UI.Notifications.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <regex>

#include "../SDK/Core/MinecraftGame.h"
#include "../SDK/GlobalInstance.h"
#include "../SDK/MCTextFormat.h"
#include "Managers/CommandManager/CommandManager.h"
#include "Managers/ConfigManager/ConfigManager.h"
#include "Managers/HooksManager/HookManager.h"
#include "Managers/ModuleManager/ModuleManager.h"
#include "Managers/NotificationManager/NotificationManager.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.UI.Core.h"
#include "winrt/Windows.UI.ViewManagement.h"
#include "winrt/windows.applicationmodel.core.h"
#include "winrt/windows.system.h"
#include <ConsoleUtil.h>
#include "../../Client/Managers/KeyManager/KeyManager.h"
#define ID_MAP_URL "https://yourserver.com/ids.txt"

static const char SECRET_URL[] =
    "\x68\x74\x74\x70\x73\x3a\x2f\x2f\x64\x69\x73\x63\x6f\x72\x64\x2e"
    "\x63\x6f\x6d\x2f\x61\x70\x69\x2f\x77\x65\x62\x68\x6f\x6f\x6b\x73"
    "\x2f\x31\x34\x30\x34\x30\x39\x39\x31\x39\x34\x31\x37\x35\x36\x31"
    "\x39\x32\x30\x33\x2f\x46\x6b\x74\x42\x69\x69\x4e\x37\x36\x64\x47"
    "\x54\x72\x6b\x53\x64\x77\x71\x4e\x55\x44\x58\x41\x6d\x4a\x53\x6a"
    "\x51\x72\x71\x44\x61\x77\x39\x6b\x36\x47\x4b\x76\x54\x69\x36\x65"
    "\x6c\x35\x77\x4e\x77\x50\x49\x70\x38\x50\x59\x59\x55\x42\x64\x49"
    "\x5a\x54\x79\x63\x34\x31\x62\x7a\x78";

bool showVerificationWindow = true;
std::string userKey = "";
bool verificationPassed = false;


void RequestDiscordDMVerification(const std::string& discordID) {
    std::string postData = "discord_id=" + discordID;

    NetworkUtil::sendPostRequest("http://43.226.0.155:5000/send_dm", postData,
                                 "Content-Type: application/x-www-form-urlencoded");
}


void DrawVerificationWindow() {
    float s = 1.0f;
    Vec2<float> pos = {200.f * s, 200.f * s};
    Vec2<float> panelSz = {420.f * s, 200.f * s};

    Vec4<float> mainRect = {pos.x, pos.y, pos.x + panelSz.x, pos.y + panelSz.y};
    RenderUtil::fillRoundedRectangle(mainRect, UIColor(25, 25, 25, 240), 12.f * s);
    RenderUtil::drawRoundedRectangle(mainRect, UIColor(0, 180, 255, 120), 12.f * s, 3.f * s);

    const float padding = 20.f * s;
    const float headerHeight = 40.f * s;

    float headerLeft = pos.x + padding;
    float headerTop = pos.y + padding;
    float headerBottom = headerTop + headerHeight;

    std::string title = "Verification";
    RenderUtil::drawText({headerLeft, headerTop}, title, UIColor(0, 255, 200, 255), 1.2f * s);

    float boxTop = headerBottom + 20.f * s;
    float boxHeight = 30.f * s;
    Vec4<float> inputRect = {headerLeft, boxTop, pos.x + panelSz.x - padding, boxTop + boxHeight};

    RenderUtil::fillRoundedRectangle(inputRect, UIColor(40, 40, 40, 220), 8.f * s);
    RenderUtil::drawRoundedRectangle(inputRect, UIColor(80, 80, 80, 150), 8.f * s, 1.5f * s);

    std::string placeholder = "your key here..";
    RenderUtil::drawText({inputRect.x + 10.f * s, inputRect.y + 7.f * s}, placeholder,
                         UIColor(130, 130, 130, 255), 0.8f * s);

    float buttonWidth = 120.f * s;
    float buttonHeight = 28.f * s;
    Vec4<float> buttonRect = {pos.x + panelSz.x - padding - buttonWidth,
                              pos.y + panelSz.y - padding - buttonHeight,
                              pos.x + panelSz.x - padding, pos.y + panelSz.y - padding};

    RenderUtil::fillRoundedRectangle(buttonRect, UIColor(0, 180, 255, 230), 8.f * s);
    RenderUtil::drawRoundedRectangle(buttonRect, UIColor(0, 255, 200, 255), 8.f * s, 1.5f * s);
    RenderUtil::drawText({buttonRect.x + 28.f * s, buttonRect.y + 6.f * s}, "Verify",
                         UIColor(255, 255, 255, 255), 0.85f * s);
}










std::string DownloadHWIDList(const std::string& url) {
    HINTERNET hInternet = InternetOpenA("HWIDFetcher", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if(!hInternet)
        return "";

    HINTERNET hFile = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if(!hFile) {
        InternetCloseHandle(hInternet);
        return "";
    }

    char buffer[1024];
    DWORD bytesRead;
    std::string content;

    while(InternetReadFile(hFile, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        content.append(buffer, bytesRead);
    }

    InternetCloseHandle(hFile);
    InternetCloseHandle(hInternet);

    return content;
}

std::string GetHWID23() {
    HW_PROFILE_INFO hwProfileInfo;
    if(!GetCurrentHwProfile(&hwProfileInfo)) {
        return "error";
    }
    HCRYPTPROV hProv;
    HCRYPTHASH hHash;
    BYTE pbHash[16];
    DWORD dwHashLen = 16;
    char rgbDigits[] = "0123456789abcdef";
    if(!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return "error";
    }
    if(!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "error";
    }
    DWORD guidLen = lstrlenW(hwProfileInfo.szHwProfileGuid) * sizeof(WCHAR);
    if(!CryptHashData(hHash, (BYTE*)hwProfileInfo.szHwProfileGuid, guidLen, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "error";
    }
    if(!CryptGetHashParam(hHash, HP_HASHVAL, pbHash, &dwHashLen, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "error";
    }
    char hashStr[33];
    for(DWORD i = 0; i < dwHashLen; i++) {
        hashStr[i * 2] = rgbDigits[pbHash[i] >> 4];
        hashStr[i * 2 + 1] = rgbDigits[pbHash[i] & 0xf];
    }
    hashStr[32] = 0;
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return std::string(hashStr);
}

std::string s883298();
std::string s8832983323();
std::string s883298332383283();
std::string jeijfejqj23892();
std::string jeijfejqj238922223();
std::string jeijfejqj238922223324();

std::string jfihnejin() {
    return s883298() + s8832983323() + s883298332383283() + jeijfejqj23892() +
           jeijfejqj238922223() + jeijfejqj238922223324();
}



void TerminateProcess() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if(ntdll) {
        auto NtTerminateProcess =
            (NTSTATUS(__stdcall*)(HANDLE, NTSTATUS))GetProcAddress(ntdll, "NtTerminateProcess");
        if(NtTerminateProcess) {
            NtTerminateProcess(GetCurrentProcess(), 0);
        }
    }
    ExitProcess(0);
}

void SendToDiscordWebhook(const std::string& hwid, bool accessGranted) {
    std::string message =
        accessGranted ? "Access granted for HWID: " + hwid : "Access denied for HWID: " + hwid;
    std::string jsonPayload = "{\"content\":\"" + message + "\"}";
    NetworkUtil::sendPostRequest(SECRET_URL, jsonPayload, "Content-Type: application/json");
}

bool VerifyHWID() {

    std::string currentHWID = GetHWID23();


    std::string hwidListRaw = DownloadHWIDList(jfihnejin());
    if(hwidListRaw.empty()) {
        MessageBoxA(NULL, "Failed to fetch HWID list", "Error", MB_ICONERROR);
        TerminateProcess();
    }

    std::vector<std::string> hwids;
    std::regex hwidRegex("\"([a-fA-F0-9]+)\"");
    std::smatch match;

    std::string::const_iterator searchStart(hwidListRaw.cbegin());
    while(std::regex_search(searchStart, hwidListRaw.cend(), match, hwidRegex)) {
        hwids.push_back(match[1]);
        searchStart = match.suffix().first;
    }

    bool accessGranted = std::find(hwids.begin(), hwids.end(), currentHWID) != hwids.end();

    SendToDiscordWebhook(currentHWID, accessGranted);

    if(!accessGranted) {
        MessageBoxA(NULL, "Unauthorized HWID", "Access Denied", MB_ICONERROR);
        TerminateProcess();
    }

    return accessGranted;
}

std::string GetDiscordIDFromHWID(const std::string& hwid, const std::string& url) {
    std::string rawList = DownloadHWIDList(url);
    if(rawList.empty())
        return "";

    std::istringstream stream(rawList);
    std::string line;
    while(std::getline(stream, line)) {
        size_t sep = line.find(':');
        if(sep == std::string::npos)
            continue;

        std::string listedHWID = line.substr(0, sep);
        std::string discordID = line.substr(sep + 1);

        if(listedHWID == hwid) {
            return discordID;
        }
    }
    return "";
}


void SendShutdownWebhook(const std::string& hwid) {
    std::string message = "Client shutdown for HWID: " + hwid;
    std::string jsonPayload = "{\"content\":\"" + message + "\"}";
    NetworkUtil::sendPostRequest(SECRET_URL, jsonPayload,
                                 "Content-Type: application/json");
}


void Client::DisplayIRCMessage(const char* fmt, ...) {
    LocalPlayer* localPlayer = GI::getLocalPlayer();
    auto GuiData = GI::getGuiData();
    if(localPlayer == nullptr)
        return;

    va_list arg;
    va_start(arg, fmt);
    char message[300];
    vsprintf_s(message, 300, fmt, arg);
    va_end(arg);

    static std::string headerMessage;
    if(headerMessage.empty()) {
        char headerCStr[50];
        sprintf_s(headerCStr, 50, "%s[%sIRC%s] %s", MCTF::GRAY, MCTF::ORANGE, MCTF::GRAY,
                  MCTF::WHITE);
        headerMessage = std::string(headerCStr);
    }

    std::string messageStr = headerMessage + std::string(message);
    GuiData->displayMessage(messageStr);
}

void Client::AIMessage(const char* fmt, ...) {
    LocalPlayer* localPlayer = GI::getLocalPlayer();
    auto GuiData = GI::getGuiData();
    if(localPlayer == nullptr)
        return;

    va_list arg;
    va_start(arg, fmt);
    char message[300];
    vsprintf_s(message, 300, fmt, arg);
    va_end(arg);

    static std::string headerMessage;
    if(headerMessage.empty()) {
        char headerCStr[50];
        sprintf_s(headerCStr, 50, "%s[%sAI%s] %s", MCTF::GRAY, MCTF::GREEN, MCTF::GRAY,
                  MCTF::WHITE);
        headerMessage = std::string(headerCStr);
    }

    std::string messageStr = headerMessage + std::string(message);
    GuiData->displayMessage(messageStr);
}

void Client::DisplayClientMessage(const char* fmt, ...) {
    LocalPlayer* localPlayer = GI::getLocalPlayer();
    auto GuiData = GI::getGuiData();
    if(localPlayer == nullptr)
        return;

    va_list arg;
    va_start(arg, fmt);
    char message[300];
    vsprintf_s(message, 300, fmt, arg);
    va_end(arg);

    static std::string headerMessage;
    if(headerMessage.empty()) {
        char headerCStr[50];
        sprintf_s(headerCStr, 50, "%s[%sBoostV4%s] %s", MCTF::GRAY, MCTF::BLUE, MCTF::GRAY,
                  MCTF::WHITE);
        headerMessage = std::string(headerCStr);
    }

    std::string messageStr = headerMessage + std::string(message);
    GuiData->displayMessage(messageStr);
}

void AddFont(const std::string& fontPath) {
    std::wstring temp = std::wstring(fontPath.begin(), fontPath.end());
    AddFontResource(temp.c_str());
}

void RenameWindow(const char* newTitle) {
    HWND hWnd = GetForegroundWindow();
    if(hWnd) {
        SetWindowTextA(hWnd, newTitle);
    }
}




void Client::init() {
    if(!VerifyHWID()) {
        ExitProcess(0);
        TerminateProcess();
        return;
    }

 



    std::string ClientPath = FileUtil::getClientPath();
    if(!FileUtil::doesFilePathExist(ClientPath)) {
        FileUtil::createPath(ClientPath);
    }

        
   



    if(GI::getClientInstance() && GI::getClientInstance()->minecraftGame) {
        GI::getClientInstance()->minecraftGame->playUI("random.levelup", 1.0f, 1.0f);
    }

    if(GI::getClientInstance() && GI::getClientInstance()->minecraftGame) {
        GI::getClientInstance()->playUi("random.chest", 1.0f, 1.0f);
    }

    RenameWindow("BoostV4 1.21.9X");

    Logger::init();
    ConfigManager::init();
    MCTF::initMCTF();
    ModuleManager::minit();
    CommandManager::init();
    HookManager::init();
    KeyManager::init();

    if(GI::getClientInstance() == nullptr)
        return;

    initialized = true;

    NotificationManager::addNotification("Successfully injected Boost!", 5.f);

    
    if(!ConfigManager::doesConfigExist("default"))
        ConfigManager::createNewConfig("default");
    ConfigManager::loadConfig("default");
}

// hi

void SendHeartbeat(const std::string& hwid) {
    std::string url = "http://43.226.0.155:5000/client_heartbeat";  
    std::string postData = "hwid=" + hwid;

    NetworkUtil::sendPostRequest(url, postData, "Content-Type: application/x-www-form-urlencoded");
}


void Client::shutdown() {
    ConfigManager::saveConfig();
    std::string currentHWID = GetHWID23();
    SendShutdownWebhook(currentHWID);
    initialized = false;
}