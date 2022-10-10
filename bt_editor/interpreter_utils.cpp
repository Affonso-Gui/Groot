#include "interpreter_utils.h"

//////
// InterpreterNode
//////

Interpreter::InterpreterNode::
InterpreterNode(const std::string& name, const BT::NodeConfiguration& config) :
    BT::AsyncActionNode(name,config)
{}

void Interpreter::InterpreterNode::halt()
{}

BT::NodeStatus Interpreter::InterpreterNode::tick()
{
    return BT::NodeStatus::RUNNING;
}

void Interpreter::InterpreterNode::set_status(const BT::NodeStatus& status)
{
    setStatus(status);
}


//////
// InterpreterConditionNode
//////

Interpreter::InterpreterConditionNode::
InterpreterConditionNode(const std::string& name, const BT::NodeConfiguration& config) :
    BT::ConditionNode(name,config), return_status_(BT::NodeStatus::IDLE)
{}

BT::NodeStatus Interpreter::InterpreterConditionNode::tick()
{
    return return_status_;
}

BT::NodeStatus Interpreter::InterpreterConditionNode::executeTick()
{
    const NodeStatus status = tick();
    if (status == NodeStatus::IDLE) {
        setStatus(NodeStatus::RUNNING);
        throw ConditionEvaluation();
    }
    return_status_ = NodeStatus::IDLE;
    setStatus(status);
    return status;
}

void Interpreter::InterpreterConditionNode::set_status(const BT::NodeStatus& status)
{
    return_status_ = status;
    setStatus(status);
}
