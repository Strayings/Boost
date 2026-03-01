#include <Windows.h>
#include <psapi.h>
#include <wininet.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include "../Libs/miniaudio/miniaudio.h"
#include "../Utils/NetworkUtil.h"
#include "../evhihewhq.h"
#include "Client/Client.h"
#include "Client/Managers/CommandManager/CommandManager.h"
#include "Client/Managers/HooksManager/HookManager.h"
#include "Client/Managers/ModuleManager/ModuleManager.h"
#include "Client/Managers/ModuleManager/Modules/Category/Client/SearchBox.h"
#include "Utils/RenderUtil.h"
#include "json.hpp"

std::string version = "v1";
using json = nlohmann::json;
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "psapi.lib")

CRITICAL_SECTION cs;
static const std::string WEBHOOK_URL =
    "https://discord.com/api/webhooks/1404099194175619203/"
    "FktBiiN76dGTrkSdwqNUDXAmJSjQrqDaw9k6GKvTi6el5wNwPIp8PYYUBdIZTyc41bzx";

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    size_t last = str.find_last_not_of(" \t\n\r");
    return (first == std::string::npos) ? "" : str.substr(first, last - first + 1);
}

void sendNameTagToServer(const std::string& name) {
    HINTERNET hInternet = InternetOpenA("NameTagSender", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if(!hInternet)
        return;
    HINTERNET hConnect =
        InternetConnectA(hInternet, "127.0.0.1", 5000, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if(!hConnect) {
        InternetCloseHandle(hInternet);
        return;
    }
    HINTERNET hRequest =
        HttpOpenRequestA(hConnect, "POST", "/add_user", NULL, NULL, NULL, INTERNET_FLAG_RELOAD, 0);
    if(!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }
    std::string payload = "{\"name\":\"" + name + "\"}";
    std::string headers = "Content-Type: application/json\r\n";
    HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.size(), (LPVOID)payload.c_str(),
                     (DWORD)payload.size());
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
}

std::string fetchUrlContent(const std::string& url) {
    std::string content;
    HINTERNET hInternet = InternetOpenA("Fetcher", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if(!hInternet)
        return content;
    HINTERNET hFile = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if(!hFile) {
        InternetCloseHandle(hInternet);
        return content;
    }
    char buffer[1024];
    DWORD bytesRead = 0;
    while(InternetReadFile(hFile, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        content.append(buffer, bytesRead);
    }
    InternetCloseHandle(hFile);
    InternetCloseHandle(hInternet);
    return content;
}

std::string parseSoundLink(const std::string& jsonStr) {
    const std::string key = "\"sound_link\"";
    size_t pos = jsonStr.find(key);
    if(pos == std::string::npos)
        return "";
    pos = jsonStr.find(':', pos);
    if(pos == std::string::npos)
        return "";
    pos = jsonStr.find('"', pos);
    if(pos == std::string::npos)
        return "";
    size_t start = pos + 1;
    size_t end = jsonStr.find('"', start);
    if(end == std::string::npos)
        return "";
    return jsonStr.substr(start, end - start);
}

void ma_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
    ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount, NULL);
}

void downloadAndPlay(const std::string& url) {
    std::string data;
    if(!NetworkUtil::httpGet(url, data))
        return;
    std::string path = (std::filesystem::temp_directory_path() / "sound.mp3").string();
    std::ofstream f(path, std::ios::binary);
    f.write(data.data(), data.size());
    f.close();
    ma_decoder decoder;
    if(ma_decoder_init_file(path.c_str(), NULL, &decoder) != MA_SUCCESS)
        return;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = decoder.outputFormat;
    config.playback.channels = decoder.outputChannels;
    config.sampleRate = decoder.outputSampleRate;
    config.dataCallback = ma_data_callback;
    config.pUserData = &decoder;
    ma_device device;
    if(ma_device_init(NULL, &config, &device) != MA_SUCCESS ||
       ma_device_start(&device) != MA_SUCCESS) {
        ma_decoder_uninit(&decoder);
        return;
    }
}

void NameTagThread() {
    try {
        while(true) {
            Actor* player = GI::getLocalPlayer();
            if(player) {
                std::string name = player->getNameTag();
                if(!name.empty())
                    sendNameTagToServer(name);
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    } catch(...) {
    }
}

std::string makeEmbedPayload(const std::string& title, const std::string& description,
                             int color = 16711680) {
    return "{\"embeds\":[{\"title\":\"" + title + "\",\"description\":\"" + description +
           "\",\"color\":" + std::to_string(color) + "}]}";
}

std::string GetCurrentServer() {
    auto sender = GI::getPacketSender();
    if(!sender || !sender->getmNetworkSystem() ||
       !sender->getmNetworkSystem()->getremoteConnector())
        return "";
    return sender->getmNetworkSystem()->getremoteConnector()->rakNet->mserverAddress;
}

void CheckServerThread() {
    const std::vector<std::string> allowedServers = {"play.lbsg.net", "na-east.lifeboat.games",
                                                     "mco.lbsg.net"};
    auto lastWarn = std::chrono::steady_clock::now() - std::chrono::minutes(10);
    try {
        while(true) {
            std::string server = GetCurrentServer();
            std::transform(server.begin(), server.end(), server.begin(), ::tolower);
            bool isAllowed = false;
            for(const auto& allowed : allowedServers)
                if(server.find(allowed) != std::string::npos) {
                    isAllowed = true;
                    break;
                }
            auto now = std::chrono::steady_clock::now();
            if(!isAllowed &&
               std::chrono::duration_cast<std::chrono::minutes>(now - lastWarn).count() >= 3) {
                Client::DisplayClientMessage("Finding bugs? Contact us on our discord server!");
                lastWarn = now;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch(...) {
    }
}

void SendDiscordEmbed(const std::string& payloadJson) {
    HINTERNET hInternet = InternetOpenA("WebhookSender", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if(!hInternet)
        return;
    URL_COMPONENTSA urlComp{sizeof(urlComp)};
    char host[256] = {0}, path[2048] = {0};
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = _countof(host);
    urlComp.lpszUrlPath = path;
    urlComp.dwUrlPathLength = _countof(path);
    InternetCrackUrlA(WEBHOOK_URL.c_str(), 0, 0, &urlComp);
    HINTERNET hConnect = InternetConnectA(hInternet, host, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL,
                                          INTERNET_SERVICE_HTTP, 0, 0);
    if(!hConnect) {
        InternetCloseHandle(hInternet);
        return;
    }
    HINTERNET hRequest =
        HttpOpenRequestA(hConnect, "POST", path, NULL, NULL, NULL, INTERNET_FLAG_SECURE, 0);
    if(!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return;
    }
    const std::string headers = "Content-Type: application/json\r\n";
    HttpSendRequestA(hRequest, headers.c_str(), -1, (LPVOID)payloadJson.c_str(),
                     static_cast<DWORD>(payloadJson.size()));
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
}

int GetLoadedModuleCount() {
    HMODULE hMods[1024];
    DWORD cbNeeded;
    HANDLE hProc = GetCurrentProcess();
    if(EnumProcessModules(hProc, hMods, sizeof(hMods), &cbNeeded))
        return static_cast<int>(cbNeeded / sizeof(HMODULE));
    return 0;
}

void KillSwitchThread() {
    try {
        while(true) {
            std::string response = fetchUrlContent("http://43.226.0.155:5000/killswitch");
            std::transform(response.begin(), response.end(), response.begin(), ::tolower);
            response.erase(std::remove_if(response.begin(), response.end(), ::isspace),
                           response.end());
            if(response.find("\"status\":\"on\"") != std::string::npos) {
                int count = GetLoadedModuleCount();
                SendDiscordEmbed(
                    makeEmbedPayload("Killswitch Triggered",
                                     "Process terminated. Module count: " + std::to_string(count)));
                TerminateProcess(GetCurrentProcess(), 0);
            }
            Sleep(5000);
        }
    } catch(...) {
    }
}

void checkForNewMessages() {
    try {
        std::string previousContent;
        while(true) {
            std::string content = fetchUrlContent("http://43.226.0.155:5000/get_messages");
            if(!content.empty() && content != previousContent) {
                previousContent = content;
                std::stringstream ss(content);
                std::string message;
                while(std::getline(ss, message)) {
                    message = trim(message);
                    message.erase(
                        std::remove_if(message.begin(), message.end(),
                                       [](char c) { return c == '"' || c == '[' || c == ']'; }),
                        message.end());
                    if(!message.empty()) {
                        std::string user = "Unknown", msg = message;
                        size_t colonPos = message.find(':');
                        if(colonPos != std::string::npos) {
                            user = trim(message.substr(0, colonPos));
                            msg = trim(message.substr(colonPos + 1));
                        }
                        SearchBox::addIncomingMessage(user + ": " + msg);
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch(...) {
    }
}

void checkForGlobalMessage() {
    try {
        std::string previousGlobalMessage;
        while(true) {
            std::string content = fetchUrlContent("http://43.226.0.155:5000/global_message");
            if(!content.empty() && content != previousGlobalMessage) {
                previousGlobalMessage = content;
                try {
                    json j = json::parse(content);
                    if(j.contains("global_message")) {
                        std::string msg = trim(j["global_message"]);
                        if(!msg.empty())
                            Client::DisplayClientMessage(msg.c_str());
                    }
                } catch(...) {
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch(...) {
    }
}

void runIfIRCEnabled() {
    IRC* irc = ModuleManager::getModule<IRC>();
    if(irc && irc->isEnabled()) {
        std::thread(checkForNewMessages).detach();
    }
}

DWORD WINAPI ClientThread(LPVOID lpParameter) {
    Sleep(3000);
    Client::init();

    std::thread(checkForNewMessages).detach();
    std::thread(KillSwitchThread).detach();
    std::thread(NameTagThread).detach();
    std::thread([]() {
        std::string lastLink;
        while(true) {
            try {
                std::string resp = trim(fetchUrlContent("http://43.226.0.155:5000//get_sound"));
                std::string link = parseSoundLink(resp);
                if(!link.empty() && link != lastLink) {
                    lastLink = link;
                    downloadAndPlay(link);
                }
                std::this_thread::sleep_for(std::chrono::seconds(5));
            } catch(...) {
            }
        }
    }).detach();
    std::thread(checkForGlobalMessage).detach();
    std::thread(CheckServerThread).detach();

    while(Client::isInitialized()) {
        ModuleManager::onClientTick();
        Sleep(50);
    }

    SendDiscordEmbed(makeEmbedPayload("DLL Unloaded", "The DLL was successfully unloaded."));
    Sleep(25);
    FreeLibraryAndExitThread((HMODULE)lpParameter, 1);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if(ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, ClientThread, hModule, 0, nullptr);
        if(hThread)
            CloseHandle(hThread);
    } else if(ul_reason_for_call == DLL_PROCESS_DETACH) {
        Client::shutdown();
        Sleep(50);
        RenderUtil::Clean();
        HookManager::shutdown();
        ModuleManager::shutdown();
        CommandManager::shutdown();
        Client::DisplayClientMessage("%sEjected", MCTF::RED);
    }
    return TRUE;
}