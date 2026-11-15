/*
 * SPDX-FileCopyrightText: 2026 techoraye
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

// Archives for the archive tests, written byte by byte at test time so the
// repository carries no binary sample: TAR (ustar), ZIP (stored, optionally
// with the classic ZIP password), RAR 4 (stored). Being written by hand,
// they can hold what no archiver would make: "../" names, absolute names,
// links out of the tree, duplicate names.

#include <QByteArray>
#include <QList>
#include <QString>

#include <array>
#include <cstdint>

namespace Fixtures
{

inline uint32_t crc32(const QByteArray &data, uint32_t crc = 0)
{
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            t[i] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (const char ch : data) {
        crc = table[(crc ^ uint8_t(ch)) & 0xff] ^ (crc >> 8);
    }
    return ~crc;
}

inline void put16(QByteArray &b, uint32_t v)
{
    b.append(char(v & 0xff));
    b.append(char((v >> 8) & 0xff));
}

inline void put32(QByteArray &b, uint32_t v)
{
    put16(b, v & 0xffff);
    put16(b, v >> 16);
}

struct Item {
    enum Kind {
        File,
        Folder,
        Symlink,
        Hardlink,
        // a GNU sparse file of realSize bytes holding no data at all
        Sparse,
        Fifo,
    };
    QString name;
    QByteArray data;
    Kind kind = File;
    QString link;
    unsigned mode = 0644;
    qint64 realSize = 0;
};

inline Item file(const QString &name, const QByteArray &data, unsigned mode = 0644)
{
    Item i;
    i.name = name;
    i.data = data;
    i.mode = mode;
    return i;
}

inline Item folder(const QString &name, unsigned mode = 0755)
{
    Item i;
    i.name = name;
    i.kind = Item::Folder;
    i.mode = mode;
    return i;
}

inline Item symlink(const QString &name, const QString &target)
{
    Item i;
    i.name = name;
    i.kind = Item::Symlink;
    i.link = target;
    i.mode = 0777;
    return i;
}

inline Item hardlink(const QString &name, const QString &target)
{
    Item i;
    i.name = name;
    i.kind = Item::Hardlink;
    i.link = target;
    return i;
}

inline Item sparse(const QString &name, qint64 size)
{
    Item i;
    i.name = name;
    i.kind = Item::Sparse;
    i.realSize = size;
    return i;
}

inline Item fifo(const QString &name)
{
    Item i;
    i.name = name;
    i.kind = Item::Fifo;
    return i;
}

// ---- TAR (ustar) ---------------------------------------------------------

inline QByteArray tar(const QList<Item> &items)
{
    QByteArray out;
    for (const Item &item : items) {
        QByteArray header(512, '\0');
        const QByteArray name = item.name.toUtf8();
        Q_ASSERT(name.size() < 100);
        header.replace(0, name.size(), name);
        auto octal = [&header](int offset, int width, qint64 value) {
            const QByteArray digits = QByteArray::number(value, 8).rightJustified(width - 1, '0');
            header.replace(offset, digits.size(), digits);
        };
        octal(100, 8, item.mode);
        octal(108, 8, 1000);
        octal(116, 8, 1000);
        const bool hasData = item.kind == Item::File;
        octal(124, 12, hasData ? item.data.size() : 0);
        octal(136, 12, 1700000000);
        static const char types[] = {'0', '5', '2', '1', 'S', '6'};
        header[156] = types[item.kind];
        if (item.kind == Item::Sparse) {
            // one map entry: no data, at the very end of the file
            octal(386, 12, item.realSize);
            octal(398, 12, 0);
            octal(483, 12, item.realSize);
        }
        const QByteArray link = item.link.toUtf8();
        header.replace(157, link.size(), link);
        if (item.kind == Item::Sparse) {
            // sparse files only exist in the GNU flavour
            header.replace(257, 8, QByteArray("ustar  \0", 8));
        } else {
            header.replace(257, 6, QByteArray("ustar\0", 6));
            header.replace(263, 2, "00");
        }
        header.replace(148, 8, QByteArray(8, ' '));
        unsigned sum = 0;
        for (const char c : std::as_const(header)) {
            sum += uint8_t(c);
        }
        const QByteArray chk = QByteArray::number(sum, 8).rightJustified(6, '0');
        header.replace(148, 6, chk);
        header[154] = '\0';
        header[155] = ' ';
        out.append(header);
        if (hasData) {
            out.append(item.data);
            out.append(QByteArray((512 - item.data.size() % 512) % 512, '\0'));
        }
    }
    out.append(QByteArray(1024, '\0'));
    return out;
}

// ---- ZIP (stored) ---------------------------------------------------------

// The classic ZIP password ("ZipCrypto"), enough to test the prompt.
class ZipCrypto
{
public:
    explicit ZipCrypto(const QByteArray &password)
    {
        for (const char c : password) {
            update(uint8_t(c));
        }
    }
    QByteArray encrypt(const QByteArray &plain)
    {
        QByteArray out(plain.size(), '\0');
        for (int i = 0; i < plain.size(); ++i) {
            const uint8_t p = uint8_t(plain.at(i));
            out[i] = char(p ^ streamByte());
            update(p);
        }
        return out;
    }

private:
    uint8_t streamByte() const
    {
        const uint16_t t = uint16_t(m_k2 | 2);
        return uint8_t((t * (t ^ 1)) >> 8);
    }
    void update(uint8_t c)
    {
        m_k0 = crcByte(m_k0, c);
        m_k1 = (m_k1 + (m_k0 & 0xff)) * 134775813u + 1;
        m_k2 = crcByte(m_k2, uint8_t(m_k1 >> 24));
    }
    static uint32_t crcByte(uint32_t crc, uint8_t c)
    {
        // one step of the CRC-32 register, without the final inversion
        return ~crc32(QByteArray(1, char(c)), ~crc);
    }
    uint32_t m_k0 = 0x12345678u;
    uint32_t m_k1 = 0x23456789u;
    uint32_t m_k2 = 0x34567890u;
};

inline QByteArray zip(const QList<Item> &items, const QByteArray &password = {})
{
    QByteArray out;
    QByteArray central;
    for (const Item &item : items) {
        const QByteArray name = (item.kind == Item::Folder && !item.name.endsWith(QLatin1Char('/')) ? item.name + QLatin1Char('/') : item.name).toUtf8();
        const QByteArray plain = item.kind == Item::Symlink ? item.link.toUtf8() : item.data;
        const uint32_t crc = crc32(plain);
        QByteArray stored = plain;
        uint16_t flags = 0x0800; // UTF-8 names
        if (!password.isEmpty() && item.kind == Item::File) {
            ZipCrypto crypto(password);
            QByteArray head(12, '\0');
            for (int i = 0; i < 11; ++i) {
                head[i] = char(0x5a + i * 17);
            }
            head[11] = char(crc >> 24);
            stored = crypto.encrypt(head + plain);
            flags |= 0x0001;
        }
        const uint32_t offset = out.size();
        out.append("PK\x03\x04", 4);
        put16(out, 20);
        put16(out, flags);
        put16(out, 0); // stored
        put16(out, 0);
        put16(out, 0x5821);
        put32(out, crc);
        put32(out, stored.size());
        put32(out, plain.size());
        put16(out, name.size());
        put16(out, 0);
        out.append(name);
        out.append(stored);

        unsigned type = item.kind == Item::Folder ? 0040000 : item.kind == Item::Symlink ? 0120000 : 0100000;
        central.append("PK\x01\x02", 4);
        put16(central, 0x0314); // made on Unix
        put16(central, 20);
        put16(central, flags);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0x5821);
        put32(central, crc);
        put32(central, stored.size());
        put32(central, plain.size());
        put16(central, name.size());
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, (type | item.mode) << 16);
        put32(central, offset);
        central.append(name);
    }
    const uint32_t centralOffset = out.size();
    out.append(central);
    out.append("PK\x05\x06", 4);
    put16(out, 0);
    put16(out, 0);
    put16(out, items.size());
    put16(out, items.size());
    put32(out, central.size());
    put32(out, centralOffset);
    put16(out, 0);
    return out;
}

// ---- RAR 4 (stored) -------------------------------------------------------

inline QByteArray rarBlock(uint8_t type, uint16_t flags, const QByteArray &rest)
{
    QByteArray body;
    body.append(char(type));
    put16(body, flags);
    put16(body, 7 + rest.size());
    body.append(rest);
    QByteArray block;
    put16(block, crc32(body) & 0xffff);
    block.append(body);
    return block;
}

inline QByteArray rar(const QList<Item> &items)
{
    QByteArray out("Rar!\x1a\x07\x00", 7);
    QByteArray main;
    put16(main, 0);
    put32(main, 0);
    out.append(rarBlock(0x73, 0, main));
    for (const Item &item : items) {
        const QByteArray name = item.name.toUtf8();
        QByteArray h;
        put32(h, item.data.size()); // packed
        put32(h, item.data.size()); // unpacked
        h.append(char(3)); // made on Unix
        put32(h, crc32(item.data));
        put32(h, 0x5b6e0000); // DOS date and time
        h.append(char(20));
        h.append(char(0x30)); // stored
        put16(h, name.size());
        put32(h, (item.kind == Item::Folder ? 0040000u : 0100000u) | item.mode);
        h.append(name);
        const uint16_t flags = 0x8000 | (item.kind == Item::Folder ? 0x00e0 : 0);
        out.append(rarBlock(0x74, flags, h));
        out.append(item.data);
    }
    out.append(rarBlock(0x7b, 0x4000, QByteArray()));
    return out;
}

}
