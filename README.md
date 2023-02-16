# Groot with Interpreter Mode

This repository is a fork of [Groot](https://github.com/BehaviorTree/Groot), with an additional Interpreter Mode for interactive node-by-node evaluation and verification.
The Interpreter Mode is compliant with [BehaviorTree.CPP v3.8.1](https://github.com/BehaviorTree/BehaviorTree.CPP/tree/3.8.1), [BehaviorTree.ROS](https://github.com/BehaviorTree/BehaviorTree.ROS), and [roseus_bt](https://github.com/jsk-ros-pkg/jsk_roseus/tree/master/roseus_bt).

![groot_interpreter_mode](https://user-images.githubusercontent.com/20625381/219300340-bae115c2-f49d-4747-ac49-15bad7570261.png)

# Interpreter Mode

The interpreter mode allows for two kinds of interaction: **disconnected evaluation** and **connected evaluation**.
In **disconnected evaluation**, the user can freely manipulate the outcomes of individual nodes, and verify which would be the node selected by the tree structure under the given situation.
In **connected evaluation**, the Groot communicates with an action server and executes the designates nodes in the real robot. In order to perform connected evaluation, the Groot must first connect to a rosbridge server, using the `Connect` button on the left bar.

The buttons provided during disconnected evaluation are:

- **Reset Tree**: Reloads the tree, resetting the state of all nodes.
- **Selection to SUCCESS**: Sets the state of all selected nodes to `SUCCESS`.
- **Selection to FAILURE**: Sets the state of all selected nodes to `FAILURE`.
- **Selection to IDLE**: Sets the state of all selected nodes to `IDLE`.
- **Running to SUCCESS**: Sets the state of all currently running nodes to `SUCCESS`.
- **Running to FAILURE**: Sets the state of all currently running nodes to `FAILURE`.
- **Display Blackboard Variables**: Display the current value of all blackboard variables in a separate dialog.

The buttons provided during connected evaluation are:

- **Execute Selection**: Evaluate selected nodes.
- **Execute Running**: Evaluate nodes that are currently running (highlighted in blue).
- **Enable Auto Execution**: Continuously evaluate running nodes and tick the tree in real-time.
- **Halt Tree**: Sends an interruption signal to all running nodes.

# Shortcuts
| Key | Action |
| :-: | :-: |
| CTRL-S | Save Tree |
| CTRL-L | Load Tree |
| CTRL-O | Load Tree |
| CTRL-Z | Undo |
| CTRL-Y | Redo |
| CTRL-SHIFT-Z | Redo |
| CTRL-A | Reorder (Arrange) Nodes |
| R | Reorder (Arrange) Nodes |
| L | Toggle Layout |
| C | Center View |
| N | New Node |
| + | Zoom In |
| - | Zoom Out |
| CTRL-SHIFT-E | Switch to Editor Mode |
| CTRL-SHIFT-I | Switch to Interpreter Mode |

# Licence

Copyright (c) 2018-2019 FUNDACIO EURECAT 

Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.
