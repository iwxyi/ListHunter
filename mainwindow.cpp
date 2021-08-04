#include <QFileDialog>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "fileutil.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
      settings(new MySettings("settings.ini", QSettings::Format::IniFormat))
{
    ui->setupUi(this);

    QString path = settings->s("recent/modeFile");
    if (!path.isEmpty() && isFileExist(path))
    {
        QString text = readTextFile(path);
        loadMode(MyJson::from(text.toLatin1()));
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::loadMode(MyJson json)
{
    searchExp = json.s("search_exp");
    LOAD_DEB << "search_exp:" << searchExp;
    resultLines.clear();
    json.each("result_lines", [=](const MyJson& line){
        resultLines.append(LineBean::fromJson(line));
    });
}

void MainWindow::search(QString key)
{

}

void MainWindow::on_searchButton_clicked()
{

}

void MainWindow::on_actionSaveMode_triggered()
{

}

void MainWindow::on_actionLoadMode_triggered()
{
    QString path = QFileDialog::getOpenFileName(this, "打开模式文件", "", "*.json");
    if (path.isEmpty())
        return ;
    QString text = readTextFileAutoCodec(path);
    bool ok;
    QString err;
    MyJson json = MyJson::from(text.toLatin1(), &ok, &err);
    if (!ok)
    {
        qCritical() << "读取模式文件失败：" << err;
        return ;
    }
    settings->set("recent/modeFile", path);
    loadMode(json);
}
