#include "sidepanel_interpreter.h"
#include "ui_sidepanel_interpreter.h"
#include <QPushButton>
#include <QMessageBox>
#include <QDebug>

#include "mainwindow.h"
#include "utils.h"
#include "interpreter_utils.h"

SidepanelInterpreter::SidepanelInterpreter(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::SidepanelInterpreter),
    _root_status(NodeStatus::IDLE),
    _tree_name("BehaviorTree"),
    _autorun(false),
    _updated(true),
    _connected(false),
    _rbc_thread(nullptr),
    _logger_cout(nullptr),
    _parent(parent)
{
    ui->setupUi(this);
    _timer = new QTimer(this);
    _timer->setInterval(20);
    connect( _timer, &QTimer::timeout, this, &SidepanelInterpreter::runStep);
    toggleButtonAutoExecution();
    toggleButtonConnect();
}

SidepanelInterpreter::~SidepanelInterpreter()
{
    delete ui;
    if (_rbc_thread) {
        delete _rbc_thread;
    }
}

void SidepanelInterpreter::clear()
{
  if (_timer) {
    _timer->stop();
  }
}

void SidepanelInterpreter::on_Connect()
{
    if( !_connected) {
        if (_rbc_thread && _rbc_thread->isRunning()) {
            qDebug() << "still connecting...";
            return;
        }

        QString hostname = ui->lineEdit->text();
        if( hostname.isEmpty() ) {
            hostname = ui->lineEdit->placeholderText();
            ui->lineEdit->setText(hostname);
        }
        QString port = ui->lineEdit_port->text();
        if( port.isEmpty() ) {
            port = ui->lineEdit_port->placeholderText();
            ui->lineEdit_port->setText(port);
        }
        ConnectBridge();
        return;
    }

    DisconnectBridge();
    DisconnectNodes(true);
    _connected = false;
    toggleButtonConnect();
}

void SidepanelInterpreter::ConnectBridge()
{
    std::string host = ui->lineEdit->text().toStdString();
    std::string port = ui->lineEdit_port->text().toStdString();

    _rbc_thread = new Interpreter::RosBridgeConnectionThread(host, port);
    connect( _rbc_thread, &Interpreter::RosBridgeConnectionThread::connectionCreated,
             this, &SidepanelInterpreter::on_connectionCreated);
    connect( _rbc_thread, &Interpreter::RosBridgeConnectionThread::connectionError,
             this, &SidepanelInterpreter::on_connectionError);
    connect( _rbc_thread, &Interpreter::RosBridgeConnectionThread::actionThreadCreated,
             this, &SidepanelInterpreter::on_actionThreadCreated);

    _rbc_thread->start();
}

void SidepanelInterpreter::DisconnectBridge()
{
    if (!_rbc_thread) {
        return;
    }

    disconnect( _rbc_thread, &Interpreter::RosBridgeConnectionThread::connectionCreated,
                this, &SidepanelInterpreter::on_connectionCreated);
    disconnect( _rbc_thread, &Interpreter::RosBridgeConnectionThread::connectionError,
                this, &SidepanelInterpreter::on_connectionError);
    _rbc_thread->stop();
}

void SidepanelInterpreter::DisconnectNodes(bool report_result)
{
    if (_rbc_thread) {
        _rbc_thread->clearSubscribers();
    }
    for (auto exec_thread: _running_threads) {
        if (!report_result) {
            disconnect( exec_thread, &Interpreter::ExecuteActionThread::actionReportResult,
                        this, &SidepanelInterpreter::on_actionReportResult);
        }
        exec_thread->stop();
    }
    for (auto tree_node: _tree.nodes) {
        auto node_ref = std::dynamic_pointer_cast<Interpreter::InterpreterNodeBase>(tree_node);
        if (node_ref) {
            node_ref->disconnect();
        }
    }
    for (auto action_capture: _connected_actions) {
        action_capture.second->action_client = nullptr;
    }
    _connected_actions.clear();
    _connected_services.clear();
    _autorun = false;
    toggleButtonAutoExecution();
}

std::shared_ptr<roseus_bt::RosbridgeActionClient> SidepanelInterpreter::
registerAction(std::string server_name, PortModels ports, BT::TreeNode::Ptr tree_node)
{
    if (!_connected) {
        throw std::runtime_error(std::string("Not connected"));
    }
    if (_connected_actions.count(server_name)) {
        // update tree_node capture
        _connected_actions[server_name]->tree_node = tree_node;
        return _connected_actions[server_name]->action_client;
    }

    std::string topic_type = getActionType(server_name);
    if (topic_type.empty()) {
        throw std::runtime_error(std::string("Could not connect to action server at ") + server_name);
    }

    std::string host = ui->lineEdit->text().toStdString();
    int port = ui->lineEdit_port->text().toInt();
    auto action_client = std::make_shared<roseus_bt::RosbridgeActionClient>(host, port, server_name, topic_type);
    auto action_capture = std::make_shared<Interpreter::RosbridgeActionClientCapture>();
    action_capture->action_client = action_client;
    action_capture->tree_node = tree_node;
    action_capture->ports = ports;

    auto cb = [action_capture](std::shared_ptr<WsClient::Connection> connection,
                               std::shared_ptr<WsClient::InMessage> in_message) {
        std::string message = in_message->string();
        rapidjson::CopyDocument document;
        document.Parse(message.c_str());
        document.Swap(document["msg"]["feedback"]);

        std::string name = document["update_field_name"].GetString();
        auto port_model = action_capture->ports.find(QString(name.c_str()))->second;
        std::string type = port_model.type_name.toStdString();

        document.Swap(document[name.c_str()]);
        Interpreter::setOutputValue(action_capture->tree_node, name, type, document);
    };

    action_client->registerFeedbackCallback(cb);

    // sleep to ensure that the topic has been successfully subscribed
    // this is required to avoid dropping messages at the beginning of the execution
    // maybe subscribe at initialization as in the remote_action node?
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    _connected_actions[server_name] = action_capture;
    return action_client;
}

std::shared_ptr<roseus_bt::RosbridgeServiceClient> SidepanelInterpreter::
registerService(std::string service_name)
{
    if (!_connected) {
        throw std::runtime_error(std::string("Not connected"));
    }
    if (_connected_services.count(service_name)) {
        return _connected_services[service_name];
    }

    std::string host = ui->lineEdit->text().toStdString();
    int port = ui->lineEdit_port->text().toInt();
    auto service_client = std::make_shared<roseus_bt::RosbridgeServiceClient>(host, port, service_name);
    _connected_services[service_name] = service_client;
    return service_client;
}

void SidepanelInterpreter::registerSubscriber(std::string message_type,
                                              BT::TreeNode::Ptr tree_node)
{
    if (!(_connected && _rbc_thread)) {
        throw std::runtime_error(std::string("Not connected"));
    }
    _rbc_thread->registerSubscriber(message_type, tree_node);
}

void SidepanelInterpreter::registerActionThread(int tree_node_id)
{
    if (!(_connected && _rbc_thread)) {
        throw std::runtime_error(std::string("Not connected"));
    }
    _rbc_thread->registerActionThread(tree_node_id);
}

void SidepanelInterpreter::setTree(const QString& bt_name, const QString& xml_filename)
{
    qDebug() << "Updating interpreter_widget tree model";

    _tree_name = bt_name;

    // cleanup connections
    DisconnectNodes(false);

    // clear background nodes
    _background_nodes.clear();

    auto main_win = dynamic_cast<MainWindow*>( _parent );
    updateTree();
    if (!_abstract_tree.rootNode()) {
        // too early; initialization has not finished yet
        return;
    }

    BT::BehaviorTreeFactory factory;
    Interpreter::RegisterInterpreterNode<Interpreter::InterpreterNode>(factory, "Root", {}, this);

    // register nodes
    for (auto& tab: main_win->getTabInfo()) {
        AbsBehaviorTree abs_tree = BuildTreeFromScene( tab.second->scene() );
        for (auto& node: abs_tree.nodes()) {
            std::string registration_ID = node.model.registration_ID.toStdString();
            BT::PortsList ports;
            for (auto& it: node.model.ports) {
                ports.insert( {it.first.toStdString(), BT::PortInfo(it.second.direction)} );
            }
            try {
                if (node.model.type == roseus_bt::NodeType::CONDITION ||
                    node.model.type == roseus_bt::NodeType::REMOTE_CONDITION) {
                    Interpreter::RegisterInterpreterNode<Interpreter::InterpreterConditionNode>
                        (factory, registration_ID, ports, this);
                }
                else if (node.model.type == roseus_bt::NodeType::ACTION ||
                         node.model.type == roseus_bt::NodeType::REMOTE_ACTION) {
                    Interpreter::RegisterInterpreterNode<Interpreter::InterpreterActionNode>
                        (factory, registration_ID, ports, this);
                }
                else if (node.model.type == roseus_bt::NodeType::SUBSCRIBER ||
                         node.model.type == roseus_bt::NodeType::REMOTE_SUBSCRIBER) {
                    Interpreter::RegisterInterpreterNode<Interpreter::InterpreterSubscriberNode>
                        (factory, registration_ID, ports, this);
                }
                else {
                    Interpreter::RegisterInterpreterNode<Interpreter::InterpreterNode>
                        (factory, registration_ID, ports, this);
                }
            }
            catch(BT::BehaviorTreeException err) {
                // Duplicated node
                // qDebug() << err.what();
            }
        }
    }

    if (xml_filename.isNull()) {
        QString xml_text = main_win->saveToXML(bt_name);
        _tree = factory.createTreeFromText(xml_text.toStdString());
    }
    else {
        _tree = factory.createTreeFromFile(xml_filename.toStdString());
    }

    _logger_cout.reset();
    _logger_cout = std::make_unique<BT::StdCoutLogger>(_tree);

    _updated = true;
    _timer->start();
}

void SidepanelInterpreter::setTree(const QString& bt_name)
{
    setTree(bt_name, QString::null);
}

void SidepanelInterpreter::updateTree()
{
    auto main_win = dynamic_cast<MainWindow*>( _parent );
    auto container = main_win->getTabByName(_tree_name);
    _abstract_tree = BuildTreeFromScene( container->scene() );
}

void SidepanelInterpreter::
translateNodeIndex(std::vector<std::pair<int, NodeStatus>>& node_status,
                   bool tree_index)
{
    // translate _tree node indexes into _abstract_tree indexes
    // if tree_index is false, translate _abstract_tree into _tree indexes

    // CAREFUL! this can recycle the _abstract_tree,
    // nullifying ongoing iterators and node pointers

    if (node_status.size() == 0) {
        return;
    }

    auto check_range = [node_status](int min, int size) {
        for (auto& it: node_status) {
            if (min < it.first && min+size >= it.first) {
                return true;
            }
        }
        return false;
    };

    auto update_range = [&node_status, tree_index](int min, int size) {
        for (auto& it: node_status) {
            if (tree_index && min+size < it.first) {
                it.first -= size;
            }
            if (!tree_index && min < it.first) {
                it.first += size;
            }
        }
    };

    int offset=0;
    int last_change_index = std::max_element(node_status.begin(), node_status.end())->first;
    auto main_win = dynamic_cast<MainWindow*>( _parent );
    auto container = main_win->getTabByName(_tree_name);

    for (int i=0; i<last_change_index; i++) {
        auto node = _abstract_tree.nodes().at(i);
        auto subtree = dynamic_cast< SubtreeNodeModel*>( node.graphic_node->nodeDataModel() );
        if (subtree && !subtree->expanded())
        {
            main_win->onRequestSubTreeExpand(*container, *node.graphic_node);
            auto subtree_nodes = container->getSubtreeNodesRecursively(*(node.graphic_node));
            int subtree_size = subtree_nodes.size() - 1;  // don't count subtree root
            if (!tree_index || !check_range(i, subtree_size))
            {
                // fold back subtree and update indexes
                main_win->onRequestSubTreeExpand(*container, *node.graphic_node);
                update_range(i+offset, subtree_size);
                if (tree_index) {
                    last_change_index -= subtree_size;
                }
                else {
                    offset += subtree_size;
                }
            }
        }
    }
}

int SidepanelInterpreter::
translateSingleNodeIndex(int node_index, bool tree_index)
{
    // translate a single _tree node index into _abstract_tree index
    // if tree_index is false, translate a _abstract_tree into _tree index

    std::vector<std::pair<int, NodeStatus>> node_status;
    node_status.push_back( {node_index, NodeStatus::IDLE} );
    translateNodeIndex(node_status, tree_index);
    return node_status.front().first;
}

void SidepanelInterpreter::
expandAndChangeNodeStyle(std::vector<std::pair<int, NodeStatus>> node_status,
                         bool reset_before_update)
{
    if (node_status.size() == 0) {
        return;
    }

    translateNodeIndex(node_status, true);
    emit changeNodeStyle(_tree_name, node_status, reset_before_update);
}

void SidepanelInterpreter::changeSelectedStyle(const NodeStatus& status)
{
    if (_tree.nodes.size() <= 1) {
        return;
    }

    std::vector<std::pair<int, NodeStatus>> node_status;
    int i = 0;
    for (auto& node: _abstract_tree.nodes()) {
        if (node.graphic_node->nodeGraphicsObject().isSelected()) {
            node_status.push_back( {i, status} );
        }
        i++;
    }
    emit changeNodeStyle(_tree_name, node_status, true);
    translateNodeIndex(node_status, false);
    for (auto it: node_status) {
        auto tree_node = _tree.nodes.at(it.first - 1);  // skip root
        changeTreeNodeStatus(tree_node, it.second);
    }
    _updated = true;
}

void SidepanelInterpreter::changeRunningStyle(const NodeStatus& status)
{
    if (_tree.nodes.size() <= 1) {
        return;
    }
    std::vector<std::pair<int, NodeStatus>> node_status;

    int i = 1;  // skip root
    for (auto& tree_node: _tree.nodes) {
        if (tree_node->status() == NodeStatus::RUNNING &&
            std::find(_background_nodes.begin(), _background_nodes.end(),
                      tree_node) == _background_nodes.end()) {
            changeTreeNodeStatus(tree_node, status);
            node_status.push_back( {i, status} );
        }
        i++;
    }
    expandAndChangeNodeStyle(node_status, true);
    _updated = true;
}

void SidepanelInterpreter::changeTreeNodeStatus(BT::TreeNode::Ptr node,
                                                const NodeStatus& status)
{
    if (node->type() == NodeType::CONDITION) {
        auto node_ref = std::static_pointer_cast<Interpreter::InterpreterConditionNode>(node);
        node_ref->set_status(status);
        return;
    }
    auto node_ref = std::static_pointer_cast<Interpreter::InterpreterNode>(node);
    node_ref->set_status(status);
}

std::string SidepanelInterpreter::getActionType(const std::string& server_name)
{
    std::string topic_name = server_name + "/goal";
    roseus_bt::RosbridgeServiceClient service_client(ui->lineEdit->text().toStdString(),
                                                     ui->lineEdit_port->text().toInt(),
                                                     "/rosapi/topic_type");
    rapidjson::Document request;
    request.SetObject();
    rapidjson::Value topic;
    topic.SetString(topic_name.c_str(), topic_name.size(), request.GetAllocator());
    request.AddMember("topic", topic, request.GetAllocator());
    service_client.call(request);
    service_client.waitForResult();
    auto result = service_client.getResult();
    if (result.HasMember("type") &&
        result["type"].IsString()) {
        std::string topic_type = result["type"].GetString();
        if (topic_type.size() <= 4) {
            // Invalid action type
            return "";
        }
        if (topic_type.substr(topic_type.size() - 4) == "Goal") {
            return topic_type.substr(0, topic_type.size() - 4);
        }
    }
    return "";
}

AbstractTreeNode SidepanelInterpreter::getAbstractNode(int tree_node_id)
{
    int node_id = translateSingleNodeIndex(tree_node_id, true);
    return _abstract_tree.nodes().at(node_id);
}

BT::TreeNode::Ptr SidepanelInterpreter::getSharedNode(const BT::TreeNode* node)
{
    for (auto tree_node : _tree.nodes) {
        if (tree_node.get() == node) {
            return(tree_node);
        }
    }
    return nullptr;
}

int SidepanelInterpreter::getNodeId(const BT::TreeNode* node)
{
    int i=1;  // skip root
    for (auto tree_node : _tree.nodes) {
        if (tree_node.get() == node) {
            return(i);
        }
        i++;
    }
    throw std::runtime_error(std::string("Could not find node " + node->name() + " in tree"));
}

void SidepanelInterpreter::connectNode(const int tree_node_id)
{
    if (tree_node_id <= 0) {
        return;
    }
    auto bt_node = _tree.nodes.at(tree_node_id - 1);
    auto node_ref = std::dynamic_pointer_cast<Interpreter::InterpreterNodeBase>(bt_node);
    if (node_ref) {
        node_ref->connect(tree_node_id);
    }
}

void SidepanelInterpreter::connectNode(const BT::TreeNode* node)
{
    int tree_node_id = getNodeId(node);
    connectNode(tree_node_id);
}

void SidepanelInterpreter::executeNode(const int tree_node_id)
{
    int node_id = translateSingleNodeIndex(tree_node_id, true);
    auto bt_node = _tree.nodes.at(tree_node_id - 1);
    std::vector<std::pair<int, NodeStatus>> node_status;

    if (bt_node->type() != BT::NodeType::ACTION &&
        bt_node->type() != BT::NodeType::CONDITION) {
        /* decorators, control, subtrees */
        return;
    }

    if (auto node_ref = std::dynamic_pointer_cast<Interpreter::InterpreterNodeBase>(bt_node)) {
        node_status.push_back( {node_id, node_ref->executeNode()} );
    }
    else {
        // builtin action nodes, such as SetBlackboard
        node_status.push_back( {node_id, bt_node->executeTick()} );
    }

    emit changeNodeStyle(_tree_name, node_status, true);
    translateNodeIndex(node_status, false);
    for (auto it: node_status) {
        auto tree_node = _tree.nodes.at(it.first - 1);  // skip root
        changeTreeNodeStatus(tree_node, it.second);
    }
}

void SidepanelInterpreter::tickRoot()
{
    if (_tree.nodes.size() <= 1) {
        return;
    }

    std::vector<std::pair<int, NodeStatus>> prev_node_status;
    std::vector<std::pair<int, NodeStatus>> node_status;
    int i;

    // clear background nodes
    _background_nodes.clear();

    // set previous status
    prev_node_status.push_back( {0, _root_status} );
    i = 1;
    for (auto& node: _tree.nodes) {
        prev_node_status.push_back( {i, node->status()} );
        i++;
    }

    bool conditionRunning = false;
    // tick tree
    try {
        _root_status = _tree.tickRoot();
    }
    catch (Interpreter::ConditionEvaluation c_eval) {
        conditionRunning = true;
    }

    if (_root_status != NodeStatus::RUNNING) {
        // stop evaluations until the next change
        _updated = false;
        if (_autorun) {
            on_buttonDisableAutoExecution_clicked();
        }
    }

    if (_tree.nodes.size() == 1 && _tree.rootNode()->name() == "Root") {
        return;
    }

    // set changed status
    if (_root_status != prev_node_status.at(0).second) {
        node_status.push_back( {0, _root_status} );
    }

    i = 1;
    for (auto& node: _tree.nodes) {
        NodeStatus new_status = node->status();
        NodeStatus prev_status = prev_node_status.at(i).second;

        if (new_status != prev_status) {
            if (new_status == NodeStatus::IDLE) {
                // pushing the previous status allows to display grayed-out
                // colors when the node is set to IDLE (#72)
                node_status.push_back( {i, prev_status} );
            }
            node_status.push_back( {i, node->status()} );
        }
        else {
            if (new_status == NodeStatus::RUNNING &&
                node->type() != NodeType::CONDITION) {
                // force update
                node_status.push_back( {i, node->status()} );
                // artificially gray-out running nodes
                if (conditionRunning) {
                    _background_nodes.push_back(node);
                    node_status.push_back( {i, NodeStatus::IDLE} );
                }
            }
        }
        i++;
    }

    expandAndChangeNodeStyle(node_status, false);
}

void SidepanelInterpreter::runStep()
{
    if (_updated || _autorun) {
        try {
            tickRoot();
            _updated = false;
        }
        catch (std::exception& err) {
            _timer->stop();
            reportError("Error during auto callback", err.what());
        }
    }
}

void SidepanelInterpreter::reportError(const QString& title, const QString& message)
{
    on_buttonDisableAutoExecution_clicked();
    QMessageBox messageBox;
    messageBox.critical(this, title, message);
    messageBox.show();
}

void SidepanelInterpreter::toggleButtonAutoExecution()
{
    ui->buttonDisableAutoExecution->setEnabled(_connected && _autorun);
    ui->buttonEnableAutoExecution->setEnabled(_connected && !_autorun);
}

void SidepanelInterpreter::toggleButtonConnect()
{
    connectionUpdate(_connected);
    ui->lineEdit->setDisabled(_connected);
    ui->lineEdit_port->setDisabled(_connected);
    ui->buttonExecSelection->setEnabled(_connected);
    ui->buttonExecRunning->setEnabled(_connected);
    toggleButtonAutoExecution();
}

void SidepanelInterpreter::on_connectionCreated()
{
    _connected = true;
    toggleButtonConnect();
}

void SidepanelInterpreter::on_connectionError(const QString& message)
{
    // close connection
    DisconnectNodes(true);
    _connected = false;
    toggleButtonConnect();

    // display error message
    reportError("Connection Error", message);
}

void SidepanelInterpreter::on_actionThreadCreated(int tree_node_id)
{
    BT::TreeNode::Ptr tree_node = _tree.nodes.at(tree_node_id - 1);
    auto node_ref = std::static_pointer_cast<Interpreter::InterpreterActionNode>(tree_node);

    auto exec_thread = new Interpreter::ExecuteActionThread(node_ref);

    connect( exec_thread, &Interpreter::ExecuteActionThread::actionReportResult,
             this, &SidepanelInterpreter::on_actionReportResult);
    connect( exec_thread, &Interpreter::ExecuteActionThread::actionReportError,
             this, [this] (const QString& message) { reportError("Error Executing Node", message); });
    connect( exec_thread, &Interpreter::ExecuteActionThread::finished,
             this, &SidepanelInterpreter::on_actionFinished);
    connect( exec_thread, &Interpreter::ExecuteActionThread::finished,
             exec_thread, &QObject::deleteLater);

    _running_threads.push_back(exec_thread);
    exec_thread->start();
}

void SidepanelInterpreter::on_actionReportResult(int tree_node_id, const QString& status)
{
    qDebug() << "actionReportResult: " << status;
    NodeStatus bt_status = BT::convertFromString<NodeStatus>(status.toStdString());
    std::vector<std::pair<int, NodeStatus>> node_status;
    node_status.push_back( {tree_node_id, bt_status} );
    expandAndChangeNodeStyle(node_status, false);
    auto tree_node = _tree.nodes.at(tree_node_id - 1);
    changeTreeNodeStatus(tree_node, bt_status);
    _updated = true;
}

void SidepanelInterpreter::on_actionFinished()
{
    auto is_finished = [](const Interpreter::ExecuteActionThread* thr) {
        return thr->isFinished();
    };

    _running_threads.erase(std::remove_if(_running_threads.begin(),
                                          _running_threads.end(),
                                          is_finished),
                           _running_threads.end());
}

void SidepanelInterpreter::on_buttonResetTree_clicked()
{
    // disable auto_execution and stop running nodes
    on_buttonHaltTree_clicked();
    // reset the tree model
    auto main_win = dynamic_cast<MainWindow*>( _parent );
    setTree(_tree_name);
    main_win->resetTreeStyle(_abstract_tree);
    _updated = true;
}

void SidepanelInterpreter::on_buttonSetSuccess_clicked()
{
    qDebug() << "buttonSetSuccess";
    changeSelectedStyle(NodeStatus::SUCCESS);
}

void SidepanelInterpreter::on_buttonSetFailure_clicked()
{
    qDebug() << "buttonSetFailure";
    changeSelectedStyle(NodeStatus::FAILURE);
}

void SidepanelInterpreter::on_buttonSetIdle_clicked()
{
    qDebug() << "buttonSetIdle";
    changeSelectedStyle(NodeStatus::IDLE);
}

void SidepanelInterpreter::on_buttonSetRunningSuccess_clicked()
{
    qDebug() << "buttonSetRunningSuccess";
    changeRunningStyle(NodeStatus::SUCCESS);
}

void SidepanelInterpreter::on_buttonSetRunningFailure_clicked()
{
    qDebug() << "buttonSetRunningFailure";
    changeRunningStyle(NodeStatus::FAILURE);
}

void SidepanelInterpreter::on_buttonEnableAutoExecution_clicked()
{
    _timer->stop();

    _autorun = true;
    _updated = true;
    toggleButtonAutoExecution();

    // set execution mode
    for (auto tree_node: _tree.nodes) {
        auto node_ref = std::dynamic_pointer_cast<Interpreter::InterpreterNodeBase>(tree_node);
        if (node_ref) {
            node_ref->set_execution_mode(true);
        }
    }

    // execute running to start
    on_buttonExecRunning_clicked();

    _timer->start();
}

void SidepanelInterpreter::on_buttonDisableAutoExecution_clicked()
{
    _autorun = false;
    toggleButtonAutoExecution();

    // set execution mode
    for (auto tree_node: _tree.nodes) {
        auto node_ref = std::dynamic_pointer_cast<Interpreter::InterpreterNodeBase>(tree_node);
        if (node_ref) {
            node_ref->set_execution_mode(false);
        }
    }
}

void SidepanelInterpreter::on_buttonHaltTree_clicked()
{
    qDebug() << "buttonHaltTree";

    if (_tree.nodes.size() <= 1) {
        return;
    }

    on_buttonDisableAutoExecution_clicked();
    // directly stop running actions instead of halting the root
    // to avoid setting the state of other nodes to IDLE
    for (auto exec_thread: _running_threads) {
        exec_thread->stop();
    }
    _updated = true;
}

void SidepanelInterpreter::on_buttonExecSelection_clicked()
{
    qDebug() << "buttonExecSelection";

    if (_tree.nodes.size() <= 1) {
        return;
    }
    std::vector<std::pair<int, NodeStatus>> node_status;

    int i = 0;
    for (auto& node: _abstract_tree.nodes()) {
      if (node.graphic_node->nodeGraphicsObject().isSelected()) {
          node_status.push_back( {i, NodeStatus::RUNNING} );
      }
      i++;
    }

    translateNodeIndex(node_status, false);
    try {
        for (auto it: node_status) {
            executeNode(it.first);
        }
    }
    catch (std::exception& err) {
        reportError("Error Executing Node", err.what() );
    }
    _updated = true;
}

void SidepanelInterpreter::on_buttonExecRunning_clicked()
{
    qDebug() << "buttonExecRunning";

    if (_tree.nodes.size() <= 1) {
        return;
    }
    std::vector<std::pair<int, NodeStatus>> node_status;

    int i = 1;  // skip root
    for (auto& tree_node: _tree.nodes) {
        if (tree_node->status() == NodeStatus::RUNNING &&
            std::find(_background_nodes.begin(), _background_nodes.end(),
                      tree_node) == _background_nodes.end()) {
            node_status.push_back( {i, NodeStatus::RUNNING} );
        }
        i++;
    }

    try {
        for (auto it: node_status) {
            executeNode(it.first);
        }
    }
    catch (std::exception& err) {
        reportError("Error Executing Node", err.what() );
    }
    _updated = true;
}

void SidepanelInterpreter::on_buttonShowBlackboard_clicked()
{
    qDebug() << "buttonShowBlackboard";

    std::stringstream ss;
    if (_tree.nodes.size() > 1) {
        auto blackboard = _tree.rootBlackboard();
        std::vector<BT::StringView> keys = blackboard->getKeys();
        std::sort(keys.begin(), keys.end(),
                  [](BT::StringView a, BT::StringView b) { return a<b; });

        for (const auto key: keys) {
            std::string key_str(key);
            const BT::Any value = *blackboard->getAny(key_str);
            if (!value.empty() && !value.isNumber() && !value.isString()) {
                // json document
                ss << key_str << ":\n" << value << "\n";
            }
            else {
                ss << key_str << ": " << value << "\n";
            }
        }
    }

    QMessageBox messageBox;
    messageBox.information(this,"Blackboard Variables",
                           ss.str().empty()? "No variables yet" : ss.str().c_str());
    messageBox.show();
}
