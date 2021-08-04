#include <QFileDialog>
#include <QProcess>
#include <QStandardItemModel>
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
        QString text = readTextFileAutoCodec(path);
        loadMode(MyJson::from(text.toUtf8()));
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
    resultTitles.clear();
    for (auto val: json.a("result_titles"))
        resultTitles.append(val.toString());
    LOAD_DEB << "result_titles:" << resultTitles;
    resultLines.clear();
    json.each("result_lines", [=](const MyJson& line){
        resultLines.append(LineBean::fromJson(line));
    });
}

void MainWindow::search(QString key)
{
    // 执行命令行
    QProcess process;
    QString cmd = searchExp.arg(key);
    cmd = "ping baidu.com";
    qInfo() << "exec cmd:" << cmd;
    process.start(cmd);
    process.waitForStarted();
    process.waitForFinished();
    QString result = QString::fromLocal8Bit(process.readAllStandardOutput());
    QStringList lines = result.split(QRegularExpression("[\\r\\n]+"), QString::SkipEmptyParts);
    qDebug() << "result:\n" << lines;

    // 设置表格
    return ;
    QStandardItemModel* model = new QStandardItemModel();
    model->setColumnCount(resultTitles.size());
    for (int i = 0; i < resultTitles.size(); i++)
        model->setHeaderData(i, Qt::Horizontal, resultTitles.at(i));

    // 添加到结果
    int modelLineIndex = 0;
    for (QString lineStr: lines)
    {
        // 判断匹配的格式
        int i;
        for (i = 0; i < resultLines.size(); i++)
        {
            const LineBean& lb = resultLines.at(i);
            const QString& expression = lb.expression;
            QRegularExpressionMatch match;
            if (lineStr.indexOf(QRegularExpression(expression), 0, &match) < 0)
                continue;
            if (lb.ignore) // 忽略这一行
                break;

            // 添加到表格
            qDebug() << "添加：" << lineStr;
            const QStringList& caps = match.capturedTexts();
            int maxi = qMin(caps.size() - 1, resultTitles.size());
            for (int j = 0; j < maxi; j++)
            {
                QString cell = caps.at(j + 1);
                // 添加到单元格里面
                model->setItem(modelLineIndex, j, new QStandardItem(cell));
            }
            modelLineIndex++;
        }
        if (i == resultLines.size())
            ; // 没有适合匹配的
    }
    ui->resultTable->setModel(model);
}

void MainWindow::on_searchButton_clicked()
{
    search(ui->searchEdit->text());
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
    MyJson json = MyJson::from(text.toUtf8(), &ok, &err);
    if (!ok)
    {
        qCritical() << "读取模式文件失败：" << err;
        return ;
    }
    settings->set("recent/modeFile", path);
    loadMode(json);
}

void MainWindow::on_searchEdit_returnPressed()
{
    search(ui->searchEdit->text());
}
