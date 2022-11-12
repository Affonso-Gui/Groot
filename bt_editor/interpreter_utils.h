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

class SidepanelInterpreter;

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
    InterpreterNode(SidepanelInterpreter* parent,
                    const std::string& name,
                    const BT::NodeConfiguration& config);

    virtual void halt() override;

    virtual BT::NodeStatus tick() override;

    virtual BT::NodeStatus executeNode();

    void set_status(const BT::NodeStatus& status);

    void set_exec_thread(ExecuteActionThread* exec_thread);

protected:
    SidepanelInterpreter* _parent;
    ExecuteActionThread* _exec_thread;
    AbstractTreeNode _node;
    bool _connected;
};


class InterpreterActionNode : public InterpreterNode
{
public:
    InterpreterActionNode(SidepanelInterpreter* parent,
                          const std::string& name,
                          const BT::NodeConfiguration& config);

    virtual void halt() override;

    bool isRunning();

    void set_exec_thread(ExecuteActionThread* exec_thread);

private:
    ExecuteActionThread* _exec_thread;
};


class InterpreterSubscriberNode : public InterpreterNode
{
public:
    InterpreterSubscriberNode(SidepanelInterpreter* parent,
                              const std::string& name,
                              const BT::NodeConfiguration& config);

    virtual BT::NodeStatus executeNode() override;

    void connect(const AbstractTreeNode& node);
};


class InterpreterConditionNode : public BT::ConditionNode
{
public:
    InterpreterConditionNode(const std::string& name, const BT::NodeConfiguration& config);

    BT::NodeStatus tick() override;

    BT::NodeStatus executeTick() override;

    BT::NodeStatus executeNode();

    void set_status(const BT::NodeStatus& status);

    void connect(const AbstractTreeNode& node, const std::string& host, int port);

private:
    BT::NodeStatus return_status_;
    std::unique_ptr<roseus_bt::RosbridgeServiceClient> service_client_;
    AbstractTreeNode _node;
    bool _connected;
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
    void registerSubscriber(const AbstractTreeNode& node, BT::TreeNode* tree_node);

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

rapidjson::Value getInputValue(const BT::TreeNode* tree_node,
                               const std::string name,
                               const std::string type,
                               rapidjson::MemoryPoolAllocator<>& allocator);

void setOutputValue(BT::TreeNode* tree_node,
                    const std::string name,
                    const std::string type,
                    const rapidjson::CopyDocument& document);

rapidjson::Document getRequestFromPorts(const AbstractTreeNode& node,
                                        const BT::TreeNode* tree_node);


template <class NodeType>
void RegisterInterpreterNode(BT::BehaviorTreeFactory& factory,
                             const std::string& registration_ID,
                             const BT::PortsList& ports,
                             SidepanelInterpreter* parent)
{
    BT::NodeBuilder builder = [parent](const std::string& name,
                                       const BT::NodeConfiguration& config) {
        return std::make_unique<NodeType>(parent, name, config);
    };

    BT::TreeNodeManifest manifest;
    manifest.type = BT::getType<NodeType>();
    manifest.registration_ID = registration_ID;
    manifest.ports.insert(ports.begin(), ports.end());
    factory.registerBuilder(manifest, builder);
}

}  // namespace
#endif  // INTERPRETER_UTILS_H
