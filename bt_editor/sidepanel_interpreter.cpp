#include "sidepanel_interpreter.h"
#include "ui_sidepanel_interpreter.h"
#include <behaviortree_cpp_v3/loggers/bt_cout_logger.h>
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
    _autorun(true),
    _updated(true),
    _connected(false),
    _rbc_thread(nullptr),
    _parent(parent)
{
    ui->setupUi(this);
    _timer = new QTimer(this);
    connect( _timer, &QTimer::timeout, this, &SidepanelInterpreter::runStep);
    toggleButtonAutoExecution();
    toggleButtonConnect();
}

SidepanelInterpreter::~SidepanelInterpreter()
{
    delete ui;
}

void SidepanelInterpreter::clear()
{
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

        _rbc_thread = new Interpreter::RosBridgeConnectionThread(hostname.toStdString(),
                                                                 port.toStdString());
        connect( _rbc_thread, &Interpreter::RosBridgeConnectionThread::connectionCreated,
                 this, &SidepanelInterpreter::on_connectionCreated);
        connect( _rbc_thread, &Interpreter::RosBridgeConnectionThread::connectionError,
                 this, &SidepanelInterpreter::on_connectionError);
        _rbc_thread->start();
        return;
    }

    if (_rbc_thread) {
        disconnect( _rbc_thread, &Interpreter::RosBridgeConnectionThread::connectionCreated,
                    this, &SidepanelInterpreter::on_connectionCreated);
        disconnect( _rbc_thread, &Interpreter::RosBridgeConnectionThread::connectionError,
                    this, &SidepanelInterpreter::on_connectionError);
        _rbc_thread->stop();
    }
    _connected = false;
    toggleButtonConnect();
}

void SidepanelInterpreter::setTree(const QString& bt_name, const QString& xml_filename)
{
    qDebug() << "Updating interpreter_widget tree model";
    _tree_name = bt_name;

    auto main_win = dynamic_cast<MainWindow*>( _parent );
    updateTree();
    if (!_abstract_tree.rootNode()) {
        // too early; initialization has not finished yet
        return;
    }

    BT::BehaviorTreeFactory factory;
    factory.registerNodeType<Interpreter::InterpreterNode>("Root", {});

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
                if (node.model.type == NodeType::CONDITION) {
                    factory.registerNodeType<Interpreter::InterpreterConditionNode>(registration_ID, ports);
                }
                else {
                    factory.registerNodeType<Interpreter::InterpreterNode>(registration_ID, ports);
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

    _updated = true;
    if (_autorun) {
        _timer->start(20);
    }
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

    BT::StdCoutLogger logger_cout(_tree);
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

    BT::StdCoutLogger logger_cout(_tree);
    std::vector<std::pair<int, NodeStatus>> node_status;

    int i = 1;  // skip root
    for (auto& tree_node: _tree.nodes) {
        if (tree_node->status() == NodeStatus::RUNNING) {
            changeTreeNodeStatus(tree_node, status);
            node_status.push_back( {i, status} );
        }
        i++;
    }
    expandAndChangeNodeStyle(node_status, true);
    _updated = true;
}

void SidepanelInterpreter::changeTreeNodeStatus(std::shared_ptr<BT::TreeNode> node,
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
    std::string topic_type("");
    std::string topic_name = server_name + "/goal";

    roseus_bt::RosbridgeServiceClient service_client_(ui->lineEdit->text().toStdString(),
                                                      ui->lineEdit_port->text().toInt(),
                                                      "/rosapi/topic_type");
    rapidjson::Document request;
    request.SetObject();
    rapidjson::Value topic;
    topic.SetString(topic_name.c_str(), topic_name.size(), request.GetAllocator());
    request.AddMember("topic", topic, request.GetAllocator());
    service_client_.call(request);
    service_client_.waitForResult();
    auto result = service_client_.getResult();
    if (result.HasMember("type") &&
        result["type"].IsString()) {
        topic_type = result["type"].GetString();
        if (topic_type.substr(topic_type.size() - 4) == "Goal") {
            topic_type = topic_type.substr(0, topic_type.size() - 4);
        }
    }
    return topic_type;
}

rapidjson::Document SidepanelInterpreter::getRequestFromPorts(const AbstractTreeNode& node,
                                                              const BT::TreeNode::Ptr& tree_node)
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
        jval = Interpreter::getInputValue(tree_node, name, type, goal.GetAllocator());
        goal.AddMember(jname, jval, goal.GetAllocator());
    }
    return goal;
}

BT::NodeStatus SidepanelInterpreter::executeConditionNode(const AbstractTreeNode& node,
                                                          const BT::TreeNode::Ptr& tree_node)
{
    roseus_bt::RosbridgeServiceClient
        service_client_(ui->lineEdit->text().toStdString(),
                        ui->lineEdit_port->text().toInt(),
                        node.model.ports.find("service_name")->second.default_value.toStdString());

    rapidjson::Document request = getRequestFromPorts(node, tree_node);
    service_client_.call(request);
    service_client_.waitForResult();
    auto result = service_client_.getResult();
    if (result.HasMember("success") &&
        result["success"].IsBool() &&
        result["success"].GetBool()) {
        return NodeStatus::SUCCESS;
    }
    return NodeStatus::FAILURE;
}

BT::NodeStatus SidepanelInterpreter::executeActionNode(const AbstractTreeNode& node,
                                                       const BT::TreeNode::Ptr& tree_node)
{
    auto server_name_port = node.model.ports.find("server_name")->second;
    std::string server_name = server_name_port.default_value.toStdString();
    std::string topic_type = getActionType(server_name);

    roseus_bt::RosbridgeActionClient action_client_(ui->lineEdit->text().toStdString(),
                                                    ui->lineEdit_port->text().toInt(),
                                                    server_name,
                                                    topic_type);

    auto cb = [node, tree_node](std::shared_ptr<WsClient::Connection> connection,
                                std::shared_ptr<WsClient::InMessage> in_message) {
        std::string message = in_message->string();
        rapidjson::CopyDocument document;
        document.Parse(message.c_str());
        document.Swap(document["msg"]["feedback"]);

        std::string name = document["update_field_name"].GetString();
        auto port_model = node.model.ports.find(QString(name.c_str()))->second;
        std::string type = port_model.type_name.toStdString();

        document.Swap(document[name.c_str()]);
        Interpreter::setOutputValue(tree_node, name, type, document);
    };
    action_client_.registerFeedbackCallback(cb);

    rapidjson::Document goal = getRequestFromPorts(node, tree_node);
    action_client_.sendGoal(goal);

    // if (action_client_.isActive()) {
    //     return NodeStatus::RUNNING;
    // }
    action_client_.waitForResult();

    auto result = action_client_.getResult();
    if (result.HasMember("success") &&
        result["success"].IsBool() &&
        result["success"].GetBool()) {
        return NodeStatus::SUCCESS;
    }
    return NodeStatus::FAILURE;
}

void SidepanelInterpreter::executeNode(const int node_id)
{
    int bt_node_id = translateSingleNodeIndex(node_id, false);
    auto node = _abstract_tree.node(node_id);
    auto bt_node = _tree.nodes.at(bt_node_id - 1);
    std::vector<std::pair<int, NodeStatus>> node_status;
    if (node->model.type == NodeType::CONDITION) {
        node_status.push_back( {node_id, executeConditionNode(*node, bt_node)} );
    }
    else if (node->model.type == NodeType::ACTION) {
        node_status.push_back( {node_id, executeActionNode(*node, bt_node)} );
    }
    else {  /* decorators, control, subtrees */
        return;
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

    BT::StdCoutLogger logger_cout(_tree);
    std::vector<std::pair<int, NodeStatus>> prev_node_status;
    std::vector<std::pair<int, NodeStatus>> node_status;
    int i;

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
    if (_updated && _autorun) {
        try {
            tickRoot();
            _updated = false;
        }
        catch (std::exception& err) {
            on_buttonDisableAutoExecution_clicked();
            qWarning() << "Error during auto callback: " << err.what();
        }
    }
}

void SidepanelInterpreter::toggleButtonAutoExecution()
{
    ui->buttonDisableAutoExecution->setEnabled(_autorun);
    ui->buttonEnableAutoExecution->setEnabled(!_autorun);
    ui->buttonRunTree->setEnabled(!_autorun);
}

void SidepanelInterpreter::toggleButtonConnect()
{
    connectionUpdate(_connected);
    ui->lineEdit->setDisabled(_connected);
    ui->lineEdit_port->setDisabled(_connected);
    ui->buttonExecSelection->setEnabled(_connected);
    ui->buttonExecRunning->setEnabled(_connected);
}

void SidepanelInterpreter::on_connectionCreated()
{
    _connected = true;
    toggleButtonConnect();
}

void SidepanelInterpreter::on_connectionError(const QString& message)
{
    // close connection
    _connected = false;
    toggleButtonConnect();

    // display error message
    QMessageBox messageBox;
    messageBox.critical(this, "Connection Error", message);
    messageBox.show();
}

void SidepanelInterpreter::on_buttonResetTree_clicked()
{
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
    _autorun = true;
    _updated = true;
    toggleButtonAutoExecution();
    _timer->start(20);
}

void SidepanelInterpreter::on_buttonDisableAutoExecution_clicked()
{
    _autorun = false;
    toggleButtonAutoExecution();
    _timer->stop();
}

void SidepanelInterpreter::on_buttonRunTree_clicked()
{
    qDebug() << "buttonRunTree";
    try {
        tickRoot();
    }
    catch (std::exception& err) {
        QMessageBox messageBox;
        messageBox.critical(this,"Error Running Tree", err.what() );
        messageBox.show();
    }
}

void SidepanelInterpreter::on_buttonExecSelection_clicked()
{
    qDebug() << "buttonExecSelection";

    if (_tree.nodes.size() <= 1) {
        return;
    }

    BT::StdCoutLogger logger_cout(_tree);
    int i = 0;
    for (auto& node: _abstract_tree.nodes()) {
        if (node.graphic_node->nodeGraphicsObject().isSelected()) {
            executeNode(i);
        }
        i++;
    }
    _updated = true;
}

void SidepanelInterpreter::on_buttonExecRunning_clicked()
{
    qDebug() << "buttonExecRunning";

    if (_tree.nodes.size() <= 1) {
        return;
    }

    BT::StdCoutLogger logger_cout(_tree);
    std::vector<std::pair<int, NodeStatus>> node_status;

    int i = 1;  // skip root
    for (auto& tree_node: _tree.nodes) {
        if (tree_node->status() == NodeStatus::RUNNING) {
            node_status.push_back( {i, NodeStatus::RUNNING} );
        }
        i++;
    }

    translateNodeIndex(node_status, true);
    for (auto it: node_status) {
        executeNode(it.first);
    }
    _updated = true;
}
