#include <QApplication>
#include <QFileSystemModel>
#include <QFileIconProvider>
#include <QScreen>
#include <QScroller>
#include <QTreeView>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QWidget>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <QHeaderView>
#include <QFileInfo>

// Класс для фильтрации строк
class FileFilterProxyModel : public QSortFilterProxyModel {
public:
    explicit FileFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override {
        QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
        return recursiveFilterAcceptsRow(index);
    }

private:
    bool recursiveFilterAcceptsRow(const QModelIndex &index) const {
        if (sourceModel()->data(index).toString().contains(filterRegularExpression())) {
            return true;
        }
        for (int i = 0; i < sourceModel()->rowCount(index); ++i) {
            if (recursiveFilterAcceptsRow(sourceModel()->index(i, 0, index))) {
                return true;
            }
        }
        return false;
    }
};

// Модель для вычисления размера папок
class CustomFileSystemModel : public QFileSystemModel {
public:
    explicit CustomFileSystemModel(QObject *parent = nullptr) : QFileSystemModel(parent) {}

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole && index.column() == 1) {  // Столбец "Размер"
            QFileInfo fileInfo = this->fileInfo(index);
            if (fileInfo.isDir()) {
                qint64 folderSize = calculateFolderSize(fileInfo.absoluteFilePath());
                return humanReadableSize(folderSize);
            }
            return humanReadableSize(fileInfo.size());  // для файлов
        }
        return QFileSystemModel::data(index, role);
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        return QFileSystemModel::columnCount(parent) + 1;  // Добавляем столбец для "Размер"
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
            if (section == 1) return "Размер";  // Название столбца
        }
        return QFileSystemModel::headerData(section, orientation, role);
    }

private:
    qint64 calculateFolderSize(const QString &folderPath) const {
        qint64 size = 0;
        QDir dir(folderPath);
        for (const QFileInfo &fileInfo : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden)) {
            size += fileInfo.size();
        }
        for (const QFileInfo &dirInfo : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
            size += calculateFolderSize(dirInfo.absoluteFilePath());
        }
        return size;
    }

    QString humanReadableSize(qint64 size) const {
        QStringList units = {"B", "KB", "MB", "GB", "TB"};
        double readableSize = size;
        int unitIndex = 0;
        while (readableSize >= 1024.0 && unitIndex < units.size() - 1) {
            readableSize /= 1024.0;
            ++unitIndex;
        }
        return QString::number(readableSize, 'f', 2) + " " + units[unitIndex];
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QT_VERSION_STR);

    QCommandLineParser parser;
    parser.setApplicationDescription("Qt Dir View Example");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption dontUseCustomDirectoryIconsOption("c", "Set QFileSystemModel::DontUseCustomDirectoryIcons");
    parser.addOption(dontUseCustomDirectoryIconsOption);

    QCommandLineOption dontWatchOption("w", "Set QFileSystemModel::DontWatch");
    parser.addOption(dontWatchOption);

    parser.addPositionalArgument("directory", "The directory to start in.");
    parser.process(app);

    const QString rootPath = parser.positionalArguments().isEmpty()
                             ? QDir::homePath()
                             : parser.positionalArguments().first();

    CustomFileSystemModel model;
    model.setRootPath("");
    model.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);

    if (parser.isSet(dontUseCustomDirectoryIconsOption))
        model.setOption(QFileSystemModel::DontUseCustomDirectoryIcons);

    if (parser.isSet(dontWatchOption))
        model.setOption(QFileSystemModel::DontWatchForChanges);


    FileFilterProxyModel filterModel;
    filterModel.setSourceModel(&model);
    filterModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
    filterModel.setFilterKeyColumn(0);

    QTreeView tree;
    tree.setModel(&filterModel);

    // Устанавливаем стартовую директорию
    if (!rootPath.isEmpty()) {
        const QModelIndex rootIndex = model.index(QDir::cleanPath(rootPath));
        if (rootIndex.isValid())
            tree.setRootIndex(filterModel.mapFromSource(rootIndex));
    }

    // Внешний вид
    tree.setAnimated(false);
    tree.setIndentation(20);
    tree.setSortingEnabled(true);
    const QSize availableSize = tree.screen()->availableGeometry().size();
    tree.resize(availableSize / 2);
    tree.setColumnWidth(0, tree.width() / 3);

    // Устанавливаем возможность изменения ширины столбцов
    tree.header()->setSectionResizeMode(QHeaderView::Interactive);
    tree.setColumnWidth(1, 100);

    QScroller::grabGesture(&tree, QScroller::TouchGesture);
    tree.setWindowTitle(QObject::tr("Dir View"));
    
    QLineEdit filterInput;
    filterInput.setPlaceholderText(QObject::tr("Enter file or folder name to filter"));

    QObject::connect(&filterInput, &QLineEdit::textChanged, [&](const QString &text) {
        filterModel.setFilterRegularExpression(QRegularExpression(text, QRegularExpression::CaseInsensitiveOption));
    });

    // Настраиваем layout и главный виджет
    QWidget mainWidget;
    QVBoxLayout layout;
    layout.addWidget(&filterInput);
    layout.addWidget(&tree);
    mainWidget.setLayout(&layout);

    mainWidget.show();

    return app.exec();
}
