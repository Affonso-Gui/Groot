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


class InterpreterNodeBase
{
public:
    InterpreterNodeBase(SidepanelInterpreter* parent);

    virtual BT::NodeStatus executeNode() = 0;

    virtual void connect(const AbstractTreeNode& node,
                         const std::string& host,
                         int port,
                         int tree_node_id) = 0;

    virtual void disconnect() = 0;

protected:
    SidepanelInterpreter* _parent;
    AbstractTreeNode _node;
    bool _connected;
};


class InterpreterNode : public BT::AsyncActionNode,
                        public InterpreterNodeBase
{
public:
    InterpreterNode(SidepanelInterpreter* parent,
                    const std::string& name,
                    const BT::NodeConfiguration& config);

    virtual void halt() override;

    virtual BT::NodeStatus tick() override;

    virtual BT::NodeStatus executeNode() override;

    virtual void connect(const AbstractTreeNode& node,
                         const std::string& host,
                         int port,
                         int tree_node_id) override;

    virtual void disconnect() override;

    void set_status(const BT::NodeStatus& status);
};


class InterpreterActionNode : public InterpreterNode
{
public:
    InterpreterActionNode(SidepanelInterpreter* parent,
                          const std::string& name,
                          const BT::NodeConfiguration& config);

    virtual void halt() override;

    virtual BT::NodeStatus executeNode() override;

    virtual void connect(const AbstractTreeNode& node, const std::string& host, int port, int tree_node_id) override;

    virtual void disconnect() override;

    bool isRunning();

private:
    ExecuteActionThread* _exec_thread;
    std::shared_ptr<roseus_bt::RosbridgeActionClient> _action_client;
    int _tree_node_id;
};


class InterpreterSubscriberNode : public InterpreterNode
{
public:
    InterpreterSubscriberNode(SidepanelInterpreter* parent,
                              const std::string& name,
                              const BT::NodeConfiguration& config);

    virtual BT::NodeStatus executeNode() override;
};


class InterpreterConditionNode : public BT::ConditionNode,
                                 public InterpreterNodeBase
{
public:
    InterpreterConditionNode(SidepanelInterpreter* parent,
                             const std::string& name,
                             const BT::NodeConfiguration& config);

    virtual BT::NodeStatus tick() override;

    virtual BT::NodeStatus executeTick() override;

    virtual BT::NodeStatus executeNode() override;

    virtual void connect(const AbstractTreeNode& node, const std::string& host, int port, int tree_node_id) override;

    virtual void disconnect() override;

    void set_status(const BT::NodeStatus& status);

private:
    std::unique_ptr<roseus_bt::RosbridgeServiceClient> _service_client;
    BT::NodeStatus _return_status;
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
    void registerSubscriber(const AbstractTreeNode& node, BT::TreeNode::Ptr tree_node);

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
    explicit ExecuteActionThread(std::shared_ptr<roseus_bt::RosbridgeActionClient> action_client,
                                 const AbstractTreeNode& node,
                                 BT::TreeNode::Ptr tree_node,
                                 int tree_node_id);
    void run();
    void stop();

private:
    std::shared_ptr<roseus_bt::RosbridgeActionClient> _action_client;
    AbstractTreeNode _node;
    BT::TreeNode::Ptr _tree_node;
    int _tree_node_id;

signals:
    void actionReportResult(int tree_node_id, QString status);
    void actionReportError(const QString& message);
};

rapidjson::Value getInputValue(BT::TreeNode::Ptr tree_node,
                               const std::string name,
                               const std::string type,
                               rapidjson::MemoryPoolAllocator<>& allocator);

void setOutputValue(BT::TreeNode::Ptr tree_node,
                    const std::string name,
                    const std::string type,
                    const rapidjson::CopyDocument& document);

rapidjson::Document getRequestFromPorts(const AbstractTreeNode& node,
                                        BT::TreeNode::Ptr tree_node);


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
