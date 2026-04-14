#include "jwtutils.h"
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QByteArray>
#include <QJsonDocument>
#include <QStringList>
#include <QVariant>
#include <QDateTime>

namespace JwtUtils
{
    static QByteArray base64UrlEncode(const QByteArray &data)
    {
        return data.toBase64(QByteArray::Base64Encoding | QByteArray::OmitTrailingEquals);
    }

    static QByteArray base64UrlDecode(const QByteArray &data)
    {
        QByteArray padded = data;
        while (padded.size() % 4 != 0)
        {
            padded += '=';
        }
        return QByteArray::fromBase64(padded, QByteArray::Base64UrlEncoding);
    }


    QString createToken(const TokenPayload &payload, const QString &secret)
    {
        //header
        QJsonObject headerObj;
        headerObj["alg"] = "HS256";
        headerObj["typ"] = "JWT";
        QByteArray header = base64UrlEncode(QJsonDocument(headerObj).toJson(QJsonDocument::Compact));

        //Payload
        QJsonObject payloadObj;
        payloadObj["uid"] = payload.userId;
        payloadObj["usr"] = payload.username;
        payloadObj["iat"] = payload.issuedAt;
        payloadObj["exp"] = payload.expiresAt;
        QByteArray body = base64UrlEncode(QJsonDocument(payloadObj).toJson(QJsonDocument::Compact)) ;

        //Signature
        QByteArray signingInput = header + '.' + body;
        QByteArray sig = QMessageAuthenticationCode::hash(signingInput, secret.toUtf8(), QCryptographicHash::Sha256);

        return QString::fromLatin1(header + '.' + body + '.' + base64UrlEncode(sig));
    }

    optional<TokenPayload> verifyToken(const QString &token, const QString &secret)
    {
        QStringList parts = token.split('.');
        if (parts.size() != 3) return nullopt;

        QByteArray header = parts[0].toLatin1();
        QByteArray body = parts[1].toLatin1();
        QByteArray sig = parts[2].toLatin1();

        //Verify signature
        QByteArray sigInput = header + '.' + body;
        QByteArray expectedSig = base64UrlEncode(QMessageAuthenticationCode::hash(sigInput, secret.toUtf8(), QCryptographicHash::Sha256));

        if (expectedSig != sig)
            return nullopt;

        //Decode payload
        QByteArray payloadJson = base64UrlDecode(body);
        QJsonDocument doc = QJsonDocument::fromJson(payloadJson);
//        if (doc.isNull() || doc.isObject())
//            return nullopt;

        QJsonObject obj = doc.object();
        TokenPayload result;
        result.userId = obj["uid"].toVariant().toLongLong();
        result.username = obj["usr"].toString();
        result.issuedAt = obj["iat"].toVariant().toLongLong();
        result.expiresAt = obj["exp"].toVariant().toLongLong();

        return result;
    }

    bool isExpired(const TokenPayload &payload)
    {
        qint64 now = QDateTime::currentSecsSinceEpoch();
        return now > payload.expiresAt;
    }

}
