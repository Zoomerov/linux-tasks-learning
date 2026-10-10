#include <QApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

struct VfsNode
{
    QString name;
    bool isDirectory = false;
    QByteArray data;
    QMap<QString, std::shared_ptr<VfsNode>> children;
};

class VfsWindow : public QMainWindow
{
public:
    VfsWindow(QString vfsName, QByteArray vfsHash,
              std::shared_ptr<VfsNode> root)
        : name(std::move(vfsName)),
          sha256(std::move(vfsHash)),
          rootNode(std::move(root)),
          currentDirectory(rootNode)
    {
        setWindowTitle("VFS");
        resize(700, 500);

        auto *central = new QWidget(this);
        auto *layout = new QVBoxLayout(central);

        output = new QPlainTextEdit(central);
        output->setReadOnly(true);

        input = new QLineEdit(central);
        input->setPlaceholderText(
            "Введите команду: ls, cd, cat, vfs-info, exit");

        layout->addWidget(output);
        layout->addWidget(input);
        setCentralWidget(central);

        connect(input, &QLineEdit::returnPressed,
                this, [this]() { runInteractiveCommand(); });
    }

    bool runScript(const QString &scriptPath)
    {
        QFile script(scriptPath);
        if (!script.open(QIODevice::ReadOnly | QIODevice::Text)) {
            output->appendPlainText(
                "Ошибка: не удалось открыть скрипт «" + scriptPath +
                "»: " + script.errorString());
            return false;
        }

        QTextStream stream(&script);
        int lineNumber = 0;

        while (!stream.atEnd()) {
            const QString line = stream.readLine();
            ++lineNumber;

            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith('#'))
                continue;

            output->appendPlainText("> " + line);

            if (!executeCommand(trimmed)) {
                output->appendPlainText(
                    "Ошибка стартового скрипта, строка " +
                    QString::number(lineNumber));
                return false;
            }
        }

        return true;
    }

private:
    QString name;
    QByteArray sha256;
    std::shared_ptr<VfsNode> rootNode;
    std::shared_ptr<VfsNode> currentDirectory;

    QPlainTextEdit *output = nullptr;
    QLineEdit *input = nullptr;

    void runInteractiveCommand()
    {
        const QString line = input->text().trimmed();
        input->clear();

        if (line.isEmpty())
            return;

        output->appendPlainText("> " + line);
        executeCommand(line);
    }

    bool executeCommand(const QString &line)
    {
        const QStringList parts =
            line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        if (parts.isEmpty())
            return true;

        const QString command = parts[0];
        const QStringList args = parts.mid(1);

        if (command == "exit") {
            if (!args.isEmpty()) {
                output->appendPlainText(
                    "Ошибка: exit не принимает аргументы");
                return false;
            }
            close();
            return true;
        }

        if (command == "vfs-info") {
            if (!args.isEmpty()) {
                output->appendPlainText(
                    "Ошибка: vfs-info не принимает аргументы");
                return false;
            }
            output->appendPlainText("Имя VFS: " + name);
            output->appendPlainText("SHA-256: " + sha256.toHex());
            return true;
        }

        if (command == "ls")
            return commandLs(args);

        if (command == "cd")
            return commandCd(args);

        if (command == "cat")
            return commandCat(args);

        output->appendPlainText(
            "Ошибка: неизвестная команда «" + command + "»");
        return false;
    }

    bool commandLs(const QStringList &args)
    {
        if (args.size() > 1) {
            output->appendPlainText("Ошибка: использование: ls [путь]");
            return false;
        }

        auto directory = args.isEmpty()
            ? currentDirectory
            : findNode(args[0]);

        if (!directory) {
            output->appendPlainText("Ошибка: путь не найден");
            return false;
        }

        if (!directory->isDirectory) {
            output->appendPlainText(directory->name);
            return true;
        }

        if (directory->children.isEmpty()) {
            output->appendPlainText("(пустой каталог)");
            return true;
        }

        for (auto it = directory->children.cbegin();
             it != directory->children.cend(); ++it) {
            const auto &node = it.value();
            output->appendPlainText(
                node->name + (node->isDirectory ? "/" : ""));
        }
        return true;
    }

    bool commandCd(const QStringList &args)
    {
        if (args.size() != 1) {
            output->appendPlainText("Ошибка: использование: cd <путь>");
            return false;
        }

        auto node = findNode(args[0]);
        if (!node) {
            output->appendPlainText("Ошибка: каталог не найден");
            return false;
        }

        if (!node->isDirectory) {
            output->appendPlainText("Ошибка: указанный путь не является каталогом");
            return false;
        }

        currentDirectory = node;
        output->appendPlainText("Текущий каталог: " + args[0]);
        return true;
    }

    bool commandCat(const QStringList &args)
    {
        if (args.size() != 1) {
            output->appendPlainText("Ошибка: использование: cat <путь>");
            return false;
        }

        auto node = findNode(args[0]);
        if (!node) {
            output->appendPlainText("Ошибка: файл не найден");
            return false;
        }

        if (node->isDirectory) {
            output->appendPlainText("Ошибка: это каталог, а не файл");
            return false;
        }

        output->appendPlainText(QString::fromUtf8(node->data));
        return true;
    }

    std::shared_ptr<VfsNode> findNode(const QString &path) const
    {
        if (path.isEmpty() || path == ".")
            return currentDirectory;

        auto node = path.startsWith('/')
            ? rootNode
            : currentDirectory;

        const QStringList parts =
            path.split('/', Qt::SkipEmptyParts);

        for (const QString &part : parts) {
            if (part == ".")
                continue;

            if (part == "..") {
                // Для учебного прототипа переход к родителю не хранится.
                // Поэтому .. поддерживается только от корня.
                if (node == rootNode)
                    continue;
                return nullptr;
            }

            if (!node->isDirectory ||
                !node->children.contains(part)) {
                return nullptr;
            }

            node = node->children[part];
        }

        return node;
    }
};

static std::shared_ptr<VfsNode> parseNode(
    const QString &nodeName, const QJsonObject &json, QString &error)
{
    const QString type = json.value("type").toString();

    auto node = std::make_shared<VfsNode>();
    node->name = nodeName;

    if (type == "file") {
        node->isDirectory = false;
        const QString encodedData = json.value("data").toString();
        node->data = QByteArray::fromBase64(encodedData.toLatin1());
        return node;
    }

    if (type != "directory") {
        error = "узел «" + nodeName +
                "»: поле type должно быть file или directory";
        return nullptr;
    }

    node->isDirectory = true;
    const QJsonValue childrenValue = json.value("children");
    if (!childrenValue.isObject()) {
        error = "каталог «" + nodeName +
                "»: поле children должно быть JSON-объектом";
        return nullptr;
    }

    const QJsonObject children = childrenValue.toObject();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (!it.value().isObject()) {
            error = "узел «" + it.key() + "» должен быть JSON-объектом";
            return nullptr;
        }

        auto child = parseNode(it.key(), it.value().toObject(), error);
        if (!child)
            return nullptr;

        node->children.insert(it.key(), child);
    }

    return node;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Прототип эмулятора VFS");
    parser.addHelpOption();

    QCommandLineOption filesystemOption(
        {"f", "filesystem"},
        "Путь к JSON-файлу виртуальной файловой системы.",
        "path");

    QCommandLineOption scriptOption(
        {"s", "script"},
        "Путь к стартовому скрипту.",
        "path");

    parser.addOption(filesystemOption);
    parser.addOption(scriptOption);
    parser.process(app);

    const QString filesystemPath = parser.value(filesystemOption);
    const QString scriptPath = parser.value(scriptOption);

    qDebug().noquote() << "filesystem:"
                       << (filesystemPath.isEmpty()
                               ? "(не задан)"
                               : filesystemPath);
    qDebug().noquote() << "script:"
                       << (scriptPath.isEmpty()
                               ? "(не задан)"
                               : scriptPath);

    if (filesystemPath.isEmpty()) {
        qCritical() << "Ошибка: задайте JSON-файл через --filesystem";
        return 1;
    }

    QFile vfsFile(filesystemPath);
    if (!vfsFile.open(QIODevice::ReadOnly)) {
        qCritical().noquote()
            << "Ошибка загрузки VFS:" << vfsFile.errorString();
        return 1;
    }

    const QByteArray jsonBytes = vfsFile.readAll();
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(jsonBytes, &parseError);

    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        qCritical().noquote()
            << "Ошибка формата VFS:" << parseError.errorString();
        return 1;
    }

    const QJsonObject rootObject = document.object();
    const QString vfsName = rootObject.value("name").toString();
    const QJsonValue rootValue = rootObject.value("root");

    if (vfsName.isEmpty() || !rootValue.isObject()) {
        qCritical() << "Ошибка формата VFS: нужны поля name и root";
        return 1;
    }

    QString nodeError;
    auto rootNode = parseNode("/", rootValue.toObject(), nodeError);
    if (!rootNode) {
        qCritical().noquote() << "Ошибка формата VFS:" << nodeError;
        return 1;
    }

    const QByteArray sha256 =
        QCryptographicHash::hash(jsonBytes, QCryptographicHash::Sha256);

    qDebug().noquote() << "VFS name:" << vfsName;
    qDebug().noquote() << "VFS SHA-256:" << sha256.toHex();

    VfsWindow window(vfsName, sha256, rootNode);
    window.show();

    if (!scriptPath.isEmpty() && !window.runScript(scriptPath))
        qWarning() << "Стартовый скрипт завершился с ошибкой.";

    return app.exec();
}
