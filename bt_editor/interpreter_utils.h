#ifndef INTERPRETER_UTILS_H
#define INTERPRETER_UTILS_H

#include "bt_editor_base.h"

namespace Interpreter
{

class ConditionEvaluation : public std::exception
{
public:
    const char* what () {
        return "Condition Evaluation";
    }
};


class InterpreterNode : public BT::AsyncActionNode
{
public:
    InterpreterNode(const std::string& name, const BT::NodeConfiguration& config);

    virtual void halt() override;

    BT::NodeStatus tick() override;

    void set_status(const BT::NodeStatus& status);
};


class InterpreterConditionNode : public BT::ConditionNode
{
public:
    InterpreterConditionNode(const std::string& name, const BT::NodeConfiguration& config);

    BT::NodeStatus tick() override;

    BT::NodeStatus executeTick() override;

    void set_status(const BT::NodeStatus& status);

private:
    BT::NodeStatus return_status_;
};

}  // namespace
#endif  // INTERPRETER_UTILS_H
