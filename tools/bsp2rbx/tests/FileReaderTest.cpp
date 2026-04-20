#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "bsp2rbx/FileReader.h"

namespace bsp2rbx {
namespace {

class FileReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() /
               ("bsp2rbx_filereader_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(tmp_, ec);
    }
    std::filesystem::path tmp_;
};

TEST_F(FileReaderTest, ReadsBackExactlyWhatWasWritten) {
    const std::vector<uint8_t> payload{0x01, 0x02, 0x03, 0xFE, 0xFF, 0x00, 0x7F};
    {
        std::ofstream out(tmp_, std::ios::binary);
        out.write(reinterpret_cast<const char*>(payload.data()),
                  static_cast<std::streamsize>(payload.size()));
    }

    FileReader reader;
    EXPECT_EQ(reader.read(tmp_), payload);
}

TEST_F(FileReaderTest, ReadsEmptyFile) {
    { std::ofstream out(tmp_, std::ios::binary); }

    FileReader reader;
    EXPECT_TRUE(reader.read(tmp_).empty());
}

TEST_F(FileReaderTest, ThrowsWhenMissing) {
    FileReader reader;
    EXPECT_THROW(reader.read(tmp_ / "does_not_exist"), std::runtime_error);
}

} // namespace
} // namespace bsp2rbx
