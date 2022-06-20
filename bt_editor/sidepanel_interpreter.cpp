#include "sidepanel_interpreter.h"
#include "ui_sidepanel_interpreter.h"
#include <QPushButton>
#include <QDebug>

#include "mainwindow.h"
#include "utils.h"

SidepanelInterpreter::SidepanelInterpreter(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::SidepanelInterpreter),
    _parent(parent)
{
    ui->setupUi(this);
}

SidepanelInterpreter::~SidepanelInterpreter()
{
    delete ui;
}

void SidepanelInterpreter::clear()
{
}

void SidepanelInterpreter::on_buttonSetSuccess_clicked()
{
    qDebug() << "buttonSetSuccess";
}

void SidepanelInterpreter::on_buttonSetFailure_clicked()
{
    qDebug() << "buttonSetFailure";
}

void SidepanelInterpreter::on_buttonSetIdle_clicked()
{
    qDebug() << "buttonSetIdle";
}

void SidepanelInterpreter::on_buttonRunNode_clicked()
{
    qDebug() << "buttonRunNode";
}
