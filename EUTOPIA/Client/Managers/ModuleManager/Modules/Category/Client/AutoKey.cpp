#include "AutoKey.h"

#include <wininet.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#pragma comment(lib, "wininet.lib")

AutoKey::AutoKey()
    : Module("AutoKey", "Automatically authenticate with your key", Category::CLIENT) {}

void AutoKey::onNormalTick(LocalPlayer* player) {
    if(ModuleManager::hasInitialized) {
        this->setEnabled(false);
        return;
    }
    if(!player)
        return;
    if(isDoneOrNot) {
        GI::DisplayClientMessage("Already authenticated.");
        this->setEnabled(false);
        return;
    }

    
    std::filesystem::path keyPath = std::filesystem::path(FileUtil::getClientPath()) / "key.txt";

    std::ifstream keyFile(keyPath);
    if(!keyFile.is_open()) {
        this->setEnabled(false);
        return;
    }

    std::string key;
    std::getline(keyFile, key);
    keyFile.close();

    key.erase(std::remove(key.begin(), key.end(), '\r'), key.end());
    key.erase(std::remove(key.begin(), key.end(), '\n'), key.end());

    size_t start = key.find_first_not_of(" \t");
    if(start == std::string::npos) {
        // key is empty or only whitespace
        this->setEnabled(false);
        return;
    }

    size_t end = key.find_last_not_of(" \t");
    key = key.substr(start, end - start + 1);

    if(key.empty()) {
        this->setEnabled(false);
        return;
    }

    if(!validateKey(key)) {
        this->setEnabled(false);
        return;
    }

    if(!ModuleManager::hasInitialized)
        ModuleManager::init();

    GI::DisplayClientMessage("Authenticated!");
    isDoneOrNot = true;
    this->setEnabled(false);
}

bool AutoKey::validateKey(const std::string& key) {
    std::string keys = downloadKeyList("http://43.226.0.155:5000//getkeys");
    if(keys.empty())
        return false;

    if((unsigned char)keys[0] == 0xEF)
        keys = keys.substr(3);

    keys.erase(std::remove(keys.begin(), keys.end(), '['), keys.end());
    keys.erase(std::remove(keys.begin(), keys.end(), ']'), keys.end());

    std::istringstream stream(keys);
    std::string token;
    while(std::getline(stream, token, ',')) {
        token = trim(token);
        if(!token.empty() && token.front() == '"' && token.back() == '"')
            token = token.substr(1, token.size() - 2);
        if(token == key)
            return true;
    }
    return false;
}

std::string AutoKey::downloadKeyList(const std::string& url) {
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

std::string AutoKey::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \r\n\t");
    size_t end = s.find_last_not_of(" \r\n\t");
    if(start == std::string::npos)
        return "";
    return s.substr(start, end - start + 1);
}
