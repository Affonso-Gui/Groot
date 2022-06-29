#include "sidepanel_interpreter.h"
#include "ui_sidepanel_interpreter.h"
#include <QPushButton>
#include <QDebug>

#include "mainwindow.h"
#include "utils.h"

SidepanelInterpreter::SidepanelInterpreter(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::SidepanelInterpreter),
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

std::vector<int> SidepanelInterpreter::get_selectedNodes(const QString& bt_name)
{
    auto main_win = dynamic_cast<MainWindow*>( _parent );
    auto tree = BuildTreeFromScene( main_win->getTabByName(bt_name)->scene() );
    std::vector<int> selected_nodes;
    int i = 0;
    for (auto& node: tree.nodes()) {
        if (node.graphic_node->nodeGraphicsObject().isSelected()) {
            selected_nodes.push_back(i);
        }
        i++;
    }
    return selected_nodes;
}

void SidepanelInterpreter::changeSelectedStyle(const QString& bt_name,
                                               const NodeStatus& status)
{
    std::vector<std::pair<int, NodeStatus>> node_status;
    for (auto& node_id: get_selectedNodes(bt_name)) {
        node_status.push_back( {node_id, status} );
    }
    emit changeNodeStyle( bt_name, node_status );
}

void SidepanelInterpreter::on_buttonSetSuccess_clicked()
{
    qDebug() << "buttonSetSuccess";
    changeSelectedStyle("BehaviorTree", NodeStatus::SUCCESS);
}

void SidepanelInterpreter::on_buttonSetFailure_clicked()
{
    qDebug() << "buttonSetFailure";
    changeSelectedStyle("BehaviorTree", NodeStatus::FAILURE);
}

void SidepanelInterpreter::on_buttonSetIdle_clicked()
{
    qDebug() << "buttonSetIdle";
    changeSelectedStyle("BehaviorTree", NodeStatus::IDLE);
}

void SidepanelInterpreter::on_buttonRunNode_clicked()
{
    qDebug() << "buttonRunNode";
}
