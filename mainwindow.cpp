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
        QMessageBox::critical(this, "加载模式JSON失败", err);
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

    ui->searchEdit->clear();
    ui->resultTable->setModel(new QStandardItemModel());
    resultLines.clear();
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
        QMessageBox::critical(this, "无法搜索", "找不和适合执行的命令行\n[search_types]下没有满足关键词的搜索表达式");
        return ;
    }

    // 执行命令行
    QProcess process;
    qInfo() << "exec_cmd:" << cmd;
    process.start("cmd", QStringList{"/c", cmd});
    process.waitForStarted();
    process.waitForFinished();
    QString result = QString::fromLocal8Bit(process.readAllStandardOutput());
    QString error = QString::fromLocal8Bit(process.readAllStandardError());
    QStringList lines = result.split(QRegularExpression("[\\r\\n]+"), QString::SkipEmptyParts);
    qInfo() << "result_line_count:" << lines.count();
    // qInfo() << "result:" << result << "    error:" << error;
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
    auto rows = ui->resultTable->selectionModel()->selectedRows(0);
    if (!rows.size())
        return ;

    QMenu* menu = new QMenu;
    int row = rows.first().row();
    if (row < 0 || row >= resultLines.size())
        return ;
    QString str = resultLines.at(row);

    auto canAllResultMatch = [=](const QString& re) -> bool {
        for (auto ri: rows)
        {
            if (!resultLines.at(ri.row()).contains(QRegularExpression(re)))
            {
                return false;
            }
        }
        return true;
    };

    QRegularExpressionMatch match;
    for (int i = 0; i < resultLineBeans.size(); i++)
    {
        const LineBean& lb = resultLineBeans.at(i);
        if (str.indexOf(QRegularExpression(lb.expression), 0, &match) < 0)
            continue;
        if (!canAllResultMatch(lb.expression))
            continue;

        // 匹配到这一个action组，遍历是否所有action都可以匹配
        QStringList caps = match.capturedTexts();
        for (auto action: lb.actions)
        {
            QAction* act = new QAction(action.name, menu);
            QString cmd = action.cmd;

            // 判断action自己的表达式
            if (!action.exp.isEmpty())
            {
                if (str.indexOf(QRegularExpression(action.exp), 0, &match) < 0)
                    continue;
                if (!canAllResultMatch(action.exp))
                    continue;
                caps = match.capturedTexts();
            }

            // 设置执行cmd
            connect(act, &QAction::triggered, this, [=]{
                QString re = action.exp.isEmpty() ? lb.expression : action.exp;
                QRegularExpressionMatch match;
                for (auto ri: rows) // 遍历每一行
                {
                    if (resultLines.at(ri.row()).indexOf(QRegularExpression(re), 0, &match) == -1)
                    {
                        qWarning() << "action.cmd匹配失败：";
                        continue;
                    }
                    QStringList caps = match.capturedTexts();
                    QString t_cmd = cmd;
                    for (int i = 0; i < caps.size(); i++)
                    {
                        t_cmd.replace("%" + QString::number(i), caps.at(i));
                    }
                    runCmds(t_cmd);
                }
                if (action.refresh)
                    on_searchButton_clicked();
            });

            // 添加菜单
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
