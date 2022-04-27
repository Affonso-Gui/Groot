#include "custom_node_dialog.h"
#include "ui_custom_node_dialog.h"

#include <QTreeWidgetItem>
#include <QPushButton>
#include <QRegExpValidator>
#include <QSettings>
#include <QModelIndexList>

CustomNodeDialog::CustomNodeDialog(const NodeModels &models,
                                   QString to_edit,
                                   QWidget *parent):
    QDialog(parent),
    ui(new Ui::CustomNodeDialog),
    _models(models),
    _editing(false)
{
    ui->setupUi(this);
    setWindowTitle("Custom TreeNode Editor");

    ui->tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    QSettings settings;
    restoreGeometry(settings.value("CustomNodeDialog/geometry").toByteArray());
    ui->tableWidget->horizontalHeader()->restoreState( settings.value("CustomNodeDialog/header").toByteArray() );

    QRegExp rx("\\w+");
    _validator = new QRegExpValidator(rx, this);

    if( to_edit.isEmpty() == false)
    {
        auto model_it = models.find(to_edit);
        if( model_it != models.end())
        {
            _editing =true;
            ui->lineEdit->setText( to_edit );

            const auto& model = model_it->second;
            for( const auto& port_it : model.ports )
            {
                registerPortNode(port_it.first.toStdString(),
                                 port_it.second.direction,
                                 port_it.second.default_value.toStdString(),
                                 port_it.second.type_name.toStdString(),
                                 port_it.second.description.toStdString(),
                                 port_it.second.required);
            }

            if( model.type == NodeType::ACTION )
            {
                ui->comboBox->setCurrentIndex(0);
            }
            else if( model.type == NodeType::CONDITION )
            {
                ui->comboBox->setCurrentIndex(1);
            }
            if( model.type == NodeType::REMOTE_ACTION )
            {
                ui->comboBox->setCurrentIndex(2);
            }
            else if( model.type == NodeType::REMOTE_CONDITION )
            {
                ui->comboBox->setCurrentIndex(3);
            }
            else if( model.type == NodeType::SUBSCRIBER )
            {
                ui->comboBox->setCurrentIndex(4);
            }
            else if( model.type == NodeType::CONTROL )
            {
                ui->comboBox->setCurrentIndex(5);
            }
            else if( model.type == NodeType::SUBTREE )
            {
                ui->comboBox->setCurrentIndex(6);
                ui->comboBox->setEnabled(false);
            }
            else if( model.type == NodeType::DECORATOR)
            {
                ui->comboBox->setCurrentIndex(7);
            }
        }
    }

    else {
        // register server_name as we open the dialog with an ActionNode selection
        registerPortNode("server_name", BT::PortDirection::INPUT, "", "",
                         "name of the Action Server", true);
    }

    connect( ui->tableWidget, &QTableWidget::cellChanged,
             this, &CustomNodeDialog::checkValid );

    connect( ui->lineEdit, &QLineEdit::textChanged,
             this, &CustomNodeDialog::checkValid );

    checkValid();
}

CustomNodeDialog::~CustomNodeDialog()
{
    delete ui;
}


NodeModel CustomNodeDialog::getTreeNodeModel() const
{
    QString ID = ui->lineEdit->text();
    NodeType type = NodeType::UNDEFINED;
    PortModels ports;

    switch( ui->comboBox->currentIndex() )
    {
    case 0: type = NodeType::ACTION; break;
    case 1: type = NodeType::CONDITION; break;
    case 2: type = NodeType::REMOTE_ACTION; break;
    case 3: type = NodeType::REMOTE_CONDITION; break;
    case 4: type = NodeType::SUBSCRIBER; break;
    case 5: type = NodeType::CONTROL; break;
    case 6: type = NodeType::SUBTREE; break;
    case 7: type = NodeType::DECORATOR; break;
    }
    for (int row=0; row < ui->tableWidget->rowCount(); row++ )
    {
        PortModel port_model;
        const QString key       = ui->tableWidget->item(row,0)->text();

        auto combo = static_cast<QComboBox*>(ui->tableWidget->cellWidget(row,1));

        const QString direction = (combo) ? combo->currentText() :
                                            ui->tableWidget->item(row,1)->text();

        port_model.direction = BT::convertFromString<PortDirection>(direction.toStdString());
        port_model.default_value =  ui->tableWidget->item(row,2)->text();
        port_model.type_name     =  ui->tableWidget->item(row,3)->text();
        port_model.description   =  ui->tableWidget->item(row,4)->text();
        // hack: judge if port is required by checking if it is editable
        port_model.required = !(ui->tableWidget->item(row,0)->flags() & Qt::ItemIsEditable);

        ports.insert( {key, port_model} );
    }
    return { type, ID, ports };
}

void CustomNodeDialog::registerPortNode(const std::string key,
                                        const BT::PortDirection& direction,
                                        const std::string value,
                                        const std::string type,
                                        const std::string description,
                                        bool required)
{
    int row = ui->tableWidget->rowCount();
    ui->tableWidget->setRowCount(row+1);

    auto key_item = new QTableWidgetItem (key.c_str());
    auto value_item = new QTableWidgetItem (value.c_str());
    auto type_item = new QTableWidgetItem (type.c_str());
    auto description_item = new QTableWidgetItem (description.c_str());

    if (required) {
        auto direction_item = new QTableWidgetItem (toStr(direction).c_str());

        key_item->setFlags(key_item->flags() & ~Qt::ItemIsEditable );
        type_item->setFlags(type_item->flags() & ~Qt::ItemIsEditable );
        description_item->setFlags(description_item->flags() & ~Qt::ItemIsEditable );
        direction_item->setFlags(direction_item->flags() & ~Qt::ItemIsEditable );

        ui->tableWidget->setItem(row, 1, direction_item);
    }
    else {
        QComboBox* combo_direction = new QComboBox;
        combo_direction->addItem(toStr(BT::PortDirection::INPUT).c_str());
        combo_direction->addItem(toStr(BT::PortDirection::OUTPUT).c_str());
        combo_direction->addItem(toStr(BT::PortDirection::INOUT).c_str());

        switch( direction )
        {
            case BT::PortDirection::INPUT : combo_direction->setCurrentIndex(0); break;
            case BT::PortDirection::OUTPUT: combo_direction->setCurrentIndex(1); break;
            case BT::PortDirection::INOUT : combo_direction->setCurrentIndex(2); break;
        }
        ui->tableWidget->setCellWidget(row, 1, combo_direction);
    }

    ui->tableWidget->setItem(row, 0, key_item);
    ui->tableWidget->setItem(row, 2, value_item);
    ui->tableWidget->setItem(row, 3, type_item);
    ui->tableWidget->setItem(row, 4, description_item);
}

void CustomNodeDialog::checkValid()
{
    auto setError = [this] (const std::string text) {
        ui->labelWarning->setText(text.c_str());
        ui->labelWarning->setStyleSheet("color: rgb(204, 0, 0)");
        ui->buttonBox->button( QDialogButtonBox::Ok )->setEnabled( false );
    };
    auto setValid = [this] () {
        ui->labelWarning->setText("OK");
        ui->labelWarning->setStyleSheet("color: rgb(78, 154, 6)");
        ui->buttonBox->button( QDialogButtonBox::Ok )->setEnabled( true );
    };

    auto checkDuplicateValue = [this, setError] (const std::string name, const QString& value,
                                                 std::vector<int> index_types,
                                                 std::vector<NodeType> node_types) {

        if ( std::find(index_types.begin(), index_types.end(),
                       ui->comboBox->currentIndex()) != index_types.end() )
        {
            for( auto it = _models.begin(); it != _models.end(); ++it )
                {
                    if( std::find(node_types.begin(), node_types.end(),
                                  it->second.type) != node_types.end() )
                    {
                        auto port_value = it->second.ports.find(name.c_str());
                        if( port_value != it->second.ports.end() &&
                            port_value->second.default_value == value )
                        {
                            return true;
                        }
                    }
                }
            return false;
        }
    };

    auto checkServerName = [checkDuplicateValue] (const QString& value) {
        std::vector<int> index_types{0, 2};
        std::vector<NodeType> node_types{NodeType::ACTION, NodeType::REMOTE_ACTION};
        return checkDuplicateValue("server_name", value, index_types, node_types);
    };

    auto checkServiceName = [checkDuplicateValue] (const QString& value) {
        std::vector<int> index_types{1, 3};
        std::vector<NodeType> node_types{NodeType::CONDITION, NodeType::REMOTE_CONDITION};
        return checkDuplicateValue("service_name", value, index_types, node_types);
    };

    auto name = ui->lineEdit->text();
    int pos;

    if( name.toLower() == "root" )
    {
        setError("The name 'root' is forbidden");
        return;
    }
    if( name.isEmpty() )
    {
        setError("The name cannot be empty");
        return;
    }
    if( _validator->validate(name, pos) != QValidator::Acceptable)
    {
        setError("Invalid name: use only letters, digits and underscores");
        return;
    }
    if( _models.count( name ) > 0 && !_editing )
    {
        setError("Another Node has the same name");
        return;
    }

    std::set<QString> param_names;
    for (int row=0; row < ui->tableWidget->rowCount(); row++ )
    {
        auto param_name_item = ui->tableWidget->item(row,0);
        auto param_value_item = ui->tableWidget->item(row,2);
        auto param_type_item = ui->tableWidget->item(row,3);

        if (!param_name_item || param_name_item->text().isEmpty()) {
            setError("Port name cannot be empty");
            return;
        }

        auto param_name = param_name_item->text();

        if( _validator->validate(param_name, pos) != QValidator::Acceptable)
        {
            setError("Invalid port name: use only letters, digits and underscores.");
            return;
        }
        if( param_name == "ID" || param_name == "name" )
        {
            setError("Reserved port name: the words \"name\" and \"ID\" should not be used.");
            return;
        }
        if( !param_type_item || (param_type_item->text().isEmpty() &&
                                 param_type_item->flags() & Qt::ItemIsEditable ) )
        {
            setError("Port type cannot be empty");
            return;
        }
        if( param_type_item->flags() & Qt::ItemIsEditable &&
            param_type_item->text().toStdString().find('/') == std::string::npos &&
            std::find(_ros_message_types.begin(), _ros_message_types.end(),
                      param_type_item->text().toStdString()) == _ros_message_types.end() )
        {
            setError("Invalid port type: use a built-in or compound ros message type");
            return;
        }
        if( !(param_name_item->flags() & Qt::ItemIsEditable) &&
            !param_value_item || param_value_item->text().isEmpty() )
        {
            setError(param_name.toStdString() + " default value cannot be empty");
            return;
        }
        if ( param_name.toStdString() == "server_name" && param_value_item && !_editing &&
             checkServerName(param_value_item->text()) )
        {
            setError("Duplicated server name: " + param_value_item->text().toStdString());
            return;
        }
        if ( param_name.toStdString() == "service_name" && param_value_item && !_editing &&
             checkServiceName(param_value_item->text()) )
        {
            setError("Duplicated service name: " + param_value_item->text().toStdString());
            return;
        }

        param_names.insert(param_name);
    }
    if( param_names.size() < ui->tableWidget->rowCount() )
    {
       setError("Duplicated port name");
       return;
    }
    if( param_names.size() != ui->tableWidget->rowCount() )
    {
        setError("Port size mismatch");
        return;
    }
    setValid();
}

void CustomNodeDialog::closeEvent(QCloseEvent *)
{
    QSettings settings;
    settings.setValue("CustomNodeDialog/geometry", saveGeometry());
    settings.setValue("CustomNodeDialog/header", ui->tableWidget->horizontalHeader()->saveState() );
}

void CustomNodeDialog::on_buttonBox_clicked(QAbstractButton *)
{
    QSettings settings;
    settings.setValue("CustomNodeDialog/geometry", saveGeometry());
    settings.setValue("CustomNodeDialog/header", ui->tableWidget->horizontalHeader()->saveState() );
}

void CustomNodeDialog::on_tableWidget_itemSelectionChanged()
{
    QModelIndexList selected_rows = ui->tableWidget->selectionModel()->selectedRows();
    ui->pushButtonRemove->setEnabled( selected_rows.count() != 0);
}

void CustomNodeDialog::on_pushButtonAdd_pressed()
{
    registerPortNode("key_name", BT::PortDirection::INPUT, "", "string", "", false);
    checkValid();
}

void CustomNodeDialog::on_pushButtonRemove_pressed()
{
    auto selected = ui->tableWidget->selectionModel()->selectedRows();
    for( const auto& index: selected)
    {
        ui->tableWidget->removeRow( index.row() );
    }
    checkValid();
}

void CustomNodeDialog::on_comboBox_currentIndexChanged(const QString &node_type)
{
    auto unregister_node = [this](std::string key) {
        auto shared_items = ui->tableWidget->findItems(key.c_str(), Qt::MatchExactly);
        for (const auto& item: shared_items) {
            ui->tableWidget->removeRow( item->row() );
        }
    };

    auto unregister_all_but = [unregister_node] (std::vector<std::string> black_list) {
        std::vector<std::string> all_nodes { "server_name",
                                             "service_name",
                                             "host_name",
                                             "host_port",
                                             "type",
                                             "topic_name",
                                             "to",
                                             "__shared_blackboard" };
        for (const auto& it : black_list) {
            all_nodes.erase(std::remove(all_nodes.begin(), all_nodes.end(), it),
                            all_nodes.end());
        }
        for (const auto& node : all_nodes) {
            unregister_node(node);
        }
    };

    auto maybeRegisterPortNode = [this] (const std::string key,
                                         const BT::PortDirection& direction,
                                         const std::string value,
                                         const std::string type,
                                         const std::string description,
                                         bool required) {
        if (ui->tableWidget->findItems(key.c_str(), Qt::MatchExactly).empty()) {
            registerPortNode(key, direction, value, type, description, required);
        }
    };

    if (node_type == "DecoratorNode" || node_type == "ControlNode") {
        unregister_all_but(std::vector<std::string>{});
    }
    if (node_type == "SubTree") {
        unregister_all_but(std::vector<std::string>{"__shared_blackboard"});
        maybeRegisterPortNode("__shared_blackboard", BT::PortDirection::INPUT, "false", "",
            "If false (default), the Subtree has an isolated blackboard and needs port remapping",
            true);
    }
    if (node_type == "Subscriber") {
        unregister_all_but(std::vector<std::string>{"type", "topic_name", "to"});
        maybeRegisterPortNode("type", BT::PortDirection::INPUT, "", "",
                              "ROS message type (e.g. std_msgs/String)",
                              true);
        maybeRegisterPortNode("topic_name", BT::PortDirection::INPUT, "", "",
                              "name of the subscribed topic",
                              true);
        maybeRegisterPortNode("to", BT::PortDirection::OUTPUT, "", "",
                              "port to where messages are redirected",
                              true);
    }
    if (node_type == "ActionNode" || node_type == "RemoteAction") {
        if (node_type == "ActionNode") {
            unregister_all_but(std::vector<std::string>{"server_name"});
        }
        if (node_type == "RemoteAction") {
            unregister_all_but(std::vector<std::string>{"server_name", "host_name", "host_port"});
        }
        maybeRegisterPortNode("server_name", BT::PortDirection::INPUT, "", "",
                              "name of the Action Server",
                              true);
    }
    if (node_type == "ConditionNode" || node_type == "RemoteCondition") {
        if (node_type == "ConditionNode") {
            unregister_all_but(std::vector<std::string>{"service_name"});
        }
        if (node_type == "RemoteCondition") {
            unregister_all_but(std::vector<std::string>{"service_name", "host_name", "host_port"});
        }
        maybeRegisterPortNode("service_name", BT::PortDirection::INPUT, "", "",
                              "name of the ROS service",
                              true);
    }
    if (node_type == "RemoteAction" || node_type == "RemoteCondition") {
        maybeRegisterPortNode("host_name", BT::PortDirection::INPUT, "", "",
                              "name of the rosbridge_server host",
                              true);
        maybeRegisterPortNode("host_port", BT::PortDirection::INPUT, "", "",
                              "port of the rosbridge_server host",
                              true);
    }

    checkValid();
}
