#include "protocol.h"

namespace Proto {

QByteArray encode(quint8 command, const QByteArray &payload)
{
    int len = nibbleLength(command);
    if (len == 0x0F)
        len = payload.isEmpty() ? 0 : quint8(payload.at(0));

    QByteArray out;
    out.append(char(command));
    if (len == 0)
        return out;                         // firmware dispatches immediately, no checksum

    QByteArray body = payload.left(len);
    body.append(QByteArray(len - body.size(), '\0'));

    quint8 checksum = command;
    for (char c : body)
        checksum ^= quint8(c);

    out.append(body);
    out.append(char(checksum));
    return out;
}

QString toHex(const QByteArray &bytes)
{
    return QString::fromLatin1(bytes.toHex(' ')).toUpper();
}

QString nackReason(quint8 reason)
{
    switch (reason)
    {
    case NACK_INVALID_CHECKSUM: return QStringLiteral("invalid checksum");
    case NACK_INVALID_LENGTH:   return QStringLiteral("invalid length");
    case NACK_TIMEOUT:          return QStringLiteral("byte timeout");
    }
    return QStringLiteral("unknown reason %1").arg(reason);
}

QString commandName(quint8 command)
{
    switch (command)
    {
    case SET_INPUT_DATA:         return QStringLiteral("SET_INPUT_DATA");
    case GET_INPUT_DATA:         return QStringLiteral("GET_INPUT_DATA");
    case SET_OUTPUT_DATA:        return QStringLiteral("SET_OUTPUT_DATA");
    case SET_OUTPUT_DATA_LEGACY: return QStringLiteral("SET_OUTPUT_DATA (legacy 13 B)");
    case GET_OUTPUT_DATA:        return QStringLiteral("GET_OUTPUT_DATA");
    case SET_INPUT_STATE:        return QStringLiteral("SET_INPUT_STATE");
    case GET_INPUT_STATE:        return QStringLiteral("GET_INPUT_STATE");
    case SET_OUTPUT_STATE:       return QStringLiteral("SET_OUTPUT_STATE");
    case GET_OUTPUT_STATE:       return QStringLiteral("GET_OUTPUT_STATE");
    case REQ_VERSION:            return QStringLiteral("REQ_VERSION");
    case SET_PRESET:             return QStringLiteral("SET_PRESET");
    case ACK:                    return QStringLiteral("ACK");
    case NACK:                   return QStringLiteral("NACK");
    case HELLO:                  return QStringLiteral("HELLO");
    case GET_ROUTE:              return QStringLiteral("GET_ROUTE");
    case SET_ROUTE:              return QStringLiteral("SET_ROUTE");
    case SERVO_LIVE:             return QStringLiteral("SERVO_LIVE");
    case GET_BOARD:              return QStringLiteral("GET_BOARD");
    case SET_BOARD:              return QStringLiteral("SET_BOARD");
    }
    if ((command >> 4) == HELLO_GROUP)
        return QStringLiteral("HELLO_REPLY");
    if ((command >> 4) == VERSION_GROUP)
        return QStringLiteral("VERSION");
    return QStringLiteral("0x%1").arg(command, 2, 16, QLatin1Char('0'));
}

void FrameParser::reset()
{
    m_state = Idle;
    m_payload.clear();
}

void FrameParser::feed(const QByteArray &bytes, Result &out)
{
    for (char c : bytes)
    {
        const quint8 b = quint8(c);

        switch (m_state)
        {
        case Idle:
            if (b == ACK)
            {
                Frame f; f.command = ACK;
                out.frames.append(f);
                break;
            }
            if (b == NACK)
            {
                m_state = NackReason;
                break;
            }
            m_command  = b;
            m_checksum = b;
            m_length   = nibbleLength(b);
            m_payload.clear();
            if      (m_length == 0)    m_state = Checksum;   // firmware sendMessage() still appends a checksum
            else if (m_length == 0x0F) m_state = ExtLength;
            else                       m_state = Payload;
            break;

        case NackReason:
        {
            Frame f; f.command = NACK; f.payload.append(char(b));
            out.frames.append(f);
            m_state = Idle;
            break;
        }

        case ExtLength:
            if (b == 0 || b > MaxLen)
            {
                out.errors.append(QStringLiteral("invalid extended length %1").arg(b));
                m_state = Idle;
                break;
            }
            m_length = b;                       // length includes this byte
            m_payload.append(char(b));
            m_checksum ^= b;
            m_state = (m_payload.size() == m_length) ? Checksum : Payload;
            break;

        case Payload:
            m_payload.append(char(b));
            m_checksum ^= b;
            if (m_payload.size() == m_length)
                m_state = Checksum;
            break;

        case Checksum:
            if (b == m_checksum)
            {
                Frame f; f.command = m_command; f.payload = m_payload;
                out.frames.append(f);
            }
            else
            {
                out.errors.append(QStringLiteral("checksum error on %1 (got %2, expected %3)")
                                      .arg(commandName(m_command))
                                      .arg(b, 2, 16, QLatin1Char('0'))
                                      .arg(m_checksum, 2, 16, QLatin1Char('0')));
            }
            m_state = Idle;
            m_payload.clear();
            break;
        }
    }
}

} // namespace Proto
