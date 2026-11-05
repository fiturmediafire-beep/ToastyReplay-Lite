#include "TtrlStorage.hpp"

#include <asp/fs.hpp>
#include <asp/iter.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/utils/StringBuffer.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <utility>

using namespace geode::prelude;

namespace toasty::replay::ttrl {
    constexpr size_t MaximumReplayFiles = 4096;
    constexpr size_t MaximumNameAttempts = 10000;

    static auto failure(StorageError error,
                        std::string detail = {},
                        std::optional<codec::CodecFailure> codec = std::nullopt) {
        return Err(StorageFailure{error, std::move(detail), codec});
    }

    static char asciiLower(char value) {
        auto byte = static_cast<unsigned char>(value);
        if (byte >= 'A' && byte <= 'Z')
            return static_cast<char>(byte - 'A' + 'a');
        return static_cast<char>(byte);
    }

    static bool endsWithTtrl(ZStringView value) {
        auto view = value.view();
        if (view.size() < 5)
            return false;

        constexpr std::string_view extension = ".ttrl";
        auto suffix = view.substr(view.size() - extension.size());
        for (size_t index = 0; index < extension.size(); ++index) {
            if (asciiLower(suffix[index]) != extension[index])
                return false;
        }
        return true;
    }

    static bool reservedName(ZStringView value) {
        std::string lower(value.view());
        for (auto& character : lower) {
            character = asciiLower(character);
        }
        auto device = ZStringView(lower).view().substr(0, lower.find('.'));

        if (device == "con" || device == "prn" || device == "aux" || device == "nul" ||
            device == "clock$") {
            return true;
        }
        if (device.size() != 4)
            return false;
        if (device[3] < '1' || device[3] > '9')
            return false;
        return device.starts_with("com") || device.starts_with("lpt");
    }

    static std::string replayBaseName(ZStringView input) {
        auto view = input.view();
        if (endsWithTtrl(input))
            view.remove_suffix(5);

        StringBuffer<MaximumReplayName> buffer;
        bool previousSpace = false;

        for (auto byte : asp::iter::from(view)) {
            if (buffer.size() == MaximumReplayName)
                break;

            auto value = static_cast<unsigned char>(byte);
            if (value == ' ' || value == '\t') {
                if (buffer.size() > 0 && !previousSpace)
                    buffer.append(' ');
                previousSpace = true;
                continue;
            }

            previousSpace = false;
            if (std::isalnum(value) || value == '-' || value == '_' || value == '.' ||
                value == '(' || value == ')' || value == '[' || value == ']') {
                buffer.append(static_cast<char>(value));
            } else {
                buffer.append('_');
            }
        }

        auto str = buffer.str();
        utils::string::trimIP(str, " .");
        if (str.empty())
            str = "Replay";
        if (reservedName(str))
            str.push_back('_');
        return str;
    }

    static std::string replayFileName(ZStringView name, size_t index) {
        StringBuffer<256> buf;
        auto base = replayBaseName(name);
        if (index != 0) {
            buf.append("{} ({})", base, index + 1);
        } else {
            buf.append("{}", base);
        }
        buf.append(".ttrl");
        return buf.str();
    }

    static std::string replayLoadFileName(ZStringView name) {
        auto view = name.view();
        auto extension = endsWithTtrl(name) ? view.substr(view.size() - 5) : std::string_view(".ttrl");
        StringBuffer<256> buf;
        buf.append("{}{}", replayBaseName(name), extension);
        return buf.str();
    }

    static std::filesystem::path toStd(asp::fs::Path const& p) {
    return std::filesystem::path(p.string());
    }

    static Result<void, StorageFailure> ensureDirectory(asp::fs::Path const& directory) {
        std::error_code ec;
        auto stdPath = toStd(directory);
        auto status = std::filesystem::status(stdPath, ec);
        if (!ec) {
            if (std::filesystem::is_directory(status))
                return Ok();
            return failure(StorageError::DirectoryUnavailable);
        }
        if (ec != std::errc::no_such_file_or_directory) {
            return failure(StorageError::DirectoryUnavailable, ec.message());
        }
        std::filesystem::create_directories(stdPath, ec);
        if (ec) {
            return failure(StorageError::DirectoryUnavailable, ec.message());
        }
        return Ok();
    }

    static Result<asp::fs::Path, StorageFailure> availablePath(asp::fs::Path const& directory,
                                                               ZStringView name) {
        for (size_t index = 0; index < MaximumNameAttempts; ++index) {
            auto path = directory / replayFileName(name, index);
            std::error_code ec;
            std::filesystem::status(toStd(path), ec);
            if (ec == std::errc::no_such_file_or_directory) {
                return Ok(path);
            }
            if (ec) {
                return failure(StorageError::DirectoryUnavailable, ec.message());
            }
        }
        return failure(StorageError::TooManyFiles);
    }

    static std::string pathFileName(asp::fs::Path const& path) {
        return toStd(path).filename().string();
    }

    static bool validReplayFileName(ZStringView fileName) {
        auto view = fileName.view();
        if (!endsWithTtrl(fileName) || view.find('/') != std::string_view::npos ||
            view.find('\\') != std::string_view::npos || view.find(':') != std::string_view::npos) {
            return false;
        }
        return replayLoadFileName(fileName) == std::string(view);
    }

    Storage::Storage(asp::fs::Path directory)
        : m_directory(std::move(directory)) {}

    asp::fs::Path const& Storage::directory() const {
        return m_directory;
    }

    SaveResult Storage::save(ZStringView name, Replay const& replay) const {
        GEODE_UNWRAP(ensureDirectory(m_directory));
        auto encoded = codec::encode(replay);
        if (encoded.isErr()) {
            return failure(StorageError::InvalidReplay, {}, encoded.unwrapErr());
        }
        GEODE_UNWRAP_INTO(auto target, availablePath(m_directory, name));
        auto writeRes = utils::file::writeBinarySafe(target, encoded.unwrap());
        if (writeRes.isErr()) {
            return failure(StorageError::WriteFailed, writeRes.unwrapErr());
        }
        return Ok(pathFileName(target));
    }

    SaveResult Storage::importFile(asp::fs::Path const& source) const {
        GEODE_UNWRAP(ensureDirectory(m_directory));
        std::error_code ec;
        auto stdSource = toStd(source);
        auto status = std::filesystem::status(stdSource, ec);
        if (ec) {
            if (ec == std::errc::no_such_file_or_directory)
                return failure(StorageError::FileNotFound);
            return failure(StorageError::ReadFailed, ec.message());
        }
        if (std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status))
            return failure(StorageError::InvalidFile);
        auto readRes = utils::file::readBinary(source);
        if (readRes.isErr())
            return failure(StorageError::ReadFailed, readRes.unwrapErr());
        auto bytes = std::move(readRes.unwrap());
        if (bytes.size() > codec::MaximumFileSize)
            return failure(StorageError::FileTooLarge);
        auto replay = codec::decode(bytes);
        if (replay.isErr())
            return failure(StorageError::InvalidReplay, {}, replay.unwrapErr());
        GEODE_UNWRAP_INTO(auto target, availablePath(m_directory, pathFileName(source)));
        auto writeRes = utils::file::writeBinarySafe(target, bytes);
        if (writeRes.isErr())
            return failure(StorageError::WriteFailed, writeRes.unwrapErr());
        return Ok(pathFileName(target));
    }

    LoadResult Storage::load(ZStringView fileName) const {
        GEODE_UNWRAP(ensureDirectory(m_directory));
        auto path = m_directory / replayLoadFileName(fileName);
        std::error_code ec;
        auto status = std::filesystem::status(toStd(path), ec);
        if (ec) {
            if (ec == std::errc::no_such_file_or_directory)
                return failure(StorageError::FileNotFound);
            return failure(StorageError::ReadFailed, ec.message());
        }
        if (std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status))
            return failure(StorageError::InvalidFile);
        auto readRes = utils::file::readBinary(path);
        if (readRes.isErr())
            return failure(StorageError::ReadFailed, readRes.unwrapErr());
        auto bytes = std::move(readRes.unwrap());
        if (bytes.size() > codec::MaximumFileSize)
            return failure(StorageError::FileTooLarge);
        auto replay = codec::decode(bytes);
        if (replay.isErr())
            return failure(StorageError::InvalidReplay, {}, replay.unwrapErr());
        return Ok(std::move(replay.unwrap()));
    }

    RemoveResult Storage::remove(ZStringView fileName) const {
        if (!validReplayFileName(fileName))
            return failure(StorageError::InvalidFile);
        GEODE_UNWRAP(ensureDirectory(m_directory));
        auto path = m_directory / std::string(fileName.view());
        std::error_code ec;
        auto status = std::filesystem::status(toStd(path), ec);
        if (ec) {
            if (ec == std::errc::no_such_file_or_directory)
                return failure(StorageError::FileNotFound);
            return failure(StorageError::DeleteFailed, ec.message());
        }
        if (std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status))
            return failure(StorageError::InvalidFile);
        std::filesystem::remove(toStd(path), ec);
        if (ec)
            return failure(StorageError::DeleteFailed, ec.message());
        return Ok();
    }

    RenameResult Storage::rename(ZStringView fileName, ZStringView name) const {
        if (!validReplayFileName(fileName))
            return failure(StorageError::InvalidFile);
        GEODE_UNWRAP(ensureDirectory(m_directory));
        auto source = m_directory / std::string(fileName.view());
        std::error_code ec;
        auto stdSource = toStd(source);
        auto sourceStatus = std::filesystem::status(stdSource, ec);
        if (ec) {
            if (ec == std::errc::no_such_file_or_directory)
                return failure(StorageError::FileNotFound);
            return failure(StorageError::RenameFailed, ec.message());
        }
        if (std::filesystem::is_symlink(sourceStatus) || !std::filesystem::is_regular_file(sourceStatus))
            return failure(StorageError::InvalidFile);
        auto targetName = replayFileName(name, 0);
        if (targetName == std::string(fileName.view()))
            return Ok(std::move(targetName));
        auto stdTarget = toStd(m_directory / targetName);
        std::filesystem::status(stdTarget, ec);
        if (!ec)
            return failure(StorageError::NameTaken);
        if (ec != std::errc::no_such_file_or_directory)
            return failure(StorageError::RenameFailed, ec.message());
        std::filesystem::rename(stdSource, stdTarget, ec);
        if (ec)
            return failure(StorageError::RenameFailed, ec.message());
        return Ok(std::move(targetName));
    }

    ListResult Storage::list() const {
        GEODE_UNWRAP(ensureDirectory(m_directory));
        std::error_code ec;
        std::filesystem::directory_iterator dirIt(toStd(m_directory), ec);
        if (ec)
            return failure(StorageError::DirectoryUnavailable, ec.message());
        std::vector<std::pair<std::filesystem::file_time_type, std::string>> files;
        for (auto const& entry : dirIt) {
            if (!entry.is_regular_file())
                continue;
            auto name = entry.path().filename().string();
            if (!endsWithTtrl(name))
                continue;
            if (files.size() == MaximumReplayFiles)
                return failure(StorageError::TooManyFiles);
            auto writeTime = entry.last_write_time(ec);
            if (ec) writeTime = std::filesystem::file_time_type::min();
            files.emplace_back(writeTime, std::move(name));
        }
        std::sort(files.begin(), files.end(), [](auto const& left, auto const& right) {
            if (left.first != right.first)
                return left.first > right.first;
            return left.second < right.second;
        });
        std::vector<std::string> names;
        names.reserve(files.size());
        for (auto& file : files)
            names.push_back(std::move(file.second));
        return Ok(std::move(names));
    }

    std::string displayName(ZStringView fileName) {
        auto view = fileName.view();
        if (endsWithTtrl(fileName))
            view.remove_suffix(5);
        return std::string(view);
    }

    ZStringView errorMessage(StorageError error) {
        switch (error) {
        case StorageError::DirectoryUnavailable: return "The replay directory is unavailable";
        case StorageError::TooManyFiles:         return "There are too many replay files";
        case StorageError::FileNotFound:         return "The replay file was not found";
        case StorageError::NameTaken:            return "A replay already uses that name";
        case StorageError::InvalidFile:          return "The replay path is not a regular file";
        case StorageError::FileTooLarge:         return "The replay file is too large";
        case StorageError::OpenFailed:           return "The replay file could not be opened";
        case StorageError::ReadFailed:           return "The replay file could not be read";
        case StorageError::WriteFailed:          return "The replay file could not be written";
        case StorageError::RenameFailed:         return "The replay file could not be finalized";
        case StorageError::DeleteFailed:         return "The replay file could not be deleted";
        case StorageError::InvalidReplay:        return "The replay data is invalid";
        }
        return "The replay operation failed";
    }

    std::string describe(StorageFailure const& failure) {
        StringBuffer<256> msg;
        msg.append("{}", errorMessage(failure.error));
        if (failure.codec)
            msg.append(": {}", codec::errorMessage(failure.codec->error));
        if (!failure.detail.empty())
            msg.append(": {}", failure.detail);
        return msg.str();
    }
} // namespace toasty::replay::ttrl
