#include "SubtreeNodeModel.hpp"
#include <QLineEdit>
#include <QVBoxLayout>

SubtreeNodeModel::SubtreeNodeModel(const TreeNodeModel& model):
    BehaviorTreeDataModel ( model ),
    _expanded(false)
{
    _line_edit_name->setReadOnly(true);
    _line_edit_name->setHidden(true);

    _expand_button = new QPushButton( _expanded ? "Collapse" : "Expand", _main_widget );
    _expand_button->setMaximumWidth(100);
    _main_layout->addWidget(_expand_button);
    _main_layout->setAlignment(_expand_button, Qt::AlignHCenter);

    _expand_button->setStyleSheet("color: black; background-color: white; "
                                  "border: 0px rgb(115, 210, 22);"
                                  "padding: 4px; border-radius: 3px; ");
    _expand_button->setFlat(false);
    _expand_button->setFocusPolicy(Qt::NoFocus);
    _expand_button->adjustSize();

    connect( _expand_button, &QPushButton::clicked,
             this, [this]()
    {
        emit expandButtonPushed() ;
    });

    updateNodeSize();
}

void SubtreeNodeModel::setExpanded(bool expand)
{
    _expanded = expand;
    _expand_button->setText( _expanded ? "Collapse" : "Expand");
    _expand_button->adjustSize();
    _main_widget->adjustSize();
}

std::pair<QString, QColor> SubtreeNodeModel::caption() const
{
    return { registrationName(),
             QtNodes::NodeStyle().FontColor };
}

QString SubtreeNodeModel::captionIicon() const {
    return(":/icons/svg/subtree.svg");
}

void SubtreeNodeModel::setInstanceName(const QString &name)
{
    _line_edit_name->setHidden( name == registrationName() );
    BehaviorTreeDataModel::setInstanceName(name);
}
