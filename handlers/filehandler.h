#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include "database/databasemanager.h"
#include "middleware/authmiddleware.h"
#include "utils/fileutils.h"

class FileHandler : public QObject
{
    Q_OBJECT
public:
    explicit FileHandler(DatabaseManager *db, AuthMiddleware *auth, const QString &storageRoot,
                         qint64 maxFileSizeBytes, QObject *parent = nullptr);



};

#endif // FILEHANDLER_H
