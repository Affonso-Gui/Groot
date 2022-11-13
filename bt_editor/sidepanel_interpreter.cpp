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
    _autorun(true),
    _updated(true),
    _connected(false),
    _rbc_thread(nullptr),
    _logger_cout(nullptr),
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
    if (_rbc_thread) {
        delete _rbc_thread;
    }
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
    for (auto exec_thread: _running_threads) {
        exec_thread->stop();
    }
    _connected = false;
    toggleButtonConnect();
}

void SidepanelInterpreter::registerSubscriber(const AbstractTreeNode& node,
                                             BT::TreeNode* tree_node)
{
    if (!(_connected && _rbc_thread)) {
        throw std::runtime_error(std::string("Not connected"));
    }
    _rbc_thread->registerSubscriber(node, tree_node);
}

void SidepanelInterpreter::
registerActionThread(Interpreter::ExecuteActionThread* exec_thread)
{
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

void SidepanelInterpreter::setTree(const QString& bt_name, const QString& xml_filename)
{
    qDebug() << "Updating interpreter_widget tree model";
    _tree_name = bt_name;

    // disconnect subscribers
    if (_connected && _rbc_thread) {
        _rbc_thread->clearSubscribers();
    }

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
                    factory.registerNodeType<Interpreter::InterpreterConditionNode>
                        (registration_ID, ports);
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

BT::NodeStatus SidepanelInterpreter::executeConditionNode(const AbstractTreeNode& node,
                                                          const BT::TreeNode::Ptr& tree_node)
{
    auto node_ref = std::static_pointer_cast<Interpreter::InterpreterConditionNode>(tree_node);
    node_ref->connect(node,
                      ui->lineEdit->text().toStdString(),
                      ui->lineEdit_port->text().toInt());

    return node_ref->executeNode();
}

BT::NodeStatus SidepanelInterpreter::executeActionNode(const AbstractTreeNode& node,
                                                       const BT::TreeNode::Ptr& tree_node,
                                                       int tree_node_id)
{
    auto node_ref = std::static_pointer_cast<Interpreter::InterpreterActionNode>(tree_node);
    if (node_ref->isRunning()) {
        return NodeStatus::RUNNING;
    }

    node_ref->connect(node,
                      ui->lineEdit->text().toStdString(),
                      ui->lineEdit_port->text().toInt(),
                      tree_node_id);

    return node_ref->executeNode();
}

BT::NodeStatus SidepanelInterpreter::executeSubscriberNode(const AbstractTreeNode& node,
                                                           const BT::TreeNode::Ptr& tree_node)
{
    auto node_ref = std::static_pointer_cast<Interpreter::InterpreterSubscriberNode>(tree_node);
    node_ref->connect(node);
    return node_ref->executeNode();
}

void SidepanelInterpreter::executeNode(const int node_id)
{
    int bt_node_id = translateSingleNodeIndex(node_id, false);
    auto node = _abstract_tree.node(node_id);
    auto bt_node = _tree.nodes.at(bt_node_id - 1);
    std::vector<std::pair<int, NodeStatus>> node_status;
    if (node->model.type == roseus_bt::NodeType::CONDITION ||
        node->model.type == roseus_bt::NodeType::REMOTE_CONDITION) {
        node_status.push_back( {node_id, executeConditionNode(*node, bt_node)} );
    }
    else if (node->model.type == roseus_bt::NodeType::ACTION ||
             node->model.type == roseus_bt::NodeType::REMOTE_ACTION) {
        node_status.push_back( {node_id, executeActionNode(*node, bt_node, bt_node_id)} );
    }
    else if (node->model.type == roseus_bt::NodeType::SUBSCRIBER ||
             node->model.type == roseus_bt::NodeType::REMOTE_SUBSCRIBER) {
        node_status.push_back( {node_id, executeSubscriberNode(*node, bt_node)} );
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

void SidepanelInterpreter::reportError(const QString& title, const QString& message)
{
    QMessageBox messageBox;
    messageBox.critical(this, title, message);
    messageBox.show();
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
    reportError("Connection Error", message);
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
        reportError("Error Running Tree", err.what() );
    }
}

void SidepanelInterpreter::on_buttonExecSelection_clicked()
{
    qDebug() << "buttonExecSelection";

    if (_tree.nodes.size() <= 1) {
        return;
    }

    try {
        int i = 0;
        for (auto& node: _abstract_tree.nodes()) {
            if (node.graphic_node->nodeGraphicsObject().isSelected()) {
                executeNode(i);
            }
            i++;
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

    translateNodeIndex(node_status, true);
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
