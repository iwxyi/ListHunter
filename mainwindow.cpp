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
        loadModeFile(path);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::loadModeFile(QString path)
{
    QFileInfo info(path);
    QString name = info.baseName();
    ui->searchEdit->setPlaceholderText(name);

    QString text = readTextFileAutoCodec(path);
    bool ok;
    QString err;
    MyJson json = MyJson::from(text.toUtf8(), &ok, &err);
    if (!ok)
    {
        qCritical() << "读取模式文件失败：" << err;
        return ;
    }
    loadMode(json);
}

void MainWindow::loadMode(MyJson json)
{
    searchTypes.clear();
    json.each("search_types", [=](const MyJson& line){
        searchTypes.append(SearchType::fromJson(line));
    });

    resultTitles.clear();
    for (auto val: json.a("result_titles"))
        resultTitles.append(val.toString());
    LOAD_DEB << "result_titles:" << resultTitles;

    resultLines.clear();
    json.each("result_lines", [=](const MyJson& line){
        resultLines.append(LineBean::fromJson(line));
    });
}

void MainWindow::saveModeFile(QString path)
{
    MyJson json;

    QJsonArray array;
    for (auto type: searchTypes)
        array.append(type.toJson());
    json.insert("search_types", array);

    array = QJsonArray();
    for (auto title: resultTitles)
        array.append(title);
    json.insert("result_titles", array);

    array = QJsonArray();
    for (auto line: resultLines)
        array.append(line.toJson());
    json.insert("result_lines", array);

    writeTextFile(path, json.toBa());
}

void MainWindow::search(QString key)
{
    // 判断要执行的命令
    QString cmd;
    for (auto type: searchTypes)
    {
        QRegularExpressionMatch match;
        if (key.indexOf(QRegularExpression(type.keyExp), 0, &match) < 0)
            continue;

        QStringList caps = match.capturedTexts();
        cmd = type.searchExp;
        for (int i = 0; i < caps.size(); i++)
            cmd.replace("%" + QString::number(i+1), caps.at(i));
        break;
    }
    if (cmd.isEmpty())
    {
        qCritical() << "没有要执行的命令行";
        return ;
    }

    // 执行命令行
    QProcess process;
    qInfo() << "exec_cmd:" << cmd;
    process.start(cmd);
    process.waitForStarted();
    process.waitForFinished();
    QString result = QString::fromLocal8Bit(process.readAllStandardOutput());
    QStringList lines = result.split(QRegularExpression("[\\r\\n]+"), QString::SkipEmptyParts);
    qInfo() << "result_count:" << lines.count();
    for (auto line: lines)
        qDebug() << line;

    // 设置表格
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
            model->setRowCount(modelLineIndex + 1);
            const QStringList& caps = match.capturedTexts();
            int maxi = qMin(caps.size() - 1, resultTitles.size());
            for (int j = 0; j < maxi; j++)
            {
                QString cell = caps.at(j + 1);
                // 添加到单元格里面
                model->setItem(modelLineIndex, j, new QStandardItem(cell));
            }
            modelLineIndex++;
            break;
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
    QString path = QFileDialog::getSaveFileName(this, "保存模式文件", settings->s("recent/modeFile"), "*.json");
    if (path.isEmpty())
        return ;
    settings->set("recent/modeFile", path);
    saveModeFile(path);
}

void MainWindow::on_actionLoadMode_triggered()
{
    QString path = QFileDialog::getOpenFileName(this, "打开模式文件", settings->s("recent/modeFile"), "*.json");
    if (path.isEmpty())
        return ;
    settings->set("recent/modeFile", path);

    loadModeFile(path);
}

void MainWindow::on_searchEdit_returnPressed()
{
    search(ui->searchEdit->text());
}

void MainWindow::showEvent(QShowEvent *e)
{
    restoreGeometry(settings->value("mainwindow/geometry").toByteArray());
    return QMainWindow::showEvent(e);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    settings->set("mainwindow/geometry", saveGeometry());
    return QMainWindow::closeEvent(e);
}
