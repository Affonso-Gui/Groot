#include "convert.h"

roseus_bt::NodeType convert(Serialization::NodeType type)
{
    switch (type)
    {
    case Serialization::NodeType::ACTION:
        return roseus_bt::NodeType::ACTION;
    case Serialization::NodeType::DECORATOR:
        return roseus_bt::NodeType::DECORATOR;
    case Serialization::NodeType::CONTROL:
        return roseus_bt::NodeType::CONTROL;
    case Serialization::NodeType::CONDITION:
        return roseus_bt::NodeType::CONDITION;
    case Serialization::NodeType::SUBTREE:
        return roseus_bt::NodeType::SUBTREE;
    case Serialization::NodeType::UNDEFINED:
        return roseus_bt::NodeType::UNDEFINED;
    }
    return roseus_bt::NodeType::UNDEFINED;
}

BT::NodeStatus convert(Serialization::NodeStatus type)
{
    switch (type)
    {
    case Serialization::NodeStatus::IDLE:
        return BT::NodeStatus::IDLE;
    case Serialization::NodeStatus::SUCCESS:
        return BT::NodeStatus::SUCCESS;
    case Serialization::NodeStatus::RUNNING:
        return BT::NodeStatus::RUNNING;
    case Serialization::NodeStatus::FAILURE:
        return BT::NodeStatus::FAILURE;
    }
    return BT::NodeStatus::IDLE;
}

BT::PortDirection convert(Serialization::PortDirection direction)
{
    switch (direction)
    {
    case Serialization::PortDirection::INPUT :
        return BT::PortDirection::INPUT;
    case Serialization::PortDirection::OUTPUT:
        return BT::PortDirection::OUTPUT;
    case Serialization::PortDirection::INOUT:
        return BT::PortDirection::INOUT;
    }
    return BT::PortDirection::INOUT;
}

roseus_bt::NodeType convert(BT::NodeType type) {
    switch (type) {
    case BT::NodeType::ACTION:
        return roseus_bt::NodeType::ACTION;
    case BT::NodeType::CONDITION:
        return roseus_bt::NodeType::CONDITION;
    case BT::NodeType::CONTROL:
        return roseus_bt::NodeType::CONTROL;
    case BT::NodeType::DECORATOR:
        return roseus_bt::NodeType::DECORATOR;
    case BT::NodeType::SUBTREE:
        return roseus_bt::NodeType::SUBTREE;
    }
    return roseus_bt::NodeType::UNDEFINED;
}

BT::NodeType convert(roseus_bt::NodeType type) {
    switch (type) {
    case roseus_bt::NodeType::ACTION:
        return BT::NodeType::ACTION;
    case roseus_bt::NodeType::CONDITION:
        return BT::NodeType::CONDITION;
    case roseus_bt::NodeType::REMOTE_ACTION:
        return BT::NodeType::ACTION;
    case roseus_bt::NodeType::REMOTE_CONDITION:
        return BT::NodeType::CONDITION;
    case roseus_bt::NodeType::SUBSCRIBER:
        return BT::NodeType::ACTION;
    case roseus_bt::NodeType::REMOTE_SUBSCRIBER:
        return BT::NodeType::ACTION;
    case roseus_bt::NodeType::CONTROL:
        return BT::NodeType::CONTROL;
    case roseus_bt::NodeType::DECORATOR:
        return BT::NodeType::DECORATOR;
    case roseus_bt::NodeType::SUBTREE:
        return BT::NodeType::SUBTREE;
    }
    return BT::NodeType::UNDEFINED;
}

bool operator==(const BT::NodeType& node_type_1, const roseus_bt::NodeType& node_type_2) {
    return (node_type_1 == convert(node_type_2));
}

bool operator!=(const BT::NodeType& node_type_1, const roseus_bt::NodeType& node_type_2) {
    return !(node_type_1 == node_type_2);
}

bool operator==(const roseus_bt::NodeType& node_type_1, const BT::NodeType& node_type_2) {
    return (convert(node_type_1) == node_type_2);
}

bool operator!=(const roseus_bt::NodeType& node_type_1, const BT::NodeType& node_type_2) {
    return !(node_type_1 == node_type_2);
}

std::ostream& operator<<(std::ostream& os, const rapidjson::CopyDocument& document)
{
    rapidjson::StringBuffer strbuf;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
    document.Accept(writer);
    os << strbuf.GetString();
    return os;
}

std::ostream& operator<<(std::ostream& os, const BT::Any& value)
{
    if (value.empty()) {
        os << "(empty)";
        return os;
    }
    if (value.isNumber()) {
        os << value.cast<double>();
        return os;
    }
    if (value.isString()) {
        os << "\"" << value.cast<std::string>() << "\"";
        return os;
    }
    os << value.cast<rapidjson::CopyDocument>();
    return os;
}
