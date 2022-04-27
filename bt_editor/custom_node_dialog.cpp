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
    registerPortNode("server_name", "Input", "", "name of the Action Server");

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
                int row = ui->tableWidget->rowCount();
                ui->tableWidget->setRowCount(row+1);

                ui->tableWidget->setItem(row,0, new QTableWidgetItem(port_it.first) );
                QComboBox* combo_direction = new QComboBox;
                combo_direction->addItem("Input");
                combo_direction->addItem("Output");
                combo_direction->addItem("In/Out");
                switch( port_it.second.direction )
                {
                case BT::PortDirection::INPUT : combo_direction->setCurrentIndex(0); break;
                case BT::PortDirection::OUTPUT: combo_direction->setCurrentIndex(1); break;
                case BT::PortDirection::INOUT : combo_direction->setCurrentIndex(2); break;
                }
                ui->tableWidget->setCellWidget(row,1, combo_direction );
                ui->tableWidget->setItem(row,2, new QTableWidgetItem(port_it.second.default_value) );
                ui->tableWidget->setItem(row,3, new QTableWidgetItem("") );
                ui->tableWidget->setItem(row,4, new QTableWidgetItem(port_it.second.description) );
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
        ports.insert( {key, port_model} );
    }
    return { type, ID, ports };
}

void CustomNodeDialog::registerPortNode(const std::string key,
                                        const std::string direction,
                                        const std::string value,
                                        const std::string description)
{
    int row = ui->tableWidget->rowCount();
    ui->tableWidget->setRowCount(row+1);

    auto key_item = new QTableWidgetItem (key.c_str());
    key_item->setFlags(key_item->flags() & ~Qt::ItemIsEditable );

    auto direction_item = new QTableWidgetItem (direction.c_str());
    direction_item->setFlags(direction_item->flags() & ~Qt::ItemIsEditable );

    auto value_item = new QTableWidgetItem (value.c_str());

    auto type_item = new QTableWidgetItem ("");
    type_item->setFlags(type_item->flags() & ~Qt::ItemIsEditable );

    auto description_item = new QTableWidgetItem (description.c_str());
    description_item->setFlags(description_item->flags() & ~Qt::ItemIsEditable );

    ui->tableWidget->setItem(row, 0, key_item);
    ui->tableWidget->setItem(row, 1, direction_item);
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
        auto param_name = ui->tableWidget->item(row,0)->text();
        auto param_type = ui->tableWidget->item(row,3);

        if(param_name.isEmpty())
        {
            setError("Port name cannot be empty");
            return;
        }
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
        if( !param_type || (param_type->text().isEmpty() &&
                            param_type->flags() & Qt::ItemIsEditable ) )
        {
            setError("Port type cannot be empty");
            return;
        }
        if( param_type->flags() & Qt::ItemIsEditable &&
            param_type->text().toStdString().find('/') == std::string::npos &&
            std::find(_ros_message_types.begin(), _ros_message_types.end(),
                      param_type->text().toStdString()) == _ros_message_types.end() )
        {
            setError("Invalid port type: use a built-in or compound ros message type");
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
    int row = ui->tableWidget->rowCount();
    ui->tableWidget->setRowCount(row+1);

    ui->tableWidget->setItem(row,0, new QTableWidgetItem( "key_name" ));
    QComboBox* combo_direction = new QComboBox;

    combo_direction->addItem("Input");
    combo_direction->addItem("Output");
    combo_direction->addItem("In/Out");

    ui->tableWidget->setCellWidget(row, 1, combo_direction);
    ui->tableWidget->setItem(row,2, new QTableWidgetItem());
    ui->tableWidget->setItem(row,3, new QTableWidgetItem("string"));
    ui->tableWidget->setItem(row,4, new QTableWidgetItem());

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

    auto unregister_all = [unregister_node] () {
        unregister_node("__shared_blackboard");
        unregister_node("type");
        unregister_node("topic_name");
        unregister_node("to");
        unregister_node("server_name");
        unregister_node("service_name");
        unregister_node("host_name");
        unregister_node("host_port");
    };

    unregister_all();
    if (node_type == "SubTree") {
      registerPortNode("__shared_blackboard", "Input", "false",
                    "If false (default), the Subtree has an isolated blackboard and needs port remapping");
    }
    if (node_type == "Subscriber") {
      registerPortNode("type", "Input", "", "ROS message type (e.g. std_msgs/String)");
      registerPortNode("topic_name", "Input", "", "name of the subscribed topic");
      registerPortNode("to", "Output", "", "port to where messages are redirected");
    }
    if (node_type == "ActionNode" || node_type == "RemoteAction") {
      registerPortNode("server_name", "Input", "", "name of the Action Server");
    }
    if (node_type == "ConditionNode" || node_type == "RemoteCondition") {
      registerPortNode("service_name", "Input", "", "name of the ROS service");
    }
    if (node_type == "RemoteAction" || node_type == "RemoteCondition") {
      registerPortNode("host_name", "Input", "", "name of the rosbridge_server host");
      registerPortNode("host_port", "Input", "", "port of the rosbridge_server host");
    }

    checkValid();
}
