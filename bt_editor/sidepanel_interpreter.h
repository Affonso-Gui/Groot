#ifndef SIDEPANEL_INTERPRETER_H
#define SIDEPANEL_INTERPRETER_H

#include <QFrame>

#include <behaviortree_cpp_v3/loggers/bt_cout_logger.h>
#include "interpreter_utils.h"

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

    std::shared_ptr<roseus_bt::RosbridgeActionClient> registerAction(std::string server_name,
                                                                     PortModels ports,
                                                                     BT::TreeNode::Ptr tree_node);

    std::shared_ptr<roseus_bt::RosbridgeServiceClient> registerService(std::string service_name);

    void registerSubscriber(std::string message_type, BT::TreeNode::Ptr tree_node);

    void registerActionThread(int tree_node_id);

    void connectNode(const BT::TreeNode* node);

    std::string getActionType(const std::string& server_name);

    AbstractTreeNode getAbstractNode(int tree_node_id);

    BT::TreeNode::Ptr getSharedNode(const BT::TreeNode* node);

    int getNodeId(const BT::TreeNode* node);

    void setTree(const QString& bt_name, const QString& xml_filename);

    void setTree(const QString& bt_name);

    void updateTree();

private slots:

    void translateNodeIndex(std::vector<std::pair<int, NodeStatus>>& node_status,
                            bool tree_index);

    int  translateSingleNodeIndex(int node_index, bool tree_index);

    void expandAndChangeNodeStyle(std::vector<std::pair<int, NodeStatus>> node_status,
                                  bool reset_before_update);

    void changeSelectedStyle(const NodeStatus& status);

    void changeRunningStyle(const NodeStatus& status);

    void changeTreeNodeStatus(BT::TreeNode::Ptr node, const NodeStatus& status);

    void ConnectBridge();

    void DisconnectBridge();

    void DisconnectNodes(bool report_result);

    void connectNode(const int tree_node_id);

    void executeNode(const int node_id);

    void tickRoot();

    void runStep();

    void reportError(const QString& title, const QString& message);

    void toggleButtonAutoExecution();

    void toggleButtonConnect();

    void on_connectionCreated();

    void on_connectionError(const QString& message);

    void on_actionThreadCreated(int tree_node_id);

    void on_actionReportResult(int tree_node_id, const QString& status);

    void on_actionFinished();

    void on_buttonResetTree_clicked();

    void on_buttonSetSuccess_clicked();

    void on_buttonSetFailure_clicked();

    void on_buttonSetIdle_clicked();

    void on_buttonSetRunningSuccess_clicked();

    void on_buttonSetRunningFailure_clicked();

    void on_buttonEnableAutoExecution_clicked();

    void on_buttonDisableAutoExecution_clicked();

    void on_buttonHaltTree_clicked();

    void on_buttonExecSelection_clicked();

    void on_buttonExecRunning_clicked();

    void on_buttonShowBlackboard_clicked();

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
    std::unique_ptr<BT::StdCoutLogger> _logger_cout;

    QTimer* _timer;
    NodeStatus _root_status;
    bool _autorun;
    bool _updated;
    std::vector<BT::TreeNode::Ptr> _background_nodes;

    bool _connected;
    Interpreter::RosBridgeConnectionThread* _rbc_thread;
    std::vector<Interpreter::ExecuteActionThread*> _running_threads;
    std::map<std::string, std::shared_ptr<Interpreter::RosbridgeActionClientCapture>> _connected_actions;
    std::map<std::string, std::shared_ptr<roseus_bt::RosbridgeServiceClient>> _connected_services;

    QWidget *_parent;

};

#endif // SIDEPANEL_INTERPRETER_H
