#include "sidepanel_interpreter.h"
#include "ui_sidepanel_interpreter.h"
#include <behaviortree_cpp_v3/loggers/bt_cout_logger.h>
#include <QPushButton>
#include <QMessageBox>
#include <QDebug>

#include "mainwindow.h"
#include "utils.h"

class InterpreterNode : public BT::AsyncActionNode
{
public:
    InterpreterNode(const std::string& name, const BT::NodeConfiguration& config) :
        BT::AsyncActionNode(name,config)
    {}

    virtual void halt() override {}

    BT::NodeStatus tick() override {
        return BT::NodeStatus::RUNNING;
    }

    void set_status(const BT::NodeStatus& status)
    {
        setStatus(status);
    }
};


SidepanelInterpreter::SidepanelInterpreter(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::SidepanelInterpreter),
    _root_status(NodeStatus::IDLE),
    _tree_name("BehaviorTree"),
    _autorun(true),
    _updated(true),
    _parent(parent)
{
    ui->setupUi(this);
    ui->buttonDisableAutoExecution->setEnabled(true);
    ui->buttonEnableAutoExecution->setEnabled(false);
    ui->buttonRunNode->setEnabled(false);
}

SidepanelInterpreter::~SidepanelInterpreter()
{
    delete ui;
}

void SidepanelInterpreter::clear()
{
}

void SidepanelInterpreter::setTree(const QString& bt_name, const QString& xml_filename) {
    qDebug() << "Updating interpreter_widget tree model";
    _tree_name = bt_name;

    auto main_win = dynamic_cast<MainWindow*>( _parent );
    _abstract_tree = BuildTreeFromScene( main_win->getTabByName(bt_name)->scene() );

    BT::BehaviorTreeFactory factory;
    factory.registerNodeType<InterpreterNode>("Root", {});

    for (auto& node: _abstract_tree.nodes()) {
        std::string registration_ID = node.model.registration_ID.toStdString();
        BT::PortsList ports;
        for (auto& it: node.model.ports) {
            ports.insert( {it.first.toStdString(), BT::PortInfo(it.second.direction)} );
        }
        try {
            factory.registerNodeType<InterpreterNode>(registration_ID, ports);
        }
        catch(BT::BehaviorTreeException err) {
            // Duplicated node
            // qDebug() << err.what();
        }
    }

    if (xml_filename.isNull()) {
        QString xml_text = main_win->saveToXML();
        _tree = factory.createTreeFromText(xml_text.toStdString());
    }
    else {
        _tree = factory.createTreeFromFile(xml_filename.toStdString());
    }

    _updated = true;
    _timer = new QTimer(this);
    connect( _timer, &QTimer::timeout, this, &SidepanelInterpreter::runStep);

    if (_autorun) {
        _timer->start(20);
    }
}

void SidepanelInterpreter::setTree(const QString& bt_name)
{
    setTree(bt_name, QString::null);
}

void SidepanelInterpreter::changeSelectedStyle(const NodeStatus& status)
{
    BT::StdCoutLogger logger_cout(_tree);
    std::vector<std::pair<int, NodeStatus>> node_status;
    int i = 0;
    for (auto& node: _abstract_tree.nodes()) {
        if (node.graphic_node->nodeGraphicsObject().isSelected()) {
            node_status.push_back( {i, status} );

            auto tree_node = _tree.nodes.at(i-1);  // skip root
            auto tree_node_ref = std::static_pointer_cast<InterpreterNode>(tree_node);
            tree_node_ref->set_status(status);
        }
        i++;
    }
    emit changeNodeStyle(_tree_name, node_status);
    _updated = true;
}

void SidepanelInterpreter::changeRunningStyle(const NodeStatus& status)
{
    BT::StdCoutLogger logger_cout(_tree);
    std::vector<std::pair<int, NodeStatus>> node_status;

    if (_tree.nodes.size() == 1 && _tree.rootNode()->name() == "Root") {
        return;
    }

    int i = 1;  // skip root
    for (auto& tree_node: _tree.nodes) {
        if (tree_node->status() == NodeStatus::RUNNING) {
            auto tree_node_ref = std::static_pointer_cast<InterpreterNode>(tree_node);
            tree_node_ref->set_status(status);
            node_status.push_back( {i, status} );
        }
        i++;
    }
    emit changeNodeStyle(_tree_name, node_status);
    _updated = true;
}

void SidepanelInterpreter::tickRoot()
{
    BT::StdCoutLogger logger_cout(_tree);
    _root_status = _tree.tickRoot();
    if (_root_status != NodeStatus::RUNNING) {
        // stop evaluations until the next change
        _updated = false;
    }

    if (_tree.nodes.size() == 1 && _tree.rootNode()->name() == "Root") {
        return;
    }

    std::vector<std::pair<int, NodeStatus>> node_status;
    node_status.push_back( {0, _root_status} );

    int i = 1;
    for (auto& node: _tree.nodes) {
        node_status.push_back( {i, node->status()} );
        i++;
    }
    emit changeNodeStyle(_tree_name, node_status);
}

void SidepanelInterpreter::runStep()
{
    if (_updated && _autorun) {
        try {
            tickRoot();
        }
        catch (std::exception& err) {
            on_buttonDisableAutoExecution_clicked();
            qWarning() << "Error during auto callback: " << err.what();
        }
    }
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
    ui->buttonDisableAutoExecution->setEnabled(true);
    ui->buttonEnableAutoExecution->setEnabled(false);
    ui->buttonRunNode->setEnabled(false);
    _timer->start(20);
}

void SidepanelInterpreter::on_buttonDisableAutoExecution_clicked()
{
    _autorun = false;
    ui->buttonDisableAutoExecution->setEnabled(false);
    ui->buttonEnableAutoExecution->setEnabled(true);
    ui->buttonRunNode->setEnabled(true);
    _timer->stop();
}

void SidepanelInterpreter::on_buttonRunNode_clicked() {
    qDebug() << "buttonRunNode";
    try {
        tickRoot();
    }
    catch (std::exception& err) {
        QMessageBox messageBox;
        messageBox.critical(this,"Error Running Tree", err.what() );
        messageBox.show();
    }
}
