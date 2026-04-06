#ifndef HASHUTILS_H
#define HASHUTILS_H

#include <QObject>
#include <QRandomGenerator>
#include <QByteArray>
#include <QCryptographicHash>

namespace HashUtils
{
    QString generateSalt()
    {
        const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        QString salt;
        salt.reserve(16);

        for (int i = 0; i < 16; ++i)
        {
            salt += chars[QRandomGenerator::global()->bounded(chars.size())];
        }
        return salt;
    }

    QString hashPassword(const QString &password, const QString &salt)
    {
        QString salted = salt + password + salt;
        return QCryptographicHash::hash(salted.toUtf8(), QCryptographicHash::Sha256).toHex();
    }

    bool verifyPassword(const QString &password, const QString &hash, const QString &salt)
    {
        return hashPassword(password, salt) == hash;
    }

    QString generateToken(int length)
    {
        const QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        QString token;
        token.reserve(length);

        for (int i = 0; i < length; ++i)
        {
            token += chars[QRandomGenerator::global()->bounded(chars.size())];
        }
        return token;
    }
}
#endif // HASHUTILS_H
