#include "sidepanel_interpreter.h"
#include "ui_sidepanel_interpreter.h"
#include <QPushButton>
#include <QDebug>

#include "mainwindow.h"
#include "utils.h"

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

void SidepanelInterpreter::setTree(const QString& name, const AbsBehaviorTree& abstract_tree) {
    qDebug() << "Updating interpreter_widget tree model";
    _tree_name = name;
    _abstract_tree = abstract_tree;
}

void SidepanelInterpreter::changeSelectedStyle(const NodeStatus& status)
{
    std::vector<int> selected_nodes;
    std::vector<std::pair<int, NodeStatus>> node_status;
    int i = 0;
    for (auto& node: _abstract_tree.nodes()) {
        if (node.graphic_node->nodeGraphicsObject().isSelected()) {
            node_status.push_back( {i, status} );
            node.status = status;
        }
        i++;
    }
    emit changeNodeStyle(_tree_name, node_status);
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

void SidepanelInterpreter::on_buttonRunNode_clicked()
{
    qDebug() << "buttonRunNode";
}
