#ifndef INTERPRETER_UTILS_H
#define INTERPRETER_UTILS_H

#include <QThread>
#include <rosbridgecpp/rosbridge_ws_client.hpp>
#include <roseus_bt/ws_service_client.h>
#include <roseus_bt/ws_action_client.h>
#include <roseus_bt/copy_document.h>
#include <fmt/format.h>
#include "models/BehaviorTreeNodeModel.hpp"
#include "bt_editor_base.h"

namespace Interpreter
{

class ExecuteActionThread;

class ConditionEvaluation : public std::exception
{
public:
    const char* what () {
        return "Condition Evaluation";
    }
};


class InterpreterNode : public BT::AsyncActionNode
{
public:
    InterpreterNode(const std::string& name, const BT::NodeConfiguration& config);

    virtual void halt() override;

    virtual BT::NodeStatus tick() override;

    void set_status(const BT::NodeStatus& status);

    void set_exec_thread(ExecuteActionThread* exec_thread);

private:
    ExecuteActionThread* _exec_thread;
};


class InterpreterActionNode : public InterpreterNode
{
public:
    InterpreterActionNode(const std::string& name, const BT::NodeConfiguration& config);

    virtual void halt() override;

    bool isRunning();

    void set_exec_thread(ExecuteActionThread* exec_thread);

private:
    ExecuteActionThread* _exec_thread;
};


class InterpreterSubscriberNode : public InterpreterNode
{
public:
    InterpreterSubscriberNode(const std::string& name, const BT::NodeConfiguration& config);
};


class InterpreterConditionNode : public BT::ConditionNode
{
public:
    InterpreterConditionNode(const std::string& name, const BT::NodeConfiguration& config);

    BT::NodeStatus tick() override;

    BT::NodeStatus executeTick() override;

    void set_status(const BT::NodeStatus& status);

private:
    BT::NodeStatus return_status_;
};


class RosBridgeConnectionThread : public QThread
{
Q_OBJECT
public:
    explicit RosBridgeConnectionThread(const std::string& hostname, const std::string& port);
    ~RosBridgeConnectionThread();

    void run();
    void stop();
    void clearSubscribers();
    void registerSubscriber(const AbstractTreeNode& node, const BT::TreeNode::Ptr& tree_node);

private:
    RosbridgeWsClient _rbc;
    std::string _address;
    const std::string _client_name = std::string("base_connection");
    std::vector<std::string> _subscribers;

signals:
    void connectionCreated();
    void connectionError(const QString& message);
};


class ExecuteActionThread : public QThread
{
Q_OBJECT
public:
    explicit ExecuteActionThread(const std::string& hostname, int port,
                                 const std::string& server_name,
                                 const std::string& action_type,
                                 const AbstractTreeNode& node,
                                 const BT::TreeNode::Ptr& tree_node,
                                 int tree_node_id);
    ~ExecuteActionThread();

    void run();
    void stop();

private:
    roseus_bt::RosbridgeActionClient _action_client;
    AbstractTreeNode _node;
    BT::TreeNode::Ptr _tree_node;
    int _tree_node_id;

signals:
    void actionReportResult(int tree_node_id, QString status);
    void actionReportError(const QString& message);
};


rapidjson::Value getInputValue(const BT::TreeNode::Ptr& tree_node,
                               const std::string name,
                               const std::string type,
                               rapidjson::MemoryPoolAllocator<>& allocator);

void setOutputValue(const BT::TreeNode::Ptr& tree_node,
                    const std::string name,
                    const std::string type,
                    const rapidjson::CopyDocument& document);

rapidjson::Document getRequestFromPorts(const AbstractTreeNode& node,
                                        const BT::TreeNode::Ptr& tree_node);

}  // namespace
#endif  // INTERPRETER_UTILS_H
