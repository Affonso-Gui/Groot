#include "interpreter_utils.h"
#include "sidepanel_interpreter.h"

using WsClient = SimpleWeb::SocketClient<SimpleWeb::WS>;

//////
// InterpreterNodeBase
//////

Interpreter::InterpreterNodeBase::
InterpreterNodeBase(SidepanelInterpreter* parent) :
    _parent(parent), _server_name(""), _execution_mode(false)
{}

void Interpreter::InterpreterNodeBase::set_execution_mode(bool execution_mode)
{
    _execution_mode = execution_mode;
}


//////
// InterpreterNode
//////

Interpreter::InterpreterNode::
InterpreterNode(SidepanelInterpreter* parent,
                const std::string& name,
                const BT::NodeConfiguration& config) :
    BT::AsyncActionNode(name,config), InterpreterNodeBase(parent)
{}

void Interpreter::InterpreterNode::halt()
{}

BT::NodeStatus Interpreter::InterpreterNode::tick()
{
    if (_execution_mode) {
        return executeNode();
    }
    return BT::NodeStatus::RUNNING;
}

BT::NodeStatus Interpreter::InterpreterNode::executeNode()
{
    _parent->connectNode(this);
    return BT::NodeStatus::RUNNING;
}

void Interpreter::InterpreterNode::
connect(int tree_node_id)
{
    _tree_node_id = tree_node_id;
}

void Interpreter::InterpreterNode::disconnect()
{
    _execution_mode = false;
}

void Interpreter::InterpreterNode::set_status(const BT::NodeStatus& status)
{
    setStatus(status);
}


//////
// InterpreterActionNode
//////

Interpreter::InterpreterActionNode::
InterpreterActionNode(SidepanelInterpreter* parent,
                      const std::string& name,
                      const BT::NodeConfiguration& config) :
    InterpreterNode(parent, name, config),
    _exec_thread(nullptr)
{}

Interpreter::InterpreterActionNode::
~InterpreterActionNode()
{
    disconnect();
}

void Interpreter::InterpreterActionNode::halt()
{
    if (isRunning()) {
        _exec_thread->stop();
    }
    _exec_thread = nullptr;
}

BT::NodeStatus Interpreter::InterpreterActionNode::executeNode()
{
    _parent->connectNode(this);
    int tree_node_id = _parent->getNodeId(this);
    _parent->registerActionThread(tree_node_id);
    return NodeStatus::RUNNING;
}

void Interpreter::InterpreterActionNode::
connect(int tree_node_id)
{
    _tree_node_id = tree_node_id;

    AbstractTreeNode node = _parent->getAbstractNode(tree_node_id);
    PortModels ports = node.model.ports;

    if (_server_name.empty()) {
        std::pair<PortsMapping, PortModels> ports = getPorts(node);
        std::string name = node.model.ports.find("server_name")->second.default_value.toStdString();
        if (name.front() != '/') {
            name = '/' + name;
        }
        _server_name = name;
        _port_mapping = ports.first;
        _ports = ports.second;

    }

    BT::TreeNode::Ptr shared_node = _parent->getSharedNode(this);
    _action_client = _parent->registerAction(_server_name, ports, shared_node);
}

void Interpreter::InterpreterActionNode::disconnect()
{
    _execution_mode = false;
    _action_client = nullptr;
}

bool Interpreter::InterpreterActionNode::isRunning()
{
    return (_exec_thread && _exec_thread->isRunning());
}


//////
// InterpreterSubscriberNode
//////

Interpreter::InterpreterSubscriberNode::
InterpreterSubscriberNode(SidepanelInterpreter* parent,
                          const std::string& name,
                          const BT::NodeConfiguration& config) :
    InterpreterNode(parent,name,config)
{}

BT::NodeStatus Interpreter::InterpreterSubscriberNode::executeNode()
{
    _parent->connectNode(this);
    AbstractTreeNode node = _parent->getAbstractNode(_tree_node_id);
    BT::TreeNode::Ptr shared_node = _parent->getSharedNode(this);
    std::string message_type = node.model.ports.find("message_type")->second.default_value.toStdString();
    _parent->registerSubscriber(message_type, shared_node);
    return NodeStatus::SUCCESS;
}


//////
// InterpreterConditionNode
//////

Interpreter::InterpreterConditionNode::
InterpreterConditionNode(SidepanelInterpreter* parent,
                         const std::string& name,
                         const BT::NodeConfiguration& config) :
    BT::ConditionNode(name,config),
    InterpreterNodeBase(parent),
    _return_status(BT::NodeStatus::IDLE)
{}

Interpreter::InterpreterConditionNode::
~InterpreterConditionNode()
{
    disconnect();
}

BT::NodeStatus Interpreter::InterpreterConditionNode::tick()
{
    if (_execution_mode) {
      return executeNode();
    }
    return _return_status;
}

BT::NodeStatus Interpreter::InterpreterConditionNode::executeTick()
{
    const NodeStatus status = tick();
    if (status == NodeStatus::IDLE && !_execution_mode) {
        setStatus(NodeStatus::RUNNING);
        throw ConditionEvaluation();
    }
    _return_status = NodeStatus::IDLE;
    setStatus(status);
    BT::Tree::transversed_nodes.push_back(this);
    return status;
}

BT::NodeStatus Interpreter::InterpreterConditionNode::executeNode()
{
    _parent->connectNode(this);
    AbstractTreeNode node = _parent->getAbstractNode(_tree_node_id);
    BT::TreeNode::Ptr shared_node = _parent->getSharedNode(this);
    rapidjson::Document request = getRequestFromPorts(node, shared_node);
    _service_client->call(request);
    _service_client->waitForResult();
    auto result = _service_client->getResult();
    if (result.HasMember("success") &&
        result["success"].IsBool() &&
        result["success"].GetBool()) {
        return NodeStatus::SUCCESS;
    }
    return NodeStatus::FAILURE;
}

void Interpreter::InterpreterConditionNode::
connect(int tree_node_id)
{
    _tree_node_id = tree_node_id;

    if (_server_name.empty()) {
        AbstractTreeNode node = _parent->getAbstractNode(tree_node_id);
        std::string name = node.model.ports.find("service_name")->second.default_value.toStdString();
        if (name.front() != '/') {
            name = '/' + name;
        }
        _server_name = name;
    }

    _service_client = _parent->registerService(_server_name);
}

void Interpreter::InterpreterConditionNode::disconnect()
{
    _execution_mode = false;
    _service_client = nullptr;
}

void Interpreter::InterpreterConditionNode::set_status(const BT::NodeStatus& status)
{
    _return_status = status;
    setStatus(status);
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

void Interpreter::RosBridgeConnectionThread::registerSubscriber(std::string topic_type,
                                                                BT::TreeNode::Ptr tree_node)
{
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

        setOutputValue(tree_node, "output_port", topic_type, document);
        tree_node->setOutput<uint8_t>("received_port", true);
    };
    _subscribers.push_back(topic_name);
    _rbc.addClient(topic_name);
    _rbc.subscribe(topic_name, topic_name, cb,  "", topic_type);
}

void Interpreter::RosBridgeConnectionThread::registerActionThread(int tree_node_id)
{
    emit actionThreadCreated(tree_node_id);
}


//////
// ExecuteActionThread
//////


Interpreter::ExecuteActionThread::
ExecuteActionThread(std::shared_ptr<InterpreterActionNode> tree_node) :
    _tree_node(tree_node)
{}

void Interpreter::ExecuteActionThread::run()
{
    _tree_node->_exec_thread = this;
    int tree_node_id = _tree_node->_tree_node_id;
    rapidjson::Value result;

    try {
      rapidjson::Document goal = getRequestFromPorts(_tree_node,
                                                     _tree_node->_port_mapping,
                                                     _tree_node->_ports);
      _tree_node->_action_client->sendGoal(goal);
      _tree_node->_action_client->waitForResult();
    }
    catch (std::exception& err) {
      emit actionReportError(err.what());
      emit actionReportResult(tree_node_id, "IDLE");
      _tree_node->_exec_thread = nullptr;
      return;
    }

    if (!(_tree_node->_action_client)) {
      // disconnected
      return;
    }

    try {
      result = _tree_node->_action_client->getResult();
    }
    catch (BT::RuntimeError& err) {
      // timed out
      emit actionReportResult(tree_node_id, "FAILURE");
      return;
    }

    if (result.HasMember("success") &&
        result["success"].IsBool() &&
        result["success"].GetBool()) {
        emit actionReportResult(tree_node_id, "SUCCESS");
        _tree_node->_exec_thread = nullptr;
        return;
    }
    emit actionReportResult(tree_node_id, "FAILURE");

    // TODO: administer _exec_thread by shared pointer instead
    _tree_node->_exec_thread = nullptr;
}

void Interpreter::ExecuteActionThread::stop()
{
    // wait for up to 0.5s before forcing disconnection
    _tree_node->_action_client->cancelGoal(500);
}


//////
// Port Variables
//////

std::pair<PortsMapping, PortModels> Interpreter::getPorts(AbstractTreeNode node)
{
    const auto* bt_node =
        dynamic_cast<const BehaviorTreeDataModel*>(node.graphic_node->nodeDataModel());
    auto port_mapping = bt_node->getCurrentPortMapping();
    auto ports = node.model.ports;
    return {port_mapping, ports};
}

rapidjson::Value Interpreter::getInputValue(BT::TreeNode::Ptr tree_node,
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

void Interpreter::setOutputValue(BT::TreeNode::Ptr tree_node,
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

rapidjson::Document Interpreter::getRequestFromPorts(BT::TreeNode::Ptr tree_node,
                                                     PortsMapping port_mapping,
                                                     PortModels ports)
{
    rapidjson::Document goal;
    goal.SetObject();
    for(const auto& port_it: port_mapping) {
        auto port_model = ports.find(port_it.first)->second;
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

rapidjson::Document Interpreter::getRequestFromPorts(const AbstractTreeNode& node,
                                                     BT::TreeNode::Ptr tree_node)
{
    auto ports = getPorts(node);
    return getRequestFromPorts(tree_node, ports.first, ports.second);
}
