#ifndef CUSTOM_NODE_DIALOG_H
#define CUSTOM_NODE_DIALOG_H

#include "bt_editor_base.h"
#include <QDialog>
#include <QValidator>
#include <QAbstractButton>

namespace Ui {
class CustomNodeDialog;
}

class CustomNodeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomNodeDialog(const NodeModels& models, QString to_edit = QString(), QWidget *parent = nullptr);

    ~CustomNodeDialog() override;

    NodeModel getTreeNodeModel() const;

private slots:
    void on_pushButtonAdd_pressed();

    void on_pushButtonRemove_pressed();

    void registerPortNode(const std::string key,
                          const BT::PortDirection& direction,
                          const std::string value,
                          const std::string type,
                          const std::string description,
                          bool required);

    void checkValid();

    virtual void closeEvent(QCloseEvent *) override;

    void on_buttonBox_clicked(QAbstractButton *button);

    void on_tableWidget_itemSelectionChanged();

    void on_comboBox_currentIndexChanged(const QString &node_type);

  private:
    Ui::CustomNodeDialog *ui;
    const NodeModels &_models;
    QValidator *_validator;
    bool _editing;
    std::vector<std::string> _ros_message_types {"bool", "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64", "float32", "float64", "string"};
};

#endif // CUSTOM_NODE_DIALOG_H
