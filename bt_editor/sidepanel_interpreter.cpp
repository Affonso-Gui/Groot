#include "sidepanel_interpreter.h"
#include "ui_sidepanel_interpreter.h"
#include <QPushButton>
#include <QDebug>

#include "mainwindow.h"
#include "utils.h"

class InterpreterNode : public BT::AsyncActionNode
{
public:
    InterpreterNode(const std::string& name, const BT::NodeConfiguration& config) :
        BT::AsyncActionNode(name,config)
    {}

    static BT::PortsList providedPorts()
    {
        return{};
    }

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
    _tree_name("BehaviorTree"),
    _parent(parent)
{
    ui->setupUi(this);
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
    std::unordered_set<std::string> nodes_set;
    nodes_set.insert("Root");
    for (auto& node: _abstract_tree.nodes()) {
        if (node.model.type == NodeType::ACTION ||
            node.model.type == NodeType::CONDITION) {
            std::string registration_ID = node.model.registration_ID.toStdString();
            nodes_set.insert(registration_ID);
        }
    }
    for (auto& node_id: nodes_set) {
        factory.registerNodeType<InterpreterNode>(node_id);
    }

    if (xml_filename.isNull()) {
        QString xml_text = main_win->saveToXML();
        _tree = factory.createTreeFromText(xml_text.toStdString());
    }
    else {
        _tree = factory.createTreeFromFile(xml_filename.toStdString());
    }
}

void SidepanelInterpreter::setTree(const QString& bt_name)
{
    setTree(bt_name, QString::null);
}

void SidepanelInterpreter::changeSelectedStyle(const NodeStatus& status)
{
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
}

void SidepanelInterpreter::changeRunningStyle(const NodeStatus& status)
{
    std::vector<std::pair<int, NodeStatus>> node_status;
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
}


void SidepanelInterpreter::on_buttonResetTree_clicked()
{
    auto main_win = dynamic_cast<MainWindow*>( _parent );
    setTree(_tree_name);
    main_win->resetTreeStyle(_abstract_tree);
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

void SidepanelInterpreter::on_buttonRunNode_clicked()
{
    qDebug() << "buttonRunNode";
    NodeStatus root_status;
    root_status = _tree.tickRoot();

    std::vector<std::pair<int, NodeStatus>> node_status;
    node_status.push_back( {0, root_status} );

    int i = 1;
    for (auto& node: _tree.nodes) {
        node_status.push_back( {i, node->status()} );
        i++;
    }
    emit changeNodeStyle(_tree_name, node_status);
}
