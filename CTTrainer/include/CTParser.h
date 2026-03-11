#pragma once
// ============================================================
//  CTParser.h  –  Parse Cheat Engine .CT (XML) export
//
//  Expected CT structure per entry:
//  <CheatEntry>
//    <Description>"Health"</Description>
//    <VariableType>4 Bytes</VariableType>
//    <Address>FC3.exe+1A2B3C</Address>
//    <Offsets>
//      <Offset>10</Offset>
//      <Offset>4C</Offset>
//    </Offsets>
//  </CheatEntry>
// ============================================================
#include <string>
#include <vector>

struct CTEntry
{
    std::string description;   // "Health", "Ammo", etc.
    std::string moduleName;    // "FC3.exe"
    uintptr_t   baseOffset;    // static offset from module base
    std::vector<uintptr_t> offsets; // pointer chain offsets
    std::string varType;       // "4 Bytes", "Float", etc.
};

namespace CTParser
{
    // Load and parse a .CT file, returns all found entries
    std::vector<CTEntry> Load(const std::string& filePath);
}