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
// InterpreterActionNode
//////

Interpreter::InterpreterActionNode::
InterpreterActionNode(const std::string& name, const BT::NodeConfiguration& config) :
    InterpreterNode(name,config),
    _exec_thread(nullptr)
{}

void Interpreter::InterpreterActionNode::halt()
{
    if (isRunning()) {
        _exec_thread->stop();
        _exec_thread = nullptr;
    }
}

bool Interpreter::InterpreterActionNode::isRunning()
{
    return (_exec_thread && _exec_thread->isRunning());
}

void Interpreter::InterpreterActionNode::set_exec_thread(ExecuteActionThread* exec_thread)
{
    _exec_thread = exec_thread;
}


//////
// InterpreterSubscriberNode
//////

Interpreter::InterpreterSubscriberNode::
InterpreterSubscriberNode(const std::string& name, const BT::NodeConfiguration& config) :
    InterpreterNode(name,config)
{}


//////
// InterpreterConditionNode
//////

Interpreter::InterpreterConditionNode::
InterpreterConditionNode(const std::string& name, const BT::NodeConfiguration& config) :
    BT::ConditionNode(name,config), return_status_(BT::NodeStatus::IDLE), _connected(false)
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

BT::NodeStatus Interpreter::InterpreterConditionNode::executeNode()
{
    rapidjson::Document request = getRequestFromPorts(_node, this);
    service_client_->call(request);
    service_client_->waitForResult();
    auto result = service_client_->getResult();
    if (result.HasMember("success") &&
        result["success"].IsBool() &&
        result["success"].GetBool()) {
        return NodeStatus::SUCCESS;
    }
    return NodeStatus::FAILURE;
}

void Interpreter::InterpreterConditionNode::set_status(const BT::NodeStatus& status)
{
    return_status_ = status;
    setStatus(status);
}

void Interpreter::InterpreterConditionNode::
connect(const AbstractTreeNode& node, const std::string& host, int port)
{
    if (_connected) {
        return;
    }
    _node = node;
    std::string name = node.model.ports.find("service_name")->second.default_value.toStdString();
    service_client_ = std::make_unique<roseus_bt::RosbridgeServiceClient>(host, port, name);
    _connected = true;
}

//////
// RosBridgeConnectionThread
//////

Interpreter::RosBridgeConnectionThread::
RosBridgeConnectionThread(const std::string& hostname, const std::string& port) :
    _rbc(fmt::format("{}:{}", hostname, port)),
    _address(fmt::format("{}:{}", hostname, port))
{}

Interpreter::RosBridgeConnectionThread::
~RosBridgeConnectionThread() {}

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
    clearSubscribers();
    _rbc.removeClient(_client_name);
}

void Interpreter::RosBridgeConnectionThread::clearSubscribers()
{
    for (auto sub : _subscribers) {
        _rbc.removeClient(sub);
    }
}

void Interpreter::RosBridgeConnectionThread::registerSubscriber(const AbstractTreeNode& node,
                                                                const BT::TreeNode::Ptr& tree_node)
{
    auto port_model = node.model.ports.find("message_type")->second;
    std::string topic_type = port_model.default_value.toStdString();
    std::string topic_name;
    auto res = tree_node->getInput("topic_name", topic_name);
    if (!res) throw std::runtime_error(res.error());

    tree_node->setOutput<uint8_t>("received_port", false);
    tree_node->setOutput<rapidjson::CopyDocument>("output_port",
                                                  rapidjson::CopyDocument(rapidjson::kObjectType));
    auto cb = [tree_node, topic_type](std::shared_ptr<WsClient::Connection> connection,
                                      std::shared_ptr<WsClient::InMessage> in_message) {
        std::string message = in_message->string();
        rapidjson::CopyDocument document;
        document.Parse(message.c_str());
        document.Swap(document["msg"]);

        setOutputValue(tree_node.get(), "output_port", topic_type, document);
        tree_node->setOutput<uint8_t>("received_port", true);
    };
    _subscribers.push_back(topic_name);
    _rbc.addClient(topic_name);
    _rbc.subscribe(topic_name, topic_name, cb,  "", topic_type);
}


//////
// ExecuteActionThread
//////


Interpreter::ExecuteActionThread::
ExecuteActionThread(const std::string& hostname, int port,
                    const std::string& server_name,
                    const std::string& action_type,
                    const AbstractTreeNode& node,
                    const BT::TreeNode::Ptr& tree_node,
                    int tree_node_id) :
    _action_client(hostname, port, server_name, action_type),
    _node(node),
    _tree_node(tree_node),
    _tree_node_id(tree_node_id)
{}

Interpreter::ExecuteActionThread::

~ExecuteActionThread()
{
  auto node_ref = std::static_pointer_cast<InterpreterActionNode>(_tree_node);
  node_ref->set_exec_thread(nullptr);
}

void Interpreter::ExecuteActionThread::run()
{
    auto cb = [this](std::shared_ptr<WsClient::Connection> connection,
                     std::shared_ptr<WsClient::InMessage> in_message) {
        std::string message = in_message->string();
        rapidjson::CopyDocument document;
        document.Parse(message.c_str());
        document.Swap(document["msg"]["feedback"]);

        std::string name = document["update_field_name"].GetString();
        auto port_model = _node.model.ports.find(QString(name.c_str()))->second;
        std::string type = port_model.type_name.toStdString();

        document.Swap(document[name.c_str()]);
        setOutputValue(_tree_node.get(), name, type, document);
    };
    _action_client.registerFeedbackCallback(cb);
    auto node_ref = std::static_pointer_cast<InterpreterActionNode>(_tree_node);
    node_ref->set_exec_thread(this);

    // sleep to ensure that the topic has been successfully subscribed
    // this is required to avoid dropping messages at the beginning of the execution
    // maybe subscribe at initialization as in the remote_action node?
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    try {
      rapidjson::Document goal = getRequestFromPorts(_node, _tree_node.get());
      _action_client.sendGoal(goal);
      _action_client.waitForResult();
    }
    catch (std::exception& err) {
      emit actionReportError(err.what());
      emit actionReportResult(_tree_node_id, "IDLE");
      return;
    }

    auto result = _action_client.getResult();
    if (result.HasMember("success") &&
        result["success"].IsBool() &&
        result["success"].GetBool()) {
        emit actionReportResult(_tree_node_id, "SUCCESS");
        return;
    }
    emit actionReportResult(_tree_node_id, "FAILURE");
}

void Interpreter::ExecuteActionThread::stop()
{
    _action_client.cancelGoal();
}


//////
// Port Variables
//////

rapidjson::Value Interpreter::getInputValue(const BT::TreeNode* tree_node,
                                            const std::string name,
                                            const std::string type,
                                            rapidjson::MemoryPoolAllocator<>& allocator)
{
    rapidjson::Value jval;

    if (type.find('/') != std::string::npos) {
        // ros messages are represented as json documents
        rapidjson::CopyDocument value;
        auto res = tree_node->getInput(name, value);
        if (!res) throw std::runtime_error(res.error());
        jval.SetObject();
        jval.CopyFrom(value, allocator);
        return jval;
    }
    // all ros types defined in: http://wiki.ros.org/msg
    if (type == "bool") {
        bool value;
        auto res = tree_node->getInput(name, value);
        if (!res) throw std::runtime_error(res.error());
        jval.SetBool(value);
        return jval;
    }
    if (type == "int8" || type == "int16" || type == "int32") {
        int value;
        auto res = tree_node->getInput(name, value);
        if (!res) throw std::runtime_error(res.error());
        jval.SetInt(value);
        return jval;
    }
    if (type == "uint8" || type == "uint16" || type == "uint32") {
        unsigned int value;
        auto res = tree_node->getInput(name, value);
        if (!res) throw std::runtime_error(res.error());
        jval.SetUint(value);
        return jval;
    }
    if (type == "int64") {
        int64_t value;
        auto res = tree_node->getInput(name, value);
        if (!res) throw std::runtime_error(res.error());
        jval.SetInt64(value);
        return jval;
    }
    if (type == "uint64") {
        uint64_t value;
        auto res = tree_node->getInput(name, value);
        if (!res) throw std::runtime_error(res.error());
        jval.SetUint64(value);
        return jval;
    }
    if (type == "float32" || type == "float64") {
        double value;
        auto res = tree_node->getInput(name, value);
        if (!res) throw std::runtime_error(res.error());
        jval.SetDouble(value);
        return jval;
    }
    if (type == "string") {
        std::string value;
        auto res = tree_node->getInput(name, value);
        if (!res) throw std::runtime_error(res.error());
        jval.SetString(value.c_str(), value.size(), allocator);
        return jval;
    }
    throw std::runtime_error(fmt::format("Invalid port type: {} for {} at {}({})",
                                         type, name,
                                         tree_node->registrationName(),
                                         tree_node->name()));
}

void Interpreter::setOutputValue(BT::TreeNode* tree_node,
                                 const std::string name,
                                 const std::string type,
                                 const rapidjson::CopyDocument& document)
{
    if (type.find('/') != std::string::npos) {
        // ros messages are represented as json documents
        tree_node->setOutput<rapidjson::CopyDocument>(name, std::move(document));
        return;
    }
    // all ros types defined in: http://wiki.ros.org/msg
    if (type == "bool") {
        tree_node->setOutput<uint8_t>(name, document.GetBool());
        return;
    }
    if (type == "int8") {
        tree_node->setOutput<int8_t>(name, document.GetInt());
        return;
    }
    if (type == "int16") {
        tree_node->setOutput<int16_t>(name, document.GetInt());
        return;
    }
    if (type == "int32") {
        tree_node->setOutput<int32_t>(name, document.GetInt());
        return;
    }
    if (type == "int64") {
        tree_node->setOutput<int64_t>(name, document.GetInt64());
        return;
    }
    if (type == "uint8") {
        tree_node->setOutput<uint8_t>(name, document.GetUint());
        return;
    }
    if (type == "uint16") {
        tree_node->setOutput<uint16_t>(name, document.GetUint());
        return;
    }
    if (type == "uint32") {
        tree_node->setOutput<uint32_t>(name, document.GetUint());
        return;
    }
    if (type == "uint64") {
        tree_node->setOutput<uint64_t>(name, document.GetUint64());
        return;
    }
    if (type == "float32") {
        tree_node->setOutput<float>(name, document.GetDouble());
        return;
    }
    if (type == "float64") {
        tree_node->setOutput<double>(name, document.GetDouble());
        return;
    }
    if (type == "string") {
        tree_node->setOutput<std::string>(name, document.GetString());
        return;
    }
    throw std::runtime_error(fmt::format("Invalid port type: {} for {} at {}({})",
                                         type, name,
                                         tree_node->registrationName(),
                                         tree_node->name()));
}

rapidjson::Document Interpreter::getRequestFromPorts(const AbstractTreeNode& node,
                                                     const BT::TreeNode* tree_node)
{
    const auto* bt_node =
        dynamic_cast<const BehaviorTreeDataModel*>(node.graphic_node->nodeDataModel());
    auto port_mapping = bt_node->getCurrentPortMapping();

    rapidjson::Document goal;
    goal.SetObject();
    for(const auto& port_it: port_mapping) {
        auto port_model = node.model.ports.find(port_it.first)->second;
        std::string name = port_it.first.toStdString();
        std::string type = port_model.type_name.toStdString();
        if (port_model.direction == PortDirection::OUTPUT) {
            continue;
        }
        rapidjson::Value jname, jval;
        jname.SetString(name.c_str(), name.size(),  goal.GetAllocator());
        jval = getInputValue(tree_node, name, type, goal.GetAllocator());
        goal.AddMember(jname, jval, goal.GetAllocator());
    }
    return goal;
}
