#include "mikupan_i18n.h"

#include "mikupan/io/mikupan_file.h"
#include "mikupan/mikupan_utils.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

/* Index matches MikuPan_GetUiLanguage(): 0=EN, 1=FR, 2=DE, 3=ES, 4=IT.
 * English has no .po file - MikuPan_Translate falls back to msgid for it. */
constexpr const char *kLanguagePoFile[5] = {
    nullptr,
    "resources/lang/fr.po",
    "resources/lang/de.po",
    "resources/lang/es.po",
    "resources/lang/it.po",
};

using TranslationTable = std::unordered_map<std::string, std::string>;

std::array<TranslationTable, 5> g_translations;
std::array<bool, 5> g_translations_loaded = {};

std::string UnescapePoString(const std::string &raw)
{
    std::string result;
    result.reserve(raw.size());

    for (size_t i = 0; i < raw.size(); i++)
    {
        if (raw[i] != '\\' || i + 1 >= raw.size())
        {
            result.push_back(raw[i]);
            continue;
        }

        i++;
        switch (raw[i])
        {
        case 'n':
            result.push_back('\n');
            break;
        case 't':
            result.push_back('\t');
            break;
        case '"':
            result.push_back('"');
            break;
        case '\\':
            result.push_back('\\');
            break;
        default:
            result.push_back(raw[i]);
            break;
        }
    }

    return result;
}

/* Extracts the content of one "..." literal starting at line[pos] (which
 * must be '"'). Returns the raw (still-escaped) content, and advances pos
 * past the closing quote. Returns false if the line doesn't start with a
 * quoted string at pos. */
bool ReadQuotedLiteral(const std::string &line, size_t pos, std::string &out)
{
    if (pos >= line.size() || line[pos] != '"')
    {
        return false;
    }

    out.clear();
    pos++;
    while (pos < line.size() && line[pos] != '"')
    {
        if (line[pos] == '\\' && pos + 1 < line.size())
        {
            out.push_back(line[pos]);
            out.push_back(line[pos + 1]);
            pos += 2;
            continue;
        }

        out.push_back(line[pos]);
        pos++;
    }

    return true;
}

/* Reads every "..." literal on directives that continue across lines, e.g.
 *   msgid "Hello, "
 *   "world"
 * and concatenates them (unescaped) into one string. `pos` is the index in
 * `lines` of the directive line itself (already past the "msgid"/"msgstr"
 * keyword); it's updated to the index of the last consumed line. */
std::string ReadPoValue(const std::vector<std::string> &lines,
                        size_t &pos,
                        size_t first_quote_pos)
{
    std::string value;
    std::string literal;

    if (ReadQuotedLiteral(lines[pos], first_quote_pos, literal))
    {
        value += UnescapePoString(literal);
    }

    while (pos + 1 < lines.size())
    {
        const std::string &next = lines[pos + 1];
        size_t start = next.find_first_not_of(" \t");
        if (start == std::string::npos || next[start] != '"')
        {
            break;
        }

        pos++;
        if (ReadQuotedLiteral(lines[pos], start, literal))
        {
            value += UnescapePoString(literal);
        }
    }

    return value;
}

void ParsePoContent(const std::string &content, TranslationTable &out)
{
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= content.size())
    {
        size_t end = content.find('\n', start);
        if (end == std::string::npos)
        {
            lines.push_back(content.substr(start));
            break;
        }

        lines.push_back(content.substr(start, end - start));
        start = end + 1;
    }

    std::string pending_msgid;
    bool has_pending_msgid = false;

    for (size_t i = 0; i < lines.size(); i++)
    {
        std::string &line = lines[i];
        size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#')
        {
            continue;
        }

        if (line.compare(first, 6, "msgid ") == 0)
        {
            size_t quote = line.find('"', first + 6);
            if (quote == std::string::npos)
            {
                continue;
            }

            pending_msgid = ReadPoValue(lines, i, quote);
            has_pending_msgid = true;
        }
        else if (has_pending_msgid && line.compare(first, 7, "msgstr ") == 0)
        {
            size_t quote = line.find('"', first + 7);
            if (quote == std::string::npos)
            {
                continue;
            }

            std::string msgstr = ReadPoValue(lines, i, quote);
            if (!pending_msgid.empty() && !msgstr.empty())
            {
                out.emplace(std::move(pending_msgid), std::move(msgstr));
            }

            has_pending_msgid = false;
        }
    }
}

void LoadLanguageTable(int lang)
{
    g_translations_loaded[lang] = true;

    const char *relative_path = kLanguagePoFile[lang];
    if (relative_path == nullptr)
    {
        return;
    }

    char po_path[1024];
    if (!MikuPan_ResolveBasePath(relative_path, po_path, sizeof(po_path)))
    {
        return;
    }

    u_int file_size = MikuPan_GetFileSize(po_path);
    if (file_size == 0)
    {
        return;
    }

    std::string content(file_size, '\0');
    MikuPan_ReadFullFile(po_path, content.data());

    ParsePoContent(content, g_translations[lang]);
}

} // namespace

const char *MikuPan_Translate(const char *msgid)
{
    if (msgid == nullptr)
    {
        return msgid;
    }

    const int lang = MikuPan_GetUiLanguage();
    if (lang < 0 || lang >= 5 || kLanguagePoFile[lang] == nullptr)
    {
        return msgid;
    }

    if (!g_translations_loaded[lang])
    {
        LoadLanguageTable(lang);
    }

    const auto found = g_translations[lang].find(msgid);
    return found != g_translations[lang].end() ? found->second.c_str() : msgid;
}

void MikuPan_TranslationsReset(void)
{
    for (int i = 0; i < 5; i++)
    {
        g_translations[i].clear();
        g_translations_loaded[i] = false;
    }
}
