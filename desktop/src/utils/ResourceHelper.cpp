#include "ResourceHelper.h"

#include <QFile>

std::vector<uint8_t> ResourceHelper::GetResource(const std::string &url) {
    auto file = QFile(QString::fromUtf8(url));
    if(!file.open(QIODevice::ReadOnly))
        throw std::runtime_error(fmt::format("Failed to open resource '{}'.", url));
    auto data = file.readAll();
    return {data.begin(), data.end()};
}
