#pragma once

#include <sstream>

#include "bsp2rbx/IRobloxXmlWriter.h"

namespace bsp2rbx {

class RobloxXmlWriter : public IRobloxXmlWriter {
public:
    void beginDocument() override;
    void emitPart(const RobloxPart& part) override;
    void emitWedge(const RobloxWedge& wedge) override;
    std::string endDocument() override;

private:
    void emitBlock(const char* className,
                   const std::string& name,
                   const std::array<float, 3>& size,
                   const std::array<float, 3>& position,
                   const std::array<float, 9>& rotation,
                   const std::array<uint8_t, 3>& color);

    std::ostringstream stream_;
    int nextReferent_ = 0;
    bool inDocument_ = false;
};

} // namespace bsp2rbx
