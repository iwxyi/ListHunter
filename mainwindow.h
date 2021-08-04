#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include "mysettings.h"
#include "myjson.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define LOAD_DEB if (0) qInfo()

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    struct OperatorBean
    {
        QString name; // 操作名字：【结束程序】
        QString cmd; // 操作命令：【taskkill /pid %1 /f】
        QList<int> args; // 传入的参数在当前行捕获组中的索引：【[5]】

        static OperatorBean fromJson(const MyJson& json)
        {
            OperatorBean ob;
            ob.name = json.s("name");
            LOAD_DEB << "        name:" << ob.name;
            ob.cmd = json.s("cmd");
            LOAD_DEB << "        cmd:" << ob.cmd;
            for (auto val: json.a("args"))
                ob.args.append(val.toInt());
            return ob;
        }

        MyJson toJson() const
        {
            MyJson json;
            json.add("name", name).add("cmd", cmd);
            QJsonArray array;
            for (int val: args)
                array.append(val);
            json.add("args", array);
            return json;
        }
    };

    struct LineBean
    {
        QString expression; // 符合这一行的正则表达式，每个捕获组都是一个标签
        /* 【^\s*(\w+)\s+([\d\.:]+)\s+([\d\.:]+)\s+LISTENING\s+(\d+)\s*$】 */
        /*   TCP    0.0.0.0:5520           0.0.0.0:0              LISTENING       24536
             TCP    [::]:5520              [::]:0                 LISTENING       24536 */
        QList<OperatorBean> actions; // 菜单操作
        bool ignore = false;
        char aaa[3];

        static LineBean fromJson(const MyJson& json)
        {
            LineBean lb;
            lb.expression = json.s("expression");
            LOAD_DEB << "    line_exp:" << lb.expression;
            lb.ignore = json.b("ignore", lb.ignore);
            for (auto val: json.a("actions"))
                lb.actions.append(OperatorBean::fromJson(val.toObject()));
            return lb;
        }

        MyJson toJson() const
        {
            MyJson json;
            json.add("expression", expression)
                    .add("ignore", ignore);
            QJsonArray array;
            for (auto action: actions)
                array.append(action.toJson());
            json.add("actions", array);
            return json;
        }
    };

private slots:
    void loadMode(MyJson json);
    void search(QString key);

private slots:
    void on_searchButton_clicked();

    void on_actionSaveMode_triggered();

    void on_actionLoadMode_triggered();

    void on_searchEdit_returnPressed();

private:
    Ui::MainWindow *ui;
    MySettings* settings;

    // 搜索变量
    QString searchKey; // 搜索的变量：【8080】
    QString searchExp; // 搜索的表达式：【netstat -ano | findstr %1】
    QStringList resultTitles;
    QList<LineBean> resultLines; // 每一行搜索结果

};
#endif // MAINWINDOW_H
