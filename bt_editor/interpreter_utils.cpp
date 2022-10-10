#include "interpreter_utils.h"

using WsClient = SimpleWeb::SocketClient<SimpleWeb::WS>;


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


//////
// RosBridgeConnectionThread
//////

Interpreter::RosBridgeConnectionThread::
RosBridgeConnectionThread(const std::string& address) :
    _rbc(address), _address(address), _client_name("base_connection")
{}

void Interpreter::RosBridgeConnectionThread::run()
{
    _rbc.addClient(_client_name);
    auto client = _rbc.getClient(_client_name);
    client->on_open = [this](std::shared_ptr<WsClient::Connection> connection) {
        emit connectionCreated();
    };
    client->on_close = [this](std::shared_ptr<WsClient::Connection> connection,
                              int status, const std::string& what) {
        emit connectionError("Connection closed.");
    };
    client->on_error = [this](std::shared_ptr<WsClient::Connection> connection,
                              const SimpleWeb::error_code &ec) {
        emit connectionError(QString(fmt::format("Could not connect to {} {}",
                                                 _address, ec.message()).c_str()));
    };
    client->start();
    client->on_open = NULL;
    client->on_message = NULL;
    client->on_close = NULL;
    client->on_error = NULL;
}

void Interpreter::RosBridgeConnectionThread::stop()
{
    _rbc.stopClient(_client_name);
}
