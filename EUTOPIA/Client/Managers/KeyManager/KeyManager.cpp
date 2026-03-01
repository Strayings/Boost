#include "KeyManager.h"

#include "../../../Utils/FileUtil.h"
#include "../../Client/Client.h"
#include "..\SDK\MCTextFormat.h"


std::string KeyManager::keyFilePath = FileUtil::getClientPath() + "key.txt";
std::string KeyManager::currentKey = "";

void KeyManager::init() {
    keyFilePath = FileUtil::getClientPath() + "/key.txt";
    std::ofstream keyFile(keyFilePath, std::ios::app);
}


bool KeyManager::doesKeyExist() {
    return FileUtil::doesFilePathExist(keyFilePath);
}

std::string KeyManager::getKeyPath() {
    return keyFilePath;
}


void KeyManager::loadKey() {
    std::ifstream keyFile(keyFilePath.c_str(), std::ifstream::binary);
    if(keyFile.is_open()) {
        std::getline(keyFile, currentKey);
        keyFile.close();
        Client::DisplayClientMessage("Key loaded!", MCTF::YELLOW);
    } else {
        Client::DisplayClientMessage(
            "Key file not found, please authenticate using .key [your_key].", MCTF::RED);
    }
}

void KeyManager::saveKey(const std::string& key) {
    FileUtil::createPath(FileUtil::getClientPath());  
    std::ofstream keyFile(keyFilePath, std::ofstream::binary);
    if(!keyFile.is_open()) {
        Client::DisplayClientMessage("Failed to open key file for saving!", MCTF::RED);
        return;
    }

    keyFile << key;
    keyFile.close();
    Client::DisplayClientMessage("Key saved to key.txt", MCTF::GREEN);
}

std::string KeyManager::getCurrentKey() {
    return currentKey;
}
