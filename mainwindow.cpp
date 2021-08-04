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
    QString tip = json.s("placeholder");
    if (!tip.isEmpty())
        ui->searchEdit->setPlaceholderText(tip);

    searchTypes.clear();
    json.each("search_types", [=](const MyJson& line){
        searchTypes.append(SearchType::fromJson(line));
    });

    resultTitles.clear();
    for (auto val: json.a("result_titles"))
        resultTitles.append(val.toString());
    LOAD_DEB << "result_titles:" << resultTitles;

    resultLineBeans.clear();
    json.each("result_lines", [=](const MyJson& line){
        resultLineBeans.append(LineBean::fromJson(line));
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
    for (auto line: resultLineBeans)
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
            cmd.replace("%" + QString::number(i + 1), caps.at(i));
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
    // for (auto line: lines)
    //    qDebug() << line;

    // 设置表格
    QStandardItemModel* model = new QStandardItemModel();
    model->setColumnCount(resultTitles.size());
    for (int i = 0; i < resultTitles.size(); i++)
        model->setHeaderData(i, Qt::Horizontal, resultTitles.at(i));

    // 添加到结果
    int modelLineIndex = 0;
    resultLines.clear();
    for (QString lineStr: lines)
    {
        // 判断匹配的格式
        int i;
        for (i = 0; i < resultLineBeans.size(); i++)
        {
            const LineBean& lb = resultLineBeans.at(i);
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
            resultLines.append(lineStr);
            break;
        }
        if (i == resultLineBeans.size())
            ; // 没有适合匹配的
    }
    ui->resultTable->setModel(model);
    ui->resultTable->resizeColumnsToContents();
}

void MainWindow::runCmds(QString cmd)
{
    QProcess process;
    qInfo() << "exec_cmd:" << cmd;
    process.start(cmd);
    process.waitForStarted();
    process.waitForFinished();
    QString result = QString::fromLocal8Bit(process.readAllStandardOutput());
    qInfo() << "result:" << result;
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

void MainWindow::on_resultTable_customContextMenuRequested(const QPoint&)
{
    // TODO: 支持多选
    auto rows = ui->resultTable->selectionModel()->selectedRows(0);
    if (!rows.size())
        return ;

    QMenu* menu = new QMenu;
    int row = rows.first().row();
    if (row < 0 || row >= resultLines.size())
        return ;
    QString str = resultLines.at(row);

    QRegularExpressionMatch match;
    for (int i = 0; i < resultLineBeans.size(); i++)
    {
        const LineBean& lb = resultLineBeans.at(i);
        if (str.indexOf(QRegularExpression(lb.expression), 0, &match) < 0)
            continue;

        // 匹配到了
        QStringList caps = match.capturedTexts();
        for (auto action: lb.actions)
        {
            QAction* act = new QAction(action.name, menu);
            QString cmd = action.cmd;
            if (!action.exp.isEmpty())
            {
                if (str.indexOf(QRegularExpression(action.exp), 0, &match) < 0)
                    continue;
            }
            for (int i = 0; i < action.args.size(); i++)
            {
                cmd.replace("%" + QString::number(i + 1), caps.at(action.args.at(i)));
            }
            connect(act, &QAction::triggered, this, [=]{
                runCmds(cmd);
            });
            menu->addAction(act);
        }
        break;
    }

    if (menu->actions().size() == 0)
    {
        delete menu;
        return ;
    }
    menu->exec(QCursor::pos());
}
