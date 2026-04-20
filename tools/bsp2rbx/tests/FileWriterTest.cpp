#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "bsp2rbx/FileWriter.h"

namespace bsp2rbx {
namespace {

class FileWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() /
               ("bsp2rbx_filewriter_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(tmp_, ec);
    }
    std::filesystem::path tmp_;
};

TEST_F(FileWriterTest, WritesContentExactly) {
    const std::string payload = "<roblox>hello</roblox>";
    FileWriter writer;
    writer.write(tmp_, payload);

    std::ifstream in(tmp_, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    EXPECT_EQ(ss.str(), payload);
}

TEST_F(FileWriterTest, OverwritesExistingFile) {
    {
        std::ofstream out(tmp_, std::ios::binary);
        out << "stale content that should be replaced";
    }
    FileWriter writer;
    writer.write(tmp_, "new");

    std::ifstream in(tmp_, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    EXPECT_EQ(ss.str(), "new");
}

TEST_F(FileWriterTest, ThrowsOnUnwritablePath) {
    FileWriter writer;
    const std::filesystem::path bad = tmp_ / "no_such_dir" / "file.txt";
    EXPECT_THROW(writer.write(bad, "x"), std::runtime_error);
}

} // namespace
} // namespace bsp2rbx
