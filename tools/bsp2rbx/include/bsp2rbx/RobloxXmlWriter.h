#pragma once

#include <sstream>

#include "bsp2rbx/IRobloxXmlWriter.h"

namespace bsp2rbx {

class RobloxXmlWriter : public IRobloxXmlWriter {
public:
    void beginDocument() override;
    void emitPart(const RobloxPart& part) override;
    std::string endDocument() override;

private:
    std::ostringstream stream_;
    int nextReferent_ = 0;
    bool inDocument_ = false;
};

} // namespace bsp2rbx
