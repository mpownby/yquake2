#pragma once

#include <gmock/gmock.h>

#include "bsp2rbx/IRobloxXmlWriter.h"

namespace bsp2rbx {

class MockRobloxXmlWriter : public IRobloxXmlWriter {
public:
    MOCK_METHOD(void, beginDocument, (), (override));
    MOCK_METHOD(void, emitPart, (const RobloxPart& part), (override));
    MOCK_METHOD(void, emitWedge, (const RobloxWedge& wedge), (override));
    MOCK_METHOD(std::string, endDocument, (), (override));
};

} // namespace bsp2rbx
