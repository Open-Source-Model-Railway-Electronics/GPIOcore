#include "qrcode.h"

namespace Qr {
namespace {

// Error correction level L, one block per version (versions 1-5)
constexpr int kDataCodewords[6] = { 0, 19, 34, 55, 80, 108 };
constexpr int kEccCodewords[6]  = { 0,  7, 10, 15, 20,  26 };

quint8 gfMultiply(quint8 x, quint8 y)
{
    // Russian peasant multiplication in GF(2^8) modulo x^8 + x^4 + x^3 + x^2 + 1 (0x11D)
    int z = 0;
    for (int i = 7; i >= 0; --i)
    {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return quint8(z);
}

QVector<quint8> rsDivisor(int degree)
{
    QVector<quint8> result(degree, 0);
    result[degree - 1] = 1;
    quint8 root = 1;
    for (int i = 0; i < degree; ++i)
    {
        for (int j = 0; j < degree; ++j)
        {
            result[j] = gfMultiply(result[j], root);
            if (j + 1 < degree)
                result[j] ^= result[j + 1];
        }
        root = gfMultiply(root, 0x02);
    }
    return result;
}

QVector<quint8> rsRemainder(const QVector<quint8> &data, const QVector<quint8> &divisor)
{
    QVector<quint8> result(divisor.size(), 0);
    for (quint8 b : data)
    {
        const quint8 factor = b ^ result.takeFirst();
        result.append(0);
        for (int i = 0; i < divisor.size(); ++i)
            result[i] ^= gfMultiply(divisor[i], factor);
    }
    return result;
}

struct Matrix
{
    int size = 0;
    QVector<QVector<bool>> dark, function;

    explicit Matrix(int n) : size(n), dark(n, QVector<bool>(n, false)), function(n, QVector<bool>(n, false)) {}
    void set(int x, int y, bool d) { dark[y][x] = d; function[y][x] = true; }   // x = column, y = row
};

void drawFinder(Matrix &m, int cx, int cy)
{
    for (int dy = -4; dy <= 4; ++dy)
        for (int dx = -4; dx <= 4; ++dx)
        {
            const int x = cx + dx, y = cy + dy;
            if (x < 0 || y < 0 || x >= m.size || y >= m.size)
                continue;
            const int dist = qMax(qAbs(dx), qAbs(dy));
            m.set(x, y, dist != 2 && dist != 4);
        }
}

void drawAlignment(Matrix &m, int cx, int cy)
{
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx)
            m.set(cx + dx, cy + dy, qMax(qAbs(dx), qAbs(dy)) != 1);
}

void drawFormat(Matrix &m)
{
    const int data = (1 << 3) | 0;          // level L (format bits 01), mask 0
    int rem = data;
    for (int i = 0; i < 10; ++i)
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    const int bits = ((data << 10) | rem) ^ 0x5412;
    auto bit = [bits](int i) { return ((bits >> i) & 1) != 0; };

    for (int i = 0; i <= 5; ++i) m.set(8, i, bit(i));
    m.set(8, 7, bit(6));
    m.set(8, 8, bit(7));
    m.set(7, 8, bit(8));
    for (int i = 9; i < 15; ++i) m.set(14 - i, 8, bit(i));
    for (int i = 0; i < 8; ++i)  m.set(m.size - 1 - i, 8, bit(i));
    for (int i = 8; i < 15; ++i) m.set(8, m.size - 15 + i, bit(i));
    m.set(8, m.size - 8, true);             // the dark module
}

} // namespace

QVector<QVector<bool>> encode(const QByteArray &text)
{
    int version = 0;
    for (int v = 1; v <= 5; ++v)
        if (4 + 8 + 8 * text.size() <= kDataCodewords[v] * 8) { version = v; break; }
    if (version == 0)
        return {};

    // ── data bits: mode 0100 (byte), 8-bit count, the bytes, terminator, padding
    QVector<bool> bits;
    auto append = [&bits](int value, int n) { for (int i = n - 1; i >= 0; --i) bits.append(((value >> i) & 1) != 0); };
    append(0x4, 4);
    append(text.size(), 8);
    for (char c : text)
        append(quint8(c), 8);
    const int capacity = kDataCodewords[version] * 8;
    append(0, qMin(4, capacity - int(bits.size())));
    while (bits.size() % 8)
        bits.append(false);
    for (quint8 pad = 0xEC; bits.size() < capacity; pad ^= 0xEC ^ 0x11)
        append(pad, 8);

    QVector<quint8> codewords(bits.size() / 8, 0);
    for (int i = 0; i < bits.size(); ++i)
        if (bits[i]) codewords[i >> 3] |= quint8(1 << (7 - (i & 7)));
    codewords += rsRemainder(codewords, rsDivisor(kEccCodewords[version]));

    // ── function patterns
    Matrix m(17 + 4 * version);
    for (int i = 0; i < m.size; ++i)
    {
        m.set(6, i, i % 2 == 0);           // timing patterns
        m.set(i, 6, i % 2 == 0);
    }
    drawFinder(m, 3, 3);
    drawFinder(m, m.size - 4, 3);
    drawFinder(m, 3, m.size - 4);
    if (version >= 2)
        drawAlignment(m, 4 * version + 10, 4 * version + 10);
    drawFormat(m);                          // also reserves the format areas

    // ── codewords in the zigzag order
    int i = 0;
    const int total = codewords.size() * 8;
    for (int right = m.size - 1; right >= 1; right -= 2)
    {
        if (right == 6)
            right = 5;
        for (int vert = 0; vert < m.size; ++vert)
            for (int j = 0; j < 2; ++j)
            {
                const int x = right - j;
                const bool upward = ((right + 1) & 2) == 0;
                const int y = upward ? m.size - 1 - vert : vert;
                if (!m.function[y][x] && i < total)
                {
                    m.dark[y][x] = ((codewords[i >> 3] >> (7 - (i & 7))) & 1) != 0;
                    ++i;
                }
            }
    }

    // ── mask 0: invert where (x + y) is even
    for (int y = 0; y < m.size; ++y)
        for (int x = 0; x < m.size; ++x)
            if (!m.function[y][x] && (x + y) % 2 == 0)
                m.dark[y][x] = !m.dark[y][x];
    return m.dark;
}

} // namespace Qr
