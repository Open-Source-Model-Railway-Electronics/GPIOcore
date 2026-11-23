#pragma once
// Minimal QR code encoder for short texts (URLs): byte mode, error correction level L,
// versions 1-5 (up to 106 bytes), mask 0. Follows ISO/IEC 18004 and the structure of
// Nayuki's public QR code generator. Used for the phone-remote URL.

#include <QByteArray>
#include <QVector>

namespace Qr {

// Square matrix, true = dark module, without the quiet zone. Empty when the text is too long.
QVector<QVector<bool>> encode(const QByteArray &text);

} // namespace Qr
