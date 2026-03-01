#pragma once
#include "CommandBase.h"

class AICommand : public CommandBase {
   public:
    AICommand();
    bool execute(const std::vector<std::string>& args) override;
};
