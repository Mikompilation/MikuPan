#include "mikupan_textoverride.h"

#include "enums.h"
#include "main/glob.h"
#include "mikupan/debug/mikupan_logging.h"
#include "mikupan/io/mikupan_file.h"
#include "mikupan_config.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

#ifdef BUILD_EU_VERSION
const char *kLanguageCode[5] = {"en", "fr", "de", "es", "it"};
const int kLanguageCount = 5;
#else
const char *kLanguageCode[1] = {"en"};
const int kLanguageCount = 1;
#endif

int ClampLanguage(int lang)
{
    return (lang >= 0 && lang < kLanguageCount) ? lang : 0;
}

bool IsItalian(int lang)
{
    return kLanguageCount == 5 && lang == 4;
}

std::filesystem::path GameTextModsDirectory(int lang)
{
    return std::filesystem::path(MikuPan_GetBaseDirectory()) / "resources"
        / "mods" / "text" / kLanguageCode[ClampLanguage(lang)];
}

const int64_t kSectorSize = 0x800;
const size_t kImgArrangementEntrySize = 8;

const size_t kEventHeaderSize = 0x14;
const size_t kEventHeaderSlots[] = {0x04, 0x08, 0x0C, 0x10};

struct EventTable
{
    size_t header_offset;
    const char *label;
};

const EventTable kEventTables[] = {
    {0x08, "msg"},
    {0x0C, "movie"},
    {0x10, "special"},
};

u_int ReadUint32LE(const u_char *p)
{
    return (u_int) p[0] | ((u_int) p[1] << 8) | ((u_int) p[2] << 16)
        | ((u_int) p[3] << 24);
}

bool ReadIgMsgFile(int lang, std::vector<u_char> &out)
{
#ifdef BUILD_EU_VERSION
    const int file_no = (int) IG_MSG_E_OBJ + lang;
#else
    const int file_no = (int) IG_MSG_OBJ;
#endif
    const int64_t entry_offset = (int64_t) file_no * kImgArrangementEntrySize;

    u_char entry[kImgArrangementEntrySize];
    if (!MikuPan_ReadDataFileRange("IMG_HD.BIN", entry_offset, entry,
                                   sizeof(entry)))
    {
        info_log("MikuPan_GameText: could not read IMG_HD.BIN entry for "
                 "file_no %d", file_no);
        return false;
    }

    const u_int start_sector = ReadUint32LE(entry);
    const u_int size = ReadUint32LE(entry + 4);

    if (size == 0 || size > 64u * 1024u * 1024u)
    {
        info_log("MikuPan_GameText: implausible ig_msg size %u for file_no "
                 "%d, aborting", size, file_no);
        return false;
    }

    out.resize(size);
    if (!MikuPan_ReadDataFileRange("IMG_BD.BIN",
                                   (int64_t) start_sector * kSectorSize,
                                   out.data(), out.size()))
    {
        info_log("MikuPan_GameText: could not read %u bytes from IMG_BD.BIN "
                 "at sector %u", size, start_sector);
        return false;
    }

    return true;
}

bool ReadEventFile(int map, int lang, std::vector<u_char> &out)
{
#ifdef BUILD_EU_VERSION
    const int file_no = (int) M0_EVENT_E_OBJ + map * 5 + lang;
#else
    const int file_no = (int) M0_EVENT_OBJ + map;
#endif
    const int64_t entry_offset = (int64_t) file_no * kImgArrangementEntrySize;

    u_char entry[kImgArrangementEntrySize];
    if (!MikuPan_ReadDataFileRange("IMG_HD.BIN", entry_offset, entry,
                                   sizeof(entry)))
    {
        info_log("MikuPan_GameText: could not read IMG_HD.BIN entry for "
                 "file_no %d", file_no);
        return false;
    }

    const u_int start_sector = ReadUint32LE(entry);
    const u_int size = ReadUint32LE(entry + 4);

    if (size == 0 || size > 64u * 1024u * 1024u)
    {
        info_log("MikuPan_GameText: implausible m%d_event size %u for "
                 "file_no %d, aborting", map, size, file_no);
        return false;
    }

    out.resize(size);
    if (!MikuPan_ReadDataFileRange("IMG_BD.BIN",
                                   (int64_t) start_sector * kSectorSize,
                                   out.data(), out.size()))
    {
        info_log("MikuPan_GameText: could not read %u bytes from IMG_BD.BIN "
                 "at sector %u", size, start_sector);
        return false;
    }

    return true;
}

int CategoryCount(const std::vector<u_char> &buf)
{
    if (buf.size() < 4) return 0;

    const u_int first_entry = ReadUint32LE(&buf[0]);
    if (first_entry == 0 || first_entry % 4 != 0
        || first_entry > buf.size())
    {
        return 0;
    }

    return (int) (first_entry / 4);
}

bool ResolveCategorySubTableRange(const std::vector<u_char> &buf, u_char type,
                                  u_int *start, u_int *end)
{
    const int category_count = CategoryCount(buf);
    if (type >= category_count) return false;

    const int64_t entry_offset = (int64_t) type * 4;
    if (entry_offset + 4 > (int64_t) buf.size()) return false;

    *start = ReadUint32LE(&buf[entry_offset]);

    u_int closest_greater = (u_int) buf.size();
    for (int i = 0; i < category_count; i++)
    {
        if (i == type) continue;

        const u_int candidate = ReadUint32LE(&buf[(int64_t) i * 4]);
        if (candidate > *start && candidate < closest_greater)
        {
            closest_greater = candidate;
        }
    }

    *end = closest_greater;

    return *end >= *start && *end <= buf.size();
}

bool ResolveEventTableRange(const std::vector<u_char> &buf, u_int start,
                            u_int *end)
{
    u_int closest_greater = (u_int) buf.size();
    for (size_t slot : kEventHeaderSlots)
    {
        const u_int candidate = ReadUint32LE(&buf[slot]);
        if (candidate > start && candidate < closest_greater)
        {
            closest_greater = candidate;
        }
    }

    *end = closest_greater;
    return *end >= start && *end <= buf.size();
}

int64_t ResolveMessageOffset(const std::vector<u_char> &buf,
                             u_int sub_table_start, int msg_no)
{
    const int64_t p = (int64_t) sub_table_start + (int64_t) msg_no * 4;
    if (p < 0 || p + 4 > (int64_t) buf.size()) return -1;

    const u_int string_offset = ReadUint32LE(&buf[p]);
    if (string_offset >= buf.size()) return -1;

    return (int64_t) string_offset;
}

const u_char kTerminator = 0xFF;
const int kMaxDecodedChars = 4096;

std::string DecodeGameChar(u_char c)
{
    if (c == 0x00) return " ";

    if (c >= 0x01 && c <= 0x1A) return std::string(1, (char) ('A' + (c - 0x01)));

    if (c >= 0x1B && c <= 0x34) return std::string(1, (char) ('a' + (c - 0x1B)));

    if (c >= 0x35 && c <= 0x3E) // digits '0'-'9', equal-width run right after 'z' in font_w_b0_e.h
    {
        return std::string(1, (char) ('0' + (c - 0x35)));
    }

    if (c >= 0x3F && c <= 0x48) // second digit range, confirmed via "Type-14/90 Film" and taped items
    {
        return std::string(1, (char) ('0' + (c - 0x3F)));
    }

    switch (c)
    {
    case 0x50: return "{ICON:LOCK?}"; // tentative, door-locked hint prefix
    case 0x52: return "\xC3\xA0"; // a-grave
    case 0x54: return "\xC3\xA9"; // e-acute
    case 0x55: return "\xC3\xAA"; // e-circumflex
    case 0x56: return "\xC3\xAE"; // i-circumflex
    case 0x5B: return "\xC3\x9F"; // sharp s
    case 0x60: return "\xC3\xA4"; // a-umlaut
    case 0x63: return "\xC3\xBC"; // u-umlaut
    case 0x64: return "\xC2\xA1"; // inverted exclamation mark
    case 0x65: return "\xC2\xBF"; // inverted question mark
    case 0x66: return "\xC3\x81"; // A-acute
    case 0x67: return "\xC3\x89"; // E-acute
    case 0x68: return "\xC3\x8D"; // I-acute
    case 0x69: return "\xC3\x91"; // N-tilde
    case 0x6A: return "\xC3\x93"; // O-acute
    case 0x6B: return "\xC3\x9A"; // U-acute
    case 0x6C: return "\xC3\xA1"; // a-acute
    case 0x6D: return "\xC3\xA9"; // e-acute
    case 0x6E: return "\xC3\xAD"; // i-acute
    case 0x6F: return "\xC3\xB1"; // n-tilde
    case 0x70: return "\xC3\xB3"; // o-acute
    case 0x71: return "\xC3\xBA"; // u-acute
    case 0x81: return "ch"; // Italian "ch" ligature
    case 0x83: return "rr"; // Italian "rr" ligature
    case 0x8B: return "'";
    case 0x8E: return "-";
    case 0x8F: return "?";
    case 0x90: return " "; // narrow space
    case 0x94: return ":";
    case 0x95: return ",";
    case 0x96: return ".";
    case 0x97: return "!";
    case 0x9D: return "\xC3\xA2"; // a-circumflex
    case 0xA4: return "\xC3\xB6"; // o-umlaut
    default: break;
    }

    char buf[8];
    std::snprintf(buf, sizeof(buf), "{%02X}", c);
    return buf;
}

int HexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool ParseHexByte(const std::string &s, size_t pos, u_char *out)
{
    if (pos + 2 > s.size()) return false;

    const int hi = HexDigit(s[pos]);
    const int lo = HexDigit(s[pos + 1]);
    if (hi < 0 || lo < 0) return false;

    *out = (u_char) ((hi << 4) | lo);
    return true;
}

bool ParseHexByteList(const std::string &s, size_t pos, int count,
                      std::vector<u_char> &out)
{
    for (int i = 0; i < count; i++)
    {
        u_char b;
        if (!ParseHexByte(s, pos, &b)) return false;

        out.push_back(b);
        pos += 2;

        if (i + 1 < count)
        {
            if (pos >= s.size() || s[pos] != ':') return false;
            pos += 1;
        }
    }

    return true;
}

struct AccentEntry
{
    const char *utf8;
    u_char code;
};

const AccentEntry kAccents[] = {
    {"\xC3\xA0", 0x52}, {"\xC3\xA9", 0x54}, {"\xC3\xAA", 0x55}, {"\xC3\xAE", 0x56},
    {"\xC3\x9F", 0x5B}, {"\xC3\xA4", 0x60}, {"\xC3\xBC", 0x63}, {"\xC2\xA1", 0x64},
    {"\xC2\xBF", 0x65}, {"\xC3\x81", 0x66}, {"\xC3\x89", 0x67}, {"\xC3\x8D", 0x68},
    {"\xC3\x91", 0x69}, {"\xC3\x93", 0x6A}, {"\xC3\x9A", 0x6B}, {"\xC3\xA1", 0x6C},
    {"\xC3\xAD", 0x6E}, {"\xC3\xB1", 0x6F}, {"\xC3\xB3", 0x70}, {"\xC3\xBA", 0x71},
    {"\xC3\xA2", 0x9D}, {"\xC3\xB6", 0xA4},
};

struct ControlCode
{
    u_char opcode;
    int param_count;
    const char *name;
};

const ControlCode kControlCodes[] = {
    {0xF0, 1, "ICON0"},
    {0xF1, 1, "ICON1"},
    {0xF2, 1, "ICON2"},
    {0xF3, 1, "ICON3"},
    {0xF5, 3, "VIBRATE"},
    {0xF6, 4, "COLOR2"},
    {0xF7, 1, "XSPACE"},
    {0xF8, 1, "YSPACE"},
    {0xF9, 3, "SELPOS"},
    {0xFB, 2, "NUM"},
    {0xFD, 3, "COLOR"},
};

bool EncodeControlCode(const std::string &token, std::vector<u_char> &out)
{
    if (token == "PARABREAK")
    {
        out.push_back(0xFA);
        return true;
    }

    if (token == "ICON:LOCK?")
    {
        out.push_back(0x50);
        return true;
    }

    if (token.size() == 7 && token.rfind("ICON:", 0) == 0)
    {
        u_char param;
        if (!ParseHexByte(token, 5, &param)) return false;
        out.insert(out.end(), {0x50, 0x00, param, 0x01, 0x00, 0x00});
        return true;
    }

    for (const ControlCode &code : kControlCodes)
    {
        const std::string prefix = std::string(code.name) + ":";
        if (token.rfind(prefix, 0) != 0) continue;

        out.push_back(code.opcode);
        return ParseHexByteList(token, prefix.size(), code.param_count, out);
    }

    u_char raw;
    if (token.size() == 2 && ParseHexByte(token, 0, &raw)) // generic {XX} fallback for a single unmapped byte
    {
        out.push_back(raw);
        return true;
    }

    return false; // includes {FC?}, which has no safe byte-length to reproduce
}

bool EncodeOneToken(const std::string &text, size_t i, int lang,
                    std::vector<u_char> &out, size_t *consumed)
{
    const char c = text[i];

    if (c == '\\' && i + 1 < text.size() && text[i + 1] == 'n')
    {
        out.push_back(0xFE);
        *consumed = 2;
        return true;
    }

    if (c == '{')
    {
        const size_t close = text.find('}', i + 1);
        if (close == std::string::npos) return false;

        if (!EncodeControlCode(text.substr(i + 1, close - i - 1), out)) return false;

        *consumed = close - i + 1;
        return true;
    }

    const bool italian = IsItalian(lang);

    if (italian && c == 'c' && i + 1 < text.size() && text[i + 1] == 'h')
    {
        out.push_back(0x81);
        *consumed = 2;
        return true;
    }

    if (italian && c == 'r' && i + 1 < text.size() && text[i + 1] == 'r')
    {
        out.push_back(0x83);
        *consumed = 2;
        return true;
    }

    if (c == ' ') { out.push_back(0x00); *consumed = 1; return true; }
    if (c >= 'A' && c <= 'Z') { out.push_back((u_char) (0x01 + (c - 'A'))); *consumed = 1; return true; }
    if (c >= 'a' && c <= 'z') { out.push_back((u_char) (0x1B + (c - 'a'))); *consumed = 1; return true; }
    if (c >= '0' && c <= '9') { out.push_back((u_char) (0x35 + (c - '0'))); *consumed = 1; return true; }

    switch (c)
    {
    case '\'': out.push_back(0x8B); *consumed = 1; return true;
    case '-':  out.push_back(0x8E); *consumed = 1; return true;
    case '?':  out.push_back(0x8F); *consumed = 1; return true;
    case ':':  out.push_back(0x94); *consumed = 1; return true;
    case ',':  out.push_back(0x95); *consumed = 1; return true;
    case '.':  out.push_back(0x96); *consumed = 1; return true;
    case '!':  out.push_back(0x97); *consumed = 1; return true;
    default: break;
    }

    for (const AccentEntry &accent : kAccents)
    {
        const size_t len = std::strlen(accent.utf8);
        if (text.compare(i, len, accent.utf8) == 0)
        {
            out.push_back(accent.code);
            *consumed = len;
            return true;
        }
    }

    return false; // no known encoding for this character
}

int64_t AppendControlCode(const std::vector<u_char> &buf, int64_t i, std::string &out)
{
    for (const ControlCode &code : kControlCodes)
    {
        if (buf[i] != code.opcode) continue;
        if (i + 1 + code.param_count > (int64_t) buf.size()) return -1;

        out += '{';
        out += code.name;
        for (int p = 0; p < code.param_count; p++)
        {
            char tmp[4];
            std::snprintf(tmp, sizeof(tmp), ":%02X", buf[i + 1 + p]);
            out += tmp;
        }
        out += '}';

        return 1 + code.param_count;
    }

    return 0;
}

std::string DecodeGameString(const std::vector<u_char> &buf, int64_t start)
{
    std::string out;
    int guard = 0;
    int64_t i = start;

    for (; guard < kMaxDecodedChars && i < (int64_t) buf.size(); guard++)
    {
        u_char c = buf[i];

        if (c == kTerminator) break;

        // Fixed 6-byte icon header "50 00 XX 01 00 00", XX is the icon-type parameter.
        if (c == 0x50 && i + 6 <= (int64_t) buf.size()
            && buf[i + 1] == 0x00 && buf[i + 3] == 0x01
            && buf[i + 4] == 0x00 && buf[i + 5] == 0x00)
        {
            char tmp[16];
            std::snprintf(tmp, sizeof(tmp), "{ICON:%02X}", buf[i + 2]);
            out += tmp;
            i += 6;
            continue;
        }

        if (c == 0xFE)
        {
            out += "\\n";
            i += 1;
            continue;
        }

        if (c == 0xFA) // paragraph/page break, distinct from the plain linebreak at 0xFE
        {
            out += "{PARABREAK}";
            i += 1;
            continue;
        }

        const int64_t code_len = AppendControlCode(buf, i, out);
        if (code_len > 0) { i += code_len; continue; }
        if (code_len < 0) break; // known control code but not enough bytes left for its parameters

        if (c == 0xFC) // message.cpp never advances past this code, treat as unreachable
        {
            out += "{FC?}";
            break;
        }

        out += DecodeGameChar(c);
        i += 1;
    }

    return out;
}

bool EncodeGameString(const std::string &text, int lang, std::vector<u_char> &out)
{
    out.clear();

    size_t i = 0;
    while (i < text.size())
    {
        size_t consumed = 0;
        if (!EncodeOneToken(text, i, lang, out, &consumed) || consumed == 0) return false;
        i += consumed;
    }

    out.push_back(kTerminator);
    return true;
}

void ExtractTable(const std::vector<u_char> &buf, const std::string &prefix,
                  u_int start, u_int end, std::string &contents,
                  int &extracted)
{
    const int entry_count = (int) ((end - start) / 4);
    for (int msg_no = 0; msg_no < entry_count; msg_no++)
    {
        const int64_t offset = ResolveMessageOffset(buf, start, msg_no);
        if (offset < 0) break;

        const std::string text = DecodeGameString(buf, offset);
        if (text.empty()) continue;

        contents += "msgid \"" + prefix + "#" + std::to_string(msg_no) + "\"\n";
        contents += "msgstr \"" + text + "\"\n\n";
        extracted++;
    }
}

void ExtractIgMsg(int lang, std::string &contents, int &extracted)
{
    std::vector<u_char> ig_msg;
    if (!ReadIgMsgFile(lang, ig_msg))
    {
        info_log("MikuPan_GameText: could not read ig_msg for lang %d, "
                 "skipping", lang);
        return;
    }

    const int category_count = CategoryCount(ig_msg);
    for (int type = 0; type < category_count; type++)
    {
        if (type == 32) continue; // IGMSG_FURN_NAME

        u_int start = 0;
        u_int end = 0;
        if (!ResolveCategorySubTableRange(ig_msg, (u_char) type, &start, &end))
        {
            info_log("MikuPan_GameText: could not resolve category bounds "
                     "for category %d, lang %d", type, lang);
            continue;
        }

        ExtractTable(ig_msg, "ig_msg#" + std::to_string(type), start, end,
                    contents, extracted);
    }
}

void ExtractEventMap(int map, int lang, std::string &contents, int &extracted)
{
    std::vector<u_char> event_buf;
    if (!ReadEventFile(map, lang, event_buf) || event_buf.size() < kEventHeaderSize)
    {
        info_log("MikuPan_GameText: could not read m%d_event for lang %d, "
                 "skipping", map, lang);
        return;
    }

    for (const EventTable &table : kEventTables)
    {
        const u_int start = ReadUint32LE(&event_buf[table.header_offset]);
        u_int end = 0;
        if (start >= event_buf.size() || !ResolveEventTableRange(event_buf, start, &end)) continue;

        ExtractTable(event_buf, "m" + std::to_string(map) + "_event#"
                    + table.label, start, end, contents, extracted);
    }
}

void ExtractLanguage(int lang)
{
    std::string contents;
    contents += "# Extracted from ig_msg_" + std::string(kLanguageCode[lang])
              + ".obj and m0..m4_event_" + std::string(kLanguageCode[lang])
              + ".obj\n\n";

    int extracted = 0;

    ExtractIgMsg(lang, contents, extracted);

    for (int map = 0; map < 5; map++)
    {
        ExtractEventMap(map, lang, contents, extracted);
    }

    const std::filesystem::path out_dir = GameTextModsDirectory(lang);
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    if (ec)
    {
        info_log("MikuPan_GameText: could not create %s (%s)",
                 out_dir.generic_string().c_str(), ec.message().c_str());
        return;
    }

    const std::filesystem::path out_path = out_dir / "game_text.po";
    std::ofstream file(out_path, std::ios::binary);
    if (!file.is_open())
    {
        info_log("MikuPan_GameText: could not open %s for writing",
                 out_path.generic_string().c_str());
        return;
    }

    file << contents;
    info_log("MikuPan_GameText: wrote %d entries to %s", extracted,
             out_path.generic_string().c_str());
}

bool ParsePoQuotedSegment(const std::string &line, size_t quote_pos, std::string &out)
{
    size_t i = quote_pos + 1;
    while (i < line.size())
    {
        const char c = line[i];
        if (c == '"') return true;

        if (c == '\\' && i + 1 < line.size())
        {
            const char next = line[i + 1];
            if (next == '"') { out += '"'; i += 2; continue; }
            if (next == '\\') { out += '\\'; i += 2; continue; }
        }

        out += c;
        i += 1;
    }

    return false;
}

bool MatchPoKeyword(const std::string &line, const char *keyword, size_t *quote_pos)
{
    const size_t len = std::strlen(keyword);
    if (line.compare(0, len, keyword) != 0) return false;

    size_t p = len;
    while (p < line.size() && line[p] == ' ')
    {
        p++;
    }

    if (p >= line.size() || line[p] != '"') return false;

    *quote_pos = p;
    return true;
}

bool SplitCategoryAndMsgNo(const std::string &msgid, std::string &category, int &msg_no)
{
    const size_t last_hash = msgid.rfind('#');
    if (last_hash == std::string::npos || last_hash + 1 >= msgid.size()) return false;

    for (size_t i = last_hash + 1; i < msgid.size(); i++)
    {
        if (msgid[i] < '0' || msgid[i] > '9') return false;
    }

    category = msgid.substr(0, last_hash);
    msg_no = std::atoi(msgid.c_str() + last_hash + 1);
    return true;
}

bool LoadPoFile(const std::filesystem::path &path,
                std::unordered_map<std::string, std::unordered_map<int, std::string>> &out)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    std::string current_id;
    std::string current_str;
    bool have_id = false;
    bool in_msgid = false;
    bool in_msgstr = false;

    auto commit = [&]()
    {
        if (have_id && !current_id.empty())
        {
            std::string category;
            int msg_no;
            if (SplitCategoryAndMsgNo(current_id, category, msg_no))
            {
                out[category][msg_no] = current_str;
            }
        }
        current_id.clear();
        current_str.clear();
        have_id = false;
        in_msgid = false;
        in_msgstr = false;
    };

    std::string line;
    bool first_line = true;
    while (std::getline(file, line))
    {
        if (first_line && line.size() >= 3 && (unsigned char) line[0] == 0xEF
            && (unsigned char) line[1] == 0xBB && (unsigned char) line[2] == 0xBF)
        {
            line.erase(0, 3); // strip UTF-8 BOM
        }
        first_line = false;

        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        const size_t first_non_space = line.find_first_not_of(" \t");
        if (first_non_space == std::string::npos)
        {
            commit(); // blank line ends the current entry
            continue;
        }

        if (line[first_non_space] == '#') continue;

        size_t quote_pos;
        if (MatchPoKeyword(line.substr(first_non_space), "msgid", &quote_pos))
        {
            commit();
            std::string value;
            ParsePoQuotedSegment(line.substr(first_non_space), quote_pos, value);
            current_id = value;
            have_id = true;
            in_msgid = true;
            continue;
        }

        if (MatchPoKeyword(line.substr(first_non_space), "msgstr", &quote_pos))
        {
            std::string value;
            ParsePoQuotedSegment(line.substr(first_non_space), quote_pos, value);
            current_str = value;
            in_msgid = false;
            in_msgstr = true;
            continue;
        }

        if (line[first_non_space] == '"')
        {
            std::string value;
            ParsePoQuotedSegment(line, first_non_space, value);
            if (in_msgstr) current_str += value;
            else if (in_msgid) current_id += value;
            continue;
        }
    }

    commit();

    return true;
}

enum class CategoryKind { IgMsg, Event };

struct ParsedCategory
{
    CategoryKind kind;
    int type_or_map;
    std::string label; // event only
};

bool ParseCategory(const std::string &category, ParsedCategory &out)
{
    if (category.rfind("ig_msg#", 0) == 0)
    {
        out.kind = CategoryKind::IgMsg;
        out.type_or_map = std::atoi(category.c_str() + 7);
        return true;
    }

    if (category.size() > 1 && category[0] == 'm')
    {
        const size_t marker = category.find("_event#");
        if (marker != std::string::npos)
        {
            out.kind = CategoryKind::Event;
            out.type_or_map = std::atoi(category.c_str() + 1);
            out.label = category.substr(marker + 7);
            return true;
        }
    }

    return false;
}

struct CategoryBuffer
{
    std::vector<u_char> data;
    std::unordered_map<int, size_t> offsets; // includes one entry past the last real message
};

bool ResolveParsedCategory(const ParsedCategory &parsed, int lang,
                           std::vector<u_char> &blob, u_int *start, u_int *end)
{
    if (parsed.kind == CategoryKind::IgMsg)
    {
        if (parsed.type_or_map == 32) return false; // IGMSG_FURN_NAME

        return ReadIgMsgFile(lang, blob)
            && ResolveCategorySubTableRange(blob, (u_char) parsed.type_or_map, start, end);
    }

    if (!ReadEventFile(parsed.type_or_map, lang, blob) || blob.size() < kEventHeaderSize) return false;

    for (const EventTable &table : kEventTables)
    {
        if (parsed.label != table.label) continue;

        *start = ReadUint32LE(&blob[table.header_offset]);
        return *start < blob.size() && ResolveEventTableRange(blob, *start, end);
    }

    return false;
}

bool BuildCategoryBuffer(const ParsedCategory &parsed, int lang,
                         const std::unordered_map<int, std::string> &overrides,
                         CategoryBuffer &out)
{
    std::vector<u_char> blob;
    u_int start = 0;
    u_int end = 0;
    if (!ResolveParsedCategory(parsed, lang, blob, &start, &end)) return false;

    const int entry_count = (int) ((end - start) / 4);

    out.data.clear();
    out.offsets.clear();
    out.offsets.reserve((size_t) entry_count + 1);

    for (int msg_no = 0; msg_no < entry_count; msg_no++)
    {
        out.offsets[msg_no] = out.data.size();

        const auto override_it = overrides.find(msg_no);
        if (override_it != overrides.end())
        {
            std::vector<u_char> encoded;
            if (EncodeGameString(override_it->second, lang, encoded))
            {
                out.data.insert(out.data.end(), encoded.begin(), encoded.end());
                continue;
            }

            info_log("MikuPan_TextMods: could not re-encode override for "
                     "msg %d, using original text", msg_no);
        }

        const int64_t str_offset = ResolveMessageOffset(blob, start, msg_no);
        if (str_offset < 0) { out.data.push_back(kTerminator); continue; }

        size_t i = (size_t) str_offset;
        const size_t begin = i;
        while (i < blob.size() && blob[i] != kTerminator)
        {
            i++;
        }

        const size_t copy_end = std::min(i + 1, blob.size());
        out.data.insert(out.data.end(), blob.begin() + begin, blob.begin() + copy_end);

        if (out.data.empty() || out.data.back() != kTerminator) out.data.push_back(kTerminator);
    }

    out.offsets[entry_count] = out.data.size();

    return true;
}

struct LanguagePack
{
    bool loaded = false;
    std::unordered_map<std::string, std::unordered_map<int, std::string>> overrides;
    std::unordered_map<std::string, CategoryBuffer> categories;
};

LanguagePack g_packs[kLanguageCount];

LanguagePack &GetPack(int lang)
{
    LanguagePack &pack = g_packs[lang];
    if (pack.loaded) return pack;

    pack.loaded = true;

    const std::filesystem::path path = GameTextModsDirectory(lang) / "game_text.po";
    if (LoadPoFile(path, pack.overrides))
    {
        size_t total = 0;
        for (const auto &entry : pack.overrides)
        {
            total += entry.second.size();
        }
        info_log("MikuPan_TextMods: loaded %zu override(s) across %zu "
                 "categor(y/ies) from %s", total, pack.overrides.size(),
                 path.generic_string().c_str());
    }

    return pack;
}

} // namespace

void MikuPan_GameTextExtractAllPOs(void)
{
    for (int lang = 0; lang < kLanguageCount; lang++)
    {
        ExtractLanguage(lang);
    }
}

int64_t MikuPan_GetTextModAddr(const char *category, int msg_no)
{
    if (!mikupan_configuration.text_mods_enabled || category == nullptr) return 0;

#ifdef BUILD_EU_VERSION
    const int lang = ClampLanguage(sys_wrk.language);
#else
    const int lang = 0;
#endif
    LanguagePack &pack = GetPack(lang);

    const auto overrides_it = pack.overrides.find(category);
    if (overrides_it == pack.overrides.end()) return 0; // no overrides at all for this category

    auto cat_it = pack.categories.find(category);
    if (cat_it == pack.categories.end())
    {
        ParsedCategory parsed;
        CategoryBuffer built;
        if (!ParseCategory(category, parsed)
            || !BuildCategoryBuffer(parsed, lang, overrides_it->second, built))
        {
            info_log("MikuPan_TextMods: could not rebuild category %s, "
                     "overrides disabled for it", category);
            built = CategoryBuffer{}; // cache the failure so we don't retry every call
        }

        cat_it = pack.categories.emplace(category, std::move(built)).first;
    }

    const auto off_it = cat_it->second.offsets.find(msg_no);
    if (off_it == cat_it->second.offsets.end()) return 0;

    return (int64_t) (uintptr_t) (cat_it->second.data.data() + off_it->second);
}
