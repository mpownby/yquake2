#include "bsp2rbx/RobloxXmlWriter.h"

#include <iomanip>
#include <stdexcept>

namespace bsp2rbx {

namespace {

uint32_t packColor3uint8(const std::array<uint8_t, 3>& rgb) {
    return (uint32_t{0xFF} << 24)
         | (uint32_t{rgb[0]} << 16)
         | (uint32_t{rgb[1]} <<  8)
         |  uint32_t{rgb[2]};
}

} // namespace

void RobloxXmlWriter::beginDocument() {
    if (inDocument_) {
        throw std::logic_error("RobloxXmlWriter: beginDocument called while already in document");
    }
    inDocument_ = true;
    nextReferent_ = 0;
    stream_.str({});
    stream_.clear();
    stream_ << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    stream_ << "<roblox xmlns:xmime=\"http://www.w3.org/2005/05/xmlmime\" "
               "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
               "xsi:noNamespaceSchemaLocation=\"http://www.roblox.com/roblox.xsd\" "
               "version=\"4\">\n";
    stream_ << "  <Item class=\"Model\" referent=\"RBX" << nextReferent_++ << "\">\n";
    stream_ << "    <Properties>\n";
    stream_ << "      <string name=\"Name\">Q2Map</string>\n";
    stream_ << "    </Properties>\n";
}

void RobloxXmlWriter::emitPart(const RobloxPart& part) {
    if (!inDocument_) {
        throw std::logic_error("RobloxXmlWriter: emitPart called before beginDocument");
    }
    emitBlock("Part", part.name, part.size, part.position, part.rotation, part.color);
}

void RobloxXmlWriter::emitWedge(const RobloxWedge& wedge) {
    if (!inDocument_) {
        throw std::logic_error("RobloxXmlWriter: emitWedge called before beginDocument");
    }
    emitBlock("WedgePart", wedge.name, wedge.size, wedge.position, wedge.rotation, wedge.color);
}

void RobloxXmlWriter::emitBlock(const char* className,
                                const std::string& name,
                                const std::array<float, 3>& size,
                                const std::array<float, 3>& position,
                                const std::array<float, 9>& rotation,
                                const std::array<uint8_t, 3>& color) {
    stream_ << std::fixed << std::setprecision(6);
    stream_ << "    <Item class=\"" << className << "\" referent=\"RBX" << nextReferent_++ << "\">\n";
    stream_ << "      <Properties>\n";
    stream_ << "        <string name=\"Name\">" << name << "</string>\n";
    stream_ << "        <bool name=\"Anchored\">true</bool>\n";
    stream_ << "        <Vector3 name=\"size\">"
            << "<X>" << size[0] << "</X>"
            << "<Y>" << size[1] << "</Y>"
            << "<Z>" << size[2] << "</Z>"
            << "</Vector3>\n";
    stream_ << "        <CoordinateFrame name=\"CFrame\">"
            << "<X>" << position[0] << "</X>"
            << "<Y>" << position[1] << "</Y>"
            << "<Z>" << position[2] << "</Z>"
            << "<R00>" << rotation[0] << "</R00>"
            << "<R01>" << rotation[1] << "</R01>"
            << "<R02>" << rotation[2] << "</R02>"
            << "<R10>" << rotation[3] << "</R10>"
            << "<R11>" << rotation[4] << "</R11>"
            << "<R12>" << rotation[5] << "</R12>"
            << "<R20>" << rotation[6] << "</R20>"
            << "<R21>" << rotation[7] << "</R21>"
            << "<R22>" << rotation[8] << "</R22>"
            << "</CoordinateFrame>\n";
    stream_ << "        <Color3uint8 name=\"Color3uint8\">"
            << packColor3uint8(color) << "</Color3uint8>\n";
    stream_ << "      </Properties>\n";
    stream_ << "    </Item>\n";
}

std::string RobloxXmlWriter::endDocument() {
    if (!inDocument_) {
        throw std::logic_error("RobloxXmlWriter: endDocument called before beginDocument");
    }
    stream_ << "  </Item>\n";
    stream_ << "</roblox>\n";
    inDocument_ = false;
    return stream_.str();
}

} // namespace bsp2rbx
