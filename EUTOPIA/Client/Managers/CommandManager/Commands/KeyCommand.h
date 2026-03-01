#pragma once
#include "../../../Client.h"
#include "CommandBase.h"

class KeyCommand : public CommandBase {
   public:
    KeyCommand();
    bool execute(const std::vector<std::string>& args) override;
};
