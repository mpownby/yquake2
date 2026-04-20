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

    stream_ << std::fixed << std::setprecision(6);
    stream_ << "    <Item class=\"Part\" referent=\"RBX" << nextReferent_++ << "\">\n";
    stream_ << "      <Properties>\n";
    stream_ << "        <string name=\"Name\">" << part.name << "</string>\n";
    stream_ << "        <bool name=\"Anchored\">true</bool>\n";
    stream_ << "        <Vector3 name=\"size\">"
            << "<X>" << part.size[0] << "</X>"
            << "<Y>" << part.size[1] << "</Y>"
            << "<Z>" << part.size[2] << "</Z>"
            << "</Vector3>\n";
    stream_ << "        <CoordinateFrame name=\"CFrame\">"
            << "<X>" << part.position[0] << "</X>"
            << "<Y>" << part.position[1] << "</Y>"
            << "<Z>" << part.position[2] << "</Z>"
            << "<R00>" << part.rotation[0] << "</R00>"
            << "<R01>" << part.rotation[1] << "</R01>"
            << "<R02>" << part.rotation[2] << "</R02>"
            << "<R10>" << part.rotation[3] << "</R10>"
            << "<R11>" << part.rotation[4] << "</R11>"
            << "<R12>" << part.rotation[5] << "</R12>"
            << "<R20>" << part.rotation[6] << "</R20>"
            << "<R21>" << part.rotation[7] << "</R21>"
            << "<R22>" << part.rotation[8] << "</R22>"
            << "</CoordinateFrame>\n";
    stream_ << "        <Color3uint8 name=\"Color3uint8\">"
            << packColor3uint8(part.color) << "</Color3uint8>\n";
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
