#ifndef NODE_CONVERT_H
#define NODE_CONVERT_H

#include <behaviortree_cpp_v3/bt_factory.h>
#include <behaviortree_cpp_v3/flatbuffers/bt_flatbuffer_helper.h>
#include <roseus_bt/basic_types.h>

roseus_bt::NodeType convert(Serialization::NodeType type);

BT::NodeStatus convert(Serialization::NodeStatus type);

BT::PortDirection convert(Serialization::PortDirection direction);

roseus_bt::NodeType convert(BT::NodeType);

BT::NodeType convert(roseus_bt::NodeType);

bool operator==(const BT::NodeType& node_type_1, const roseus_bt::NodeType& node_type_2);
bool operator!=(const BT::NodeType& node_type_1, const roseus_bt::NodeType& node_type_2);
bool operator==(const roseus_bt::NodeType& node_type_1, const BT::NodeType& node_type_2);
bool operator!=(const roseus_bt::NodeType& node_type_1, const BT::NodeType& node_type_2);

#endif // NODE_CONVERT_H
