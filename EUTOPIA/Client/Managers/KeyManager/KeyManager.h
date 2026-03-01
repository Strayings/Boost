#pragma once

#include <string>

class KeyManager {
   public:

    static void init();


    static bool doesKeyExist();


    static void loadKey();

    static std::string getKeyPath();

    static void saveKey(const std::string& key);

    static std::string getCurrentKey();

   private:

    static std::string keyFilePath;


    static std::string currentKey;
};

