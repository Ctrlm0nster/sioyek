// Focused compatibility checks: persisted checksum identity and UTF-8 iteration.
#include "checksum.h"
#include "utf8.h"
#include <QByteArrayView>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <vector>

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

template<class Iterator>
void check_iteration(Iterator begin, Iterator end) {
    using Traits = std::iterator_traits<Iterator>;
    static_assert(std::is_same_v<typename Traits::value_type, utf8::uint32_t>);
    static_assert(std::is_same_v<typename Traits::difference_type, std::ptrdiff_t>);
    static_assert(std::is_same_v<typename Traits::iterator_category, std::bidirectional_iterator_tag>);
    const std::vector<utf8::uint32_t> expected = {0x41, 0xe9, 0x20ac, 0x1f642};
    require(std::vector<utf8::uint32_t>(begin, end) == expected, "UTF-8 decode changed");
    require(std::distance(begin, end) == 4, "UTF-8 distance changed");
    auto it = end;
    for (auto cp = expected.rbegin(); cp != expected.rend(); ++cp) {
        require(*--it == *cp, "UTF-8 reverse iteration changed");
    }
    require(it == begin, "UTF-8 reverse endpoint changed");
    std::advance(it, 3);
    require(*it == expected[3], "UTF-8 advance changed");
    std::advance(it, -2);
    require(*it == expected[1], "UTF-8 negative advance changed");
    require(*it++ == expected[1] && *it == expected[2], "UTF-8 postincrement changed");
    require(*it-- == expected[2] && *it == expected[1], "UTF-8 postdecrement changed");
}

int main(int argc, char** argv) {
    try {
        QTemporaryDir temp;
        require(temp.isValid(), "Temporary directory unavailable");
        const QString path = temp.filePath("checksum.bin");
        // Empty, partial, exact and multiple 10 KiB chunks, with embedded NULs.
        for (int size : {0, 1, 10239, 10240, 10241, 30721}) {
            QByteArray bytes(size, '\0');
            for (int i = 0; i < size; ++i) bytes[i] = static_cast<char>(i % 256);
            QFile file(path);
            require(file.open(QIODevice::WriteOnly), "Could not write checksum fixture");
            require(file.write(bytes) == bytes.size(), "Short fixture write");
            file.close();
            for (auto algorithm : {QCryptographicHash::Md5, QCryptographicHash::Sha256}) {
                auto expected = QCryptographicHash::hash(QByteArrayView(bytes), algorithm).toHex().toStdString();
                require(compute_checksum(path, algorithm) == expected, "Persisted checksum identity changed");
            }
        }
        require(compute_checksum(temp.filePath("missing"), QCryptographicHash::Md5).empty(),
                "Missing-file behavior changed");
        if (argc == 2) {
            require(compute_checksum(QString::fromLocal8Bit(argv[1]), QCryptographicHash::Md5)
                    == "0bf8c44b14f9a20803ad4b5daed25688", "Tutorial document identity changed");
        }
        const std::string text = "A\xc3\xa9\xe2\x82\xac\xf0\x9f\x99\x82";
        using Base = std::string::const_iterator;
        check_iteration(utf8::iterator<Base>(text.begin(), text.begin(), text.end()),
                        utf8::iterator<Base>(text.end(), text.begin(), text.end()));
        check_iteration(utf8::unchecked::iterator<Base>(text.begin()),
                        utf8::unchecked::iterator<Base>(text.end()));
        const std::string invalid = "\xff";
        bool rejected = false;
        try {
            (void)*utf8::iterator<Base>(invalid.begin(), invalid.begin(), invalid.end());
        } catch (const utf8::invalid_utf8&) {
            rejected = true;
        }
        require(rejected, "Checked UTF-8 no longer rejects invalid input");
        std::cout << "PASS checksum compatibility, chunk boundaries, UTF-8 forward/reverse iteration and validation\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
