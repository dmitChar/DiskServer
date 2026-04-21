#pragma once

#include <QString>
#include <QJsonObject>
#include <optional>

using  std::optional;
using std::nullopt;

struct TokenPayload
{
    qint64 userId;
    QString username;
    qint64 issuedAt;
    qint64 expiresAt;
};

namespace JwtUtils
{
    //SHA256 JWT header.payload.signature
    QString createToken(const TokenPayload &payload, const QString &secret);
    optional<TokenPayload> verifyToken(const QString &token, const QString &secret);
    bool isExpired(const TokenPayload &payload);
}
