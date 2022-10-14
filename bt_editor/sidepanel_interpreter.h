#ifndef SIDEPANEL_INTERPRETER_H
#define SIDEPANEL_INTERPRETER_H

#include <QFrame>

#include "interpreter_utils.h"
#include <boost/lexical_cast.hpp>

namespace Ui {
class SidepanelInterpreter;
}

class SidepanelInterpreter : public QFrame
{
    Q_OBJECT

public:
    explicit SidepanelInterpreter(QWidget *parent = nullptr);
    ~SidepanelInterpreter();

    void clear();

    void on_Connect();

    void setTree(const QString& bt_name, const QString& xml_filename);

    void setTree(const QString& bt_name);

    void updateTree();

private slots:

    void translateNodeIndex(std::vector<std::pair<int, NodeStatus>>& node_status,
                            bool tree_index);

    void expandAndChangeNodeStyle(std::vector<std::pair<int, NodeStatus>> node_status,
                                  bool reset_before_update);

    void changeSelectedStyle(const NodeStatus& status);

    void changeRunningStyle(const NodeStatus& status);

    void changeTreeNodeStatus(std::shared_ptr<BT::TreeNode> node, const NodeStatus& status);

    std::string getActionType(const std::string& server_name);

    std::pair<std::string, bool> getPortValue(const PortModel& port_model,
                                              const QString& mapping_value);

    std::pair<std::string, bool> getPortValue(const AbstractTreeNode& node,
                                              PortsMapping port_mapping,
                                              const QString& port_name);

    rapidjson::Document getRequestFromPorts(const AbstractTreeNode& node,
                                            const PortsMapping& port_mapping);

    BT::NodeStatus executeConditionNode(const AbstractTreeNode& node);

    BT::NodeStatus executeActionNode(const AbstractTreeNode& node);

    void executeNode(const int node_id);

    void tickRoot();

    void runStep();

    void toggleButtonAutoExecution();

    void toggleButtonConnect();

    void on_connectionCreated();

    void on_connectionError(const QString& message);

    void on_buttonResetTree_clicked();

    void on_buttonSetSuccess_clicked();

    void on_buttonSetFailure_clicked();

    void on_buttonSetIdle_clicked();

    void on_buttonSetRunningSuccess_clicked();

    void on_buttonSetRunningFailure_clicked();

    void on_buttonEnableAutoExecution_clicked();

    void on_buttonDisableAutoExecution_clicked();

    void on_buttonRunTree_clicked();

    void on_buttonExecSelection_clicked();

    void on_buttonExecRunning_clicked();

signals:
    void connectionUpdate(bool connected);

    void changeNodeStyle(const QString& bt_name,
                         const std::vector<std::pair<int, NodeStatus>>& node_status,
                         bool reset_before_update);

private:
    Ui::SidepanelInterpreter *ui;

    BT::Tree _tree;
    AbsBehaviorTree _abstract_tree;
    QString _tree_name;

    QTimer* _timer;
    NodeStatus _root_status;
    bool _autorun;
    bool _updated;

    bool _connected;
    Interpreter::RosBridgeConnectionThread* _rbc_thread;
    std::map<std::string, rapidjson::Document> _blackboard;

    QWidget *_parent;

};

#endif // SIDEPANEL_INTERPRETER_H
