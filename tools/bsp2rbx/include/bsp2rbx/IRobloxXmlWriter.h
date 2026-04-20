#pragma once

#include <string>

#include "bsp2rbx/Bsp.h"

namespace bsp2rbx {

class IRobloxXmlWriter {
public:
    virtual ~IRobloxXmlWriter() = default;
    virtual void beginDocument() = 0;
    virtual void emitPart(const RobloxPart& part) = 0;
    virtual void emitWedge(const RobloxWedge& wedge) = 0;
    virtual std::string endDocument() = 0;
};

} // namespace bsp2rbx
