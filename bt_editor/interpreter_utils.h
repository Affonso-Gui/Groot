#ifndef INTERPRETER_UTILS_H
#define INTERPRETER_UTILS_H

#include <QThread>
#include <rosbridgecpp/rosbridge_ws_client.hpp>
#include <roseus_bt/ws_service_client.h>
#include <roseus_bt/ws_action_client.h>
#include <roseus_bt/copy_document.h>
#include <fmt/format.h>
#include "bt_editor_base.h"

namespace Interpreter
{

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

    BT::NodeStatus tick() override;

    void set_status(const BT::NodeStatus& status);
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

    void run();
    void stop();

private:
    RosbridgeWsClient _rbc;
    std::string _address;
    const std::string _client_name = std::string("base_connection");

signals:
    void connectionCreated();
    void connectionError(const QString& message);
};

rapidjson::Value getInputValue(const BT::TreeNode::Ptr& tree_node,
                               const std::string name,
                               const std::string type,
                               rapidjson::MemoryPoolAllocator<>& allocator);

void setOutputValue(const BT::TreeNode::Ptr& tree_node,
                    const std::string name,
                    const std::string type,
                    const rapidjson::CopyDocument& document);

}  // namespace
#endif  // INTERPRETER_UTILS_H
