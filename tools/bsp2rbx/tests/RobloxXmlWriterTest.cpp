#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "bsp2rbx/RobloxXmlWriter.h"

namespace bsp2rbx {
namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

TEST(RobloxXmlWriterTest, BeginEndProducesRobloxShell) {
    RobloxXmlWriter w;
    w.beginDocument();
    const std::string xml = w.endDocument();

    EXPECT_TRUE(contains(xml, "<?xml"));
    EXPECT_TRUE(contains(xml, "<roblox "));
    EXPECT_TRUE(contains(xml, "</roblox>"));
    EXPECT_TRUE(contains(xml, "<Item class=\"Model\""));
    EXPECT_TRUE(contains(xml, "<string name=\"Name\">Q2Map</string>"));
}

TEST(RobloxXmlWriterTest, EmitsPartProperties) {
    RobloxXmlWriter w;
    w.beginDocument();
    RobloxPart p{};
    p.position = { 1.0f, 2.0f, 3.0f };
    p.size     = { 4.0f, 5.0f, 6.0f };
    p.color    = { 255, 0, 0 };
    p.name     = "brush_42";
    w.emitPart(p);
    const std::string xml = w.endDocument();

    EXPECT_TRUE(contains(xml, "<Item class=\"Part\""));
    EXPECT_TRUE(contains(xml, "<string name=\"Name\">brush_42</string>"));
    EXPECT_TRUE(contains(xml, "<Vector3 name=\"size\">"));
    EXPECT_TRUE(contains(xml, "<CoordinateFrame name=\"CFrame\">"));
    EXPECT_TRUE(contains(xml, "<bool name=\"Anchored\">true</bool>"));
    // 0xFF000000 | (255<<16) | 0 | 0 = 4294901760
    EXPECT_TRUE(contains(xml, "<Color3uint8 name=\"Color3uint8\">4294901760</Color3uint8>"));
}

TEST(RobloxXmlWriterTest, EmitPartWithoutBeginThrows) {
    RobloxXmlWriter w;
    RobloxPart p{};
    EXPECT_THROW(w.emitPart(p), std::logic_error);
}

TEST(RobloxXmlWriterTest, EndWithoutBeginThrows) {
    RobloxXmlWriter w;
    EXPECT_THROW(w.endDocument(), std::logic_error);
}

TEST(RobloxXmlWriterTest, CanReuseAcrossDocuments) {
    RobloxXmlWriter w;
    w.beginDocument();
    const std::string a = w.endDocument();
    w.beginDocument();
    const std::string b = w.endDocument();
    EXPECT_EQ(a, b);
}

TEST(RobloxXmlWriterTest, ReferentsAreUnique) {
    RobloxXmlWriter w;
    w.beginDocument();
    RobloxPart p{};
    p.name = "b0";
    w.emitPart(p);
    p.name = "b1";
    w.emitPart(p);
    const std::string xml = w.endDocument();
    EXPECT_TRUE(contains(xml, "referent=\"RBX0\""));
    EXPECT_TRUE(contains(xml, "referent=\"RBX1\""));
    EXPECT_TRUE(contains(xml, "referent=\"RBX2\""));
}

} // namespace
} // namespace bsp2rbx
