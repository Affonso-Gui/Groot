#ifndef SIDEPANEL_INTERPRETER_H
#define SIDEPANEL_INTERPRETER_H

#include <QFrame>

#include "bt_editor_base.h"

namespace Ui {
class SidepanelInterpreter;
}

class SidepanelInterpreter : public QFrame
{
    Q_OBJECT

public:
    explicit SidepanelInterpreter(QWidget *parent = nullptr);
    ~SidepanelInterpreter();

    void clear();

private slots:

    void on_buttonSetSuccess_clicked();

    void on_buttonSetFailure_clicked();

    void on_buttonSetIdle_clicked();

    void on_buttonRunNode_clicked();

signals:
    void changeNodeStyle(const QString& bt_name,
                         const std::vector<std::pair<int, NodeStatus>>& node_status);

private:
    Ui::SidepanelInterpreter *ui;

    QWidget *_parent;

};

#endif // SIDEPANEL_INTERPRETER_H
