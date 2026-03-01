#pragma once
#include <string>

#include "../../Client/Client.h"
#include "../../Client/Managers/KeyManager/KeyManager.h"
#include "../../Client/Managers/ModuleManager/ModuleManager.h"
#include "../../ModuleBase/Module.h"

class AutoKey : public Module {
   public:
    AutoKey();
    void onNormalTick(LocalPlayer* player) override;
    bool isDoneOrNot = false;

   private:
    bool validateKey(const std::string& key);
    std::string downloadKeyList(const std::string& url);
    std::string trim(const std::string& s);
};
