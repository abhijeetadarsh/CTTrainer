// ============================================================
//  CTParser.cpp
// ============================================================
#include "CTParser.h"
#include "Logger.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

static std::string Trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static bool ExtractTag(const std::string& src, const std::string& tag,
    size_t& pos, std::string& out)
{
    std::string open = "<" + tag + ">";
    std::string close = "</" + tag + ">";
    size_t a = src.find(open, pos);
    if (a == std::string::npos) return false;
    size_t b = src.find(close, a);
    if (b == std::string::npos) return false;
    out = Trim(src.substr(a + open.size(), b - a - open.size()));
    pos = b + close.size();
    return true;
}

static std::string StripQuotes(const std::string& s)
{
    std::string r = Trim(s);
    if (r.size() >= 2 && r.front() == '"' && r.back() == '"')
        r = r.substr(1, r.size() - 2);
    return r;
}

static void ParseAddress(const std::string& addr,
    std::string& modName, uintptr_t& offset)
{
    bool   inQuote = false;
    size_t plus = std::string::npos;
    for (size_t i = 0; i < addr.size(); ++i)
    {
        if (addr[i] == '"') { inQuote = !inQuote; continue; }
        if (!inQuote && addr[i] == '+') { plus = i; break; }
    }

    if (plus != std::string::npos)
    {
        modName = StripQuotes(addr.substr(0, plus));
        std::string offStr = Trim(addr.substr(plus + 1));
        try { offset = (uintptr_t)std::stoull(offStr, nullptr, 16); }
        catch (...) { offset = 0; }
    }
    else
    {
        modName = "";
        try { offset = (uintptr_t)std::stoull(StripQuotes(addr), nullptr, 16); }
        catch (...) { offset = 0; }
    }
}

namespace CTParser
{
    std::vector<CTEntry> Load(const std::string& filePath)
    {
        std::vector<CTEntry> entries;

        LOG_INFO("CTParser: loading file: " + filePath);

        std::ifstream file(filePath);
        if (!file.is_open())
        {
            LOG_ERR("CTParser: cannot open file: " + filePath);
            return entries;
        }

        std::stringstream buf;
        buf << file.rdbuf();
        std::string xml = buf.str();
        LOG_DEBUG("CTParser: file size = " + std::to_string(xml.size()) + " bytes");

        // Strip XML comments
        while (true)
        {
            size_t cs = xml.find("<!--");
            if (cs == std::string::npos) break;
            size_t ce = xml.find("-->", cs);
            if (ce == std::string::npos) break;
            xml.erase(cs, ce - cs + 3);
        }

        const std::string openEntry = "<CheatEntry>";
        const std::string closeEntry = "</CheatEntry>";
        size_t cur = 0;
        int    entryIndex = 0;

        while (true)
        {
            size_t blockStart = xml.find(openEntry, cur);
            if (blockStart == std::string::npos) break;
            size_t blockEnd = xml.find(closeEntry, blockStart);
            if (blockEnd == std::string::npos) break;

            std::string block = xml.substr(
                blockStart + openEntry.size(),
                blockEnd - blockStart - openEntry.size());
            cur = blockEnd + closeEntry.size();

            CTEntry entry;

            size_t p = 0;
            std::string desc;
            if (ExtractTag(block, "Description", p, desc))
                entry.description = StripQuotes(desc);

            p = 0;
            ExtractTag(block, "VariableType", p, entry.varType);

            p = 0;
            std::string addrStr;
            if (ExtractTag(block, "Address", p, addrStr))
            {
                LOG_DEBUG("CTParser: entry[" + std::to_string(entryIndex) +
                    "] raw address = '" + addrStr + "'");
                ParseAddress(addrStr, entry.moduleName, entry.baseOffset);
            }

            // Parse offsets -- CE lists them innermost-first, reverse for traversal
            size_t offBlockStart = block.find("<Offsets>");
            size_t offBlockEnd = block.find("</Offsets>");
            if (offBlockStart != std::string::npos &&
                offBlockEnd != std::string::npos)
            {
                std::string offBlock = block.substr(
                    offBlockStart + 9,
                    offBlockEnd - offBlockStart - 9);

                size_t op = 0;
                std::string offVal;
                while (ExtractTag(offBlock, "Offset", op, offVal))
                {
                    try {
                        entry.offsets.push_back(
                            (uintptr_t)std::stoull(offVal, nullptr, 16));
                    }
                    catch (...) {
                        LOG_WARN("CTParser: failed to parse offset value: '" + offVal + "'");
                    }
                }
                std::reverse(entry.offsets.begin(), entry.offsets.end());
            }

            // Log what we parsed
            char hexBuf[32];
            snprintf(hexBuf, sizeof(hexBuf), "0x%llX",
                (unsigned long long)entry.baseOffset);
            std::string offStr;
            for (auto& o : entry.offsets)
            {
                char b[16]; snprintf(b, sizeof(b), "0x%llX", (unsigned long long)o);
                offStr += std::string(b) + " ";
            }
            LOG_INFO("CTParser: entry[" + std::to_string(entryIndex) + "]"
                "  desc='" + entry.description + "'"
                "  module='" + entry.moduleName + "'"
                "  base=" + hexBuf +
                "  offsets=[ " + offStr + "]"
                "  type='" + entry.varType + "'");

            if (!entry.description.empty())
                entries.push_back(entry);

            ++entryIndex;
        }

        LOG_INFO("CTParser: loaded " + std::to_string(entries.size()) + " entries total");
        return entries;
    }
}