#include "KeyCommand.h"

#include <wininet.h>

#include <sstream>
#include <vector>

#include "../../Client/Client.h"
#include "../../Client/Managers/ModuleManager/ModuleManager.h"

#pragma comment(lib, "wininet.lib")



static std::string DownloadKeyList(const std::string& url) {
    HINTERNET hInternet = InternetOpenA("KeyFetcher", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
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

    while(InternetReadFile(hFile, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
        content.append(buffer, bytesRead);

    InternetCloseHandle(hFile);
    InternetCloseHandle(hInternet);

    return content;
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \r\n\t");
    size_t end = s.find_last_not_of(" \r\n\t");
    if(start == std::string::npos)
        return "";
    return s.substr(start, end - start + 1);
}

KeyCommand::KeyCommand() : CommandBase("key", "Authenticate with your key.", "<key>", {}) {}

bool KeyCommand::execute(const std::vector<std::string>& args) {

    auto autokey = ModuleManager::getModule<AutoKey>();
    if(autokey && autokey->isDoneOrNot) {
        Client::DisplayClientMessage("You are already authenticated.", MCTF::YELLOW);
        return true;
    }
    if(args.size() < 2) {
        Client::DisplayClientMessage("Usage: key <your_key>", MCTF::RED);
        return false;
    }



    std::string inputKey = args[1];
    Client::DisplayClientMessage("Checking your key...", MCTF::YELLOW);

    std::string keyList = DownloadKeyList("http://43.226.0.155:5000//getkeys");
    if(keyList.empty()) {
        Client::DisplayClientMessage("Failed to fetch key list.", MCTF::RED);
        return false;
    }

    if(!keyList.empty() && (unsigned char)keyList[0] == 0xEF)
        keyList = keyList.substr(3);

    keyList.erase(std::remove(keyList.begin(), keyList.end(), '['), keyList.end());
    keyList.erase(std::remove(keyList.begin(), keyList.end(), ']'), keyList.end());

    std::istringstream stream(keyList);
    std::string token;
    bool keyFound = false;

    while(std::getline(stream, token, ',')) {
        token = trim(token);
        if(!token.empty() && token.front() == '"' && token.back() == '"')
            token = token.substr(1, token.size() - 2);

        if(token == inputKey) {
            keyFound = true;
            KeyManager::saveKey(inputKey); 
            GI::DisplayClientMessage("Your key has been saved for AutoKey!");
            break;
        }
    }

    if(!keyFound) {
        Client::DisplayClientMessage("Invalid key!", MCTF::RED);
        return false;
    }

    if(ModuleManager::hasInitialized) {
        Client::DisplayClientMessage("You are already authenticated.", MCTF::YELLOW);
        return true;
    }

    Client::DisplayClientMessage("Fully injected!", MCTF::GREEN);
    ModuleManager::init();

    return true;
}
