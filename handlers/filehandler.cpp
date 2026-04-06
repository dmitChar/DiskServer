#include "filehandler.h"

FileHandler::FileHandler(DatabaseManager *db, AuthMiddleware *auth, const QString &storageRoot, qint64 maxFileSizeBytes, QObject *parent) : QObject(parent)
{

}
