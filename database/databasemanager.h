#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlQuery>

class DatabaseManager : public QObject
{
public:
    DatabaseManager(QObject *parent);
};

#endif // DATABASEMANAGER_H
