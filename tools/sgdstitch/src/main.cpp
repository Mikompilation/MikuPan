#include "sgd_container.h"
#include "sgd_io.h"
#include "sgd_stitch.h"

#include "mikupan/bake/mikupan_bake.h"
#include "mikupan/bake/mikupan_bake_format.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{

constexpr uint64_t kSectorSize = 0x800;

struct Args
{
    std::string mode;
    std::vector<std::string> positional;
    std::string out_dir;
    std::string names_path;
    std::string only;
    bool dry_run = false;
    bool verbose = false;
    sgd::StitchOptions stitch;
    bool has_stamp = false;
    uint64_t stamp = 0;
    bool write_archive = false;
};

void PrintUsage()
{
    std::printf(
        "sgdstitch - stitches the VU1-sized mesh batches of Fatal Frame SGD "
        "models\n"
        "            into larger batches so MikuPan issues far fewer draw "
        "calls.\n"
        "\n"
        "Usage:\n"
        "  sgdstitch archive <IMG_HD.BIN> <IMG_BD.BIN> --out <dir> "
        "[options]\n"
        "      Writes <dir>/IMG_HD.BIN and <dir>/IMG_BD.BIN with every SGD "
        "stitched.\n"
        "  sgdstitch file <input> <output> [options]\n"
        "      Stitches one extracted .sgd / .mdl / .mpk / .pk2 file.\n"
        "  sgdstitch stats <IMG_HD.BIN> <IMG_BD.BIN> [options]\n"
        "  sgdstitch stats <file> [options]\n"
        "      Reports what would change without writing anything.\n"
        "  sgdstitch bake <IMG_HD.BIN> <IMG_BD.BIN> --names <fnameee.h> --out "
        "<dir>\n"
        "      Decodes every model texture and 2D picture into "
        "<dir>/IMG_TEX.BIN,\n"
        "      the pack the game bakes on first launch. --archive also "
        "writes\n"
        "      <dir>/IMG_HD.BIN and <dir>/IMG_BD.BIN with the baked entries "
        "and\n"
        "      the 2D stamps patched in place.\n"
        "\n"
        "Options:\n"
        "  --names <fnameee.h>   label archive entries using "
        "src/os/eeiop/cdvd/fnameee.h\n"
        "  --only <prefix>       only entries whose name starts with prefix "
        "(needs --names)\n"
        "  --dry-run             analyse only, write nothing\n"
        "  --no-verify           skip the per-SGD equivalence check (not "
        "recommended)\n"
        "  --max-strips <n>      strips per stitched mesh (1-127, default "
        "127)\n"
        "  --max-skin-verts <n>  vertices per stitched skinned batch (default "
        "480, max 500)\n"
        "  --verbose             report every file\n"
        "  --stamp <n>           bake: build stamp (default: the current "
        "time)\n"
        "  --archive             bake: also write a patched IMG_BD.BIN\n");
}

bool ParseInt(const char* text, int* out)
{
    char* end = nullptr;
    const long v = std::strtol(text, &end, 10);

    if (end == text || *end != '\0' || v < 1 || v > 1000000)
    {
        return false;
    }

    *out = (int) v;
    return true;
}

bool ParseArgs(int argc, char** argv, Args* a)
{
    if (argc < 2)
    {
        return false;
    }

    a->mode = argv[1];

    for (int i = 2; i < argc; i++)
    {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;

        if (arg == "--out" && has_value)
        {
            a->out_dir = argv[++i];
        }
        else if (arg == "--names" && has_value)
        {
            a->names_path = argv[++i];
        }
        else if (arg == "--only" && has_value)
        {
            a->only = argv[++i];
        }
        else if (arg == "--max-strips" && has_value)
        {
            if (!ParseInt(argv[++i], &a->stitch.max_strips)
                || a->stitch.max_strips > sgd::kMaxStripsPerMesh)
            {
                std::fprintf(stderr, "--max-strips must be 1-%d\n",
                             sgd::kMaxStripsPerMesh);
                return false;
            }
        }
        else if (arg == "--max-skin-verts" && has_value)
        {
            if (!ParseInt(argv[++i], &a->stitch.max_referenced_verts)
                || a->stitch.max_referenced_verts
                       > sgd::kMaxReferencedVertsHardLimit)
            {
                std::fprintf(stderr, "--max-skin-verts must be 1-%d\n",
                             sgd::kMaxReferencedVertsHardLimit);
                return false;
            }
        }
        else if (arg == "--dry-run")
        {
            a->dry_run = true;
        }
        else if (arg == "--stamp" && has_value)
        {
            char* end = nullptr;
            a->stamp = std::strtoull(argv[++i], &end, 0);
            a->has_stamp = end != nullptr && *end == '\0';

            if (!a->has_stamp)
            {
                std::fprintf(stderr, "--stamp needs a number\n");
                return false;
            }
        }
        else if (arg == "--archive")
        {
            a->write_archive = true;
        }
        else if (arg == "--no-verify")
        {
            a->stitch.verify = false;
        }
        else if (arg == "--verbose")
        {
            a->verbose = true;
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            std::fprintf(stderr, "unknown option %s\n", arg.c_str());
            return false;
        }
        else
        {
            a->positional.push_back(arg);
        }
    }

    return true;
}

bool ReadFile(const fs::path& path, std::vector<uint8_t>* out)
{
    std::ifstream in(path, std::ios::binary);

    if (!in)
    {
        return false;
    }

    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);

    out->resize((size_t) size);
    in.read((char*) out->data(), size);
    return (bool) in;
}

bool WriteFile(const fs::path& path, const std::vector<uint8_t>& data)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write((const char*) data.data(), (std::streamsize) data.size());
    return (bool) out;
}

/// fnameee.h holds the PAL table under #ifdef BUILD_EU_VERSION and the
/// NTSC-U table under #else. Pick the one that matches the archive.
std::vector<std::string> LoadNames(const std::string& path, size_t expected)
{
    std::ifstream in(path);
    std::vector<std::string> tables[2];
    int region = -1;
    std::string line;

    if (!in)
    {
        std::fprintf(stderr, "warning: cannot read %s\n", path.c_str());
        return {};
    }

    while (std::getline(in, line))
    {
        const size_t first = line.find_first_not_of(" \t");
        const std::string t = first == std::string::npos ? ""
                                                         : line.substr(first);

        if (t.rfind("#ifdef BUILD_EU_VERSION", 0) == 0)
        {
            region = 0;
            continue;
        }

        if (t.rfind("#else", 0) == 0 && region == 0)
        {
            region = 1;
            continue;
        }

        if (t.rfind("#endif", 0) == 0 && region == 1)
        {
            region = -1;
            continue;
        }

        if (region < 0)
        {
            continue;
        }

        for (size_t p = line.find('"'); p != std::string::npos;)
        {
            const size_t q = line.find('"', p + 1);

            if (q == std::string::npos)
            {
                break;
            }

            tables[region].push_back(line.substr(p + 1, q - p - 1));
            p = line.find('"', q + 1);
        }
    }

    for (std::vector<std::string>& table : tables)
    {
        if (table.size() == expected)
        {
            return table;
        }
    }

    std::fprintf(stderr,
                 "warning: name tables have %zu / %zu entries but the archive "
                 "has %zu, names ignored\n",
                 tables[0].size(), tables[1].size(), expected);
    return {};
}

std::string Category(const std::string& name)
{
    const std::string prefix = "bin/3ddata/";

    if (name.rfind(prefix, 0) != 0)
    {
        return name.empty() ? "all" : "other";
    }

    const size_t end = name.find('/', prefix.size());
    return name.substr(prefix.size(), end - prefix.size());
}

double Percent(uint64_t before, uint64_t after)
{
    return before == 0 ? 0.0 : 100.0 * (double) (before - after) / before;
}

void PrintLine(const std::string& label, const sgd::StitchStats& s)
{
    std::printf("  %-10s SGDs %5llu (stitched %4llu)  draw calls %6llu -> "
                "%6llu (-%4.1f%%)  shadow-only %5llu -> %5llu\n",
                label.c_str(), (unsigned long long) s.sgds,
                (unsigned long long) s.sgds_modified,
                (unsigned long long) s.draws_before,
                (unsigned long long) s.draws_after,
                Percent(s.draws_before, s.draws_after),
                (unsigned long long) s.shadow_before,
                (unsigned long long) s.shadow_after);
}

void PrintCounts(const char* title, const std::map<std::string, uint64_t>& m)
{
    if (m.empty())
    {
        return;
    }

    std::printf("%s\n", title);
    for (const auto& [why, count] : m)
    {
        std::printf("    %-40s %llu\n", why.c_str(),
                    (unsigned long long) count);
    }
}

void PrintSummary(const sgd::StitchStats& total,
                  const std::map<std::string, sgd::StitchStats>& categories)
{
    std::printf("\nSummary (draw calls = MESH units drawn by the main colour "
                "pass;\n         the shadow passes redraw the same units)\n");

    for (const auto& [name, s] : categories)
    {
        PrintLine(name, s);
    }

    PrintLine("total", total);

    std::printf("\n  pairs examined %llu, folded %llu, vtype 2->3 batches "
                "%llu, bytes freed %llu\n",
                (unsigned long long) total.pairs,
                (unsigned long long) total.pairs_merged_away,
                (unsigned long long) total.vtype3_batches,
                (unsigned long long) total.bytes_freed);

    if (total.sgds_invalid != 0 || total.sgds_rolled_back != 0)
    {
        std::printf("  SGDs left untouched: %llu invalid, %llu rolled back by "
                    "the verifier\n",
                    (unsigned long long) total.sgds_invalid,
                    (unsigned long long) total.sgds_rolled_back);
    }

    PrintCounts("\n  pairs kept as-is (layout not stitchable):",
                total.unsupported);
    PrintCounts("\n  why adjacent pairs were not stitched:", total.breaks);
    PrintCounts("\n  blocks not rewritten:", total.locked);
}

int RunArchive(const Args& a, bool write)
{
    if (a.positional.size() != 2 || (write && a.out_dir.empty()))
    {
        PrintUsage();
        return 1;
    }

    const fs::path hd_path = a.positional[0];
    const fs::path bd_path = a.positional[1];
    std::vector<uint8_t> hd;

    if (!ReadFile(hd_path, &hd) || hd.size() < 8)
    {
        std::fprintf(stderr, "cannot read %s\n", hd_path.string().c_str());
        return 1;
    }

    const size_t count = hd.size() / 8;
    std::vector<std::string> names;

    if (!a.names_path.empty())
    {
        names = LoadNames(a.names_path, count);
    }

    if (!a.only.empty() && names.empty())
    {
        std::fprintf(stderr, "--only needs a matching --names table\n");
        return 1;
    }

    std::ifstream bd(bd_path, std::ios::binary);
    if (!bd)
    {
        std::fprintf(stderr, "cannot read %s\n", bd_path.string().c_str());
        return 1;
    }

    std::fstream out_bd;

    if (write)
    {
        const fs::path out_dir = a.out_dir;
        const fs::path out_hd = out_dir / "IMG_HD.BIN";
        const fs::path out_bd_path = out_dir / "IMG_BD.BIN";
        std::error_code ec;

        fs::create_directories(out_dir, ec);

        if ((fs::exists(out_bd_path)
             && fs::equivalent(out_bd_path, bd_path, ec))
            || (fs::exists(out_hd) && fs::equivalent(out_hd, hd_path, ec)))
        {
            std::fprintf(stderr, "--out must not be the input folder\n");
            return 1;
        }

        std::printf("copying archive to %s ...\n", out_dir.string().c_str());

        /// Every file keeps its size and sector, so IMG_HD.BIN is unchanged
        /// and IMG_BD.BIN is patched in place.
        if (!fs::copy_file(hd_path, out_hd,
                           fs::copy_options::overwrite_existing, ec)
            || !fs::copy_file(bd_path, out_bd_path,
                              fs::copy_options::overwrite_existing, ec))
        {
            std::fprintf(stderr, "copy failed: %s\n", ec.message().c_str());
            return 1;
        }

        out_bd.open(out_bd_path, std::ios::binary | std::ios::in
                                     | std::ios::out);
        if (!out_bd)
        {
            std::fprintf(stderr, "cannot open %s\n",
                         out_bd_path.string().c_str());
            return 1;
        }
    }

    sgd::StitchStats total;
    std::map<std::string, sgd::StitchStats> categories;
    std::set<std::pair<uint32_t, uint32_t>> seen;
    std::vector<uint8_t> data;
    uint64_t files_written = 0;

    for (size_t i = 0; i < count; i++)
    {
        const uint32_t sector = sgd::Rd32(&hd[i * 8]);
        const uint32_t size = sgd::Rd32(&hd[i * 8 + 4]);
        const std::string name = names.empty() ? "" : names[i];

        if (size == 0 || !seen.insert({sector, size}).second)
        {
            continue;
        }

        if (!a.only.empty() && name.rfind(a.only, 0) != 0)
        {
            continue;
        }

        data.resize(size);
        bd.seekg((std::streamoff) (sector * kSectorSize));
        bd.read((char*) data.data(), size);

        if (!bd)
        {
            std::fprintf(stderr, "entry %zu is outside IMG_BD.BIN\n", i);
            bd.clear();
            continue;
        }

        if (!sgd::LooksStitchable(data.data(), data.size()))
        {
            continue;
        }

        sgd::StitchStats stats;
        std::vector<std::string> log;
        const bool modified = sgd::StitchBlob(data.data(), data.size(),
                                              a.stitch, &stats, &log);

        if (stats.sgds == 0)
        {
            continue;
        }

        total.Add(stats);
        categories[Category(name)].Add(stats);

        if (a.verbose || !log.empty())
        {
            std::printf("[%4zu] %-48s draws %4llu -> %4llu\n", i,
                        name.empty() ? "?" : name.c_str(),
                        (unsigned long long) stats.draws_before,
                        (unsigned long long) stats.draws_after);

            for (const std::string& line : log)
            {
                std::printf("         %s\n", line.c_str());
            }
        }

        if (modified && write)
        {
            out_bd.seekp((std::streamoff) (sector * kSectorSize));
            out_bd.write((const char*) data.data(), size);
            files_written++;

            if (!out_bd)
            {
                std::fprintf(stderr, "write failed for entry %zu\n", i);
                return 1;
            }
        }
    }

    PrintSummary(total, categories);

    if (write)
    {
        std::printf("\n  %llu archive entries rewritten in %s\n",
                    (unsigned long long) files_written, a.out_dir.c_str());
    }

    return 0;
}

int RunFile(const Args& a, bool write)
{
    if (a.positional.size() != (write ? 2u : 1u))
    {
        PrintUsage();
        return 1;
    }

    std::vector<uint8_t> data;
    if (!ReadFile(a.positional[0], &data))
    {
        std::fprintf(stderr, "cannot read %s\n", a.positional[0].c_str());
        return 1;
    }

    if (!sgd::LooksStitchable(data.data(), data.size()))
    {
        std::fprintf(stderr, "%s is not an SGD or a pak of SGDs\n",
                     a.positional[0].c_str());
        return 1;
    }

    sgd::StitchStats stats;
    std::vector<std::string> log;
    sgd::StitchBlob(data.data(), data.size(), a.stitch, &stats, &log);

    for (const std::string& line : log)
    {
        std::printf("  %s\n", line.c_str());
    }

    PrintSummary(stats, {});

    if (write && !WriteFile(a.positional[1], data))
    {
        std::fprintf(stderr, "cannot write %s\n", a.positional[1].c_str());
        return 1;
    }

    return 0;
}

int RunBake(const Args& a)
{
    if (a.positional.size() != 2 || a.out_dir.empty() || a.names_path.empty())
    {
        PrintUsage();
        return 1;
    }

    const fs::path hd_path = a.positional[0];
    const fs::path bd_path = a.positional[1];
    const fs::path out_dir = a.out_dir;
    std::error_code ec;

    MikuPanBake::Source source;

    if (!ReadFile(hd_path, &source.hd) || source.hd.size() < 8)
    {
        std::fprintf(stderr, "cannot read %s\n", hd_path.string().c_str());
        return 1;
    }

    source.names = LoadNames(a.names_path, source.hd.size() / 8);
    if (source.names.empty())
    {
        std::fprintf(stderr, "bake needs the name table of this archive\n");
        return 1;
    }

    std::ifstream bd(bd_path, std::ios::binary);
    source.bd_size = fs::file_size(bd_path, ec);
    if (!bd || ec)
    {
        std::fprintf(stderr, "cannot read %s\n", bd_path.string().c_str());
        return 1;
    }

    source.read_bd = [&bd](uint64_t offset, void* buffer, size_t size) {
        bd.clear();
        bd.seekg((std::streamoff) offset);
        bd.read((char*) buffer, (std::streamsize) size);
        return (bool) bd;
    };

    fs::create_directories(out_dir, ec);
    const fs::path pack_path = out_dir / MikuPanBake::kPackFileName;
    const fs::path temp_path = out_dir / "IMG_TEX.BIN.part";

    if (a.write_archive
        && ((fs::exists(out_dir / "IMG_BD.BIN")
             && fs::equivalent(out_dir / "IMG_BD.BIN", bd_path, ec))
            || (fs::exists(out_dir / "IMG_HD.BIN")
                && fs::equivalent(out_dir / "IMG_HD.BIN", hd_path, ec))))
    {
        std::fprintf(stderr, "--out must not be the input folder\n");
        return 1;
    }

    std::fstream pack(temp_path, std::ios::binary | std::ios::in
                                     | std::ios::out | std::ios::trunc);
    if (!pack)
    {
        std::fprintf(stderr, "cannot write %s\n", temp_path.string().c_str());
        return 1;
    }

    MikuPanBake::Sink sink;
    sink.write = [&pack](uint64_t offset, const void* data, size_t size) {
        pack.clear();
        pack.seekp((std::streamoff) offset);
        pack.write((const char*) data, (std::streamsize) size);
        return (bool) pack;
    };
    sink.read = [&pack](uint64_t offset, void* buffer, size_t size) {
        pack.flush();
        pack.clear();
        pack.seekg((std::streamoff) offset);
        pack.read((char*) buffer, (std::streamsize) size);
        return (bool) pack;
    };

    MikuPanBake::Options options;
    options.build_stamp =
        a.has_stamp ? a.stamp
                    : (uint64_t) std::chrono::duration_cast<
                          std::chrono::nanoseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
    options.verify = a.stitch.verify;
    options.log = [&a](const std::string& line) {
        if (a.verbose || line.find(" reads ") == std::string::npos)
        {
            std::printf("  %s\n", line.c_str());
        }
    };
    std::string last_stage;
    size_t last_percent = 101;
    options.progress = [&](const char* stage, size_t done, size_t total) {
        const size_t percent = total == 0 ? 100 : done * 100 / total;

        if (stage != last_stage || percent / 10 != last_percent / 10)
        {
            std::printf("%s %3zu%%\n", stage, percent);
            std::fflush(stdout);
            last_stage = stage;
            last_percent = percent;
        }
    };

    const auto t0 = std::chrono::steady_clock::now();
    MikuPanBake::Stats stats;
    std::string error;
    const bool ok = MikuPanBake::BakeTextures(source, options, &sink, &stats,
                                              &error);
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0)
            .count();
    pack.close();

    if (!ok)
    {
        fs::remove(temp_path, ec);
        std::fprintf(stderr, "bake failed: %s\n", error.c_str());
        return 1;
    }

    fs::rename(temp_path, pack_path, ec);
    if (ec)
    {
        std::fprintf(stderr, "cannot rename %s: %s\n",
                     temp_path.string().c_str(), ec.message().c_str());
        return 1;
    }

    std::printf("\nBaked %s in %.1f s (stamp %llu)\n",
                pack_path.string().c_str(), seconds,
                (unsigned long long) options.build_stamp);
    std::printf("  entries rewritten %zu (%.1f MB), SGDs %zu\n",
                stats.files_rewritten, stats.entry_bytes / 1e6, stats.sgds);
    std::printf("  mesh slots %zu: no texture %zu, black & white %zu, "
                "animated %zu, costume %zu\n",
                stats.slots, stats.slots_no_texture, stats.slots_black_white,
                stats.slots_animated, stats.slots_costume);
    std::printf("  texture ids %zu, images %zu (%.1f MB)\n", stats.textures,
                stats.images, stats.image_bytes / 1e6);
    std::printf("  uncovered texels %zu in %zu textures\n",
                stats.uncovered_texels, stats.uncovered_textures);
    std::printf("  2D files %zu: pictures %zu (%zu CLUT-only, %zu stored as "
                "pixels), derived textures %zu (%.1f MB of pixels)\n",
                stats.files_2d, stats.pictures_2d, stats.pictures_2d_base,
                stats.pictures_2d_baked, stats.derived_2d,
                stats.image_bytes_2d / 1e6);
    std::printf("  verified slots %zu, 2D textures %zu%s\n",
                stats.verified_slots, stats.verified_2d,
                options.verify ? "" : " (verification skipped)");
    std::printf("  pack size %.1f MB\n", stats.pack_size / 1e6);

    if (!a.write_archive)
    {
        return 0;
    }

    /// Same layout as the source: every baked entry keeps its size and
    /// sector, so IMG_HD.BIN is copied as is and IMG_BD.BIN is patched.
    std::printf("writing the patched archive to %s ...\n",
                out_dir.string().c_str());

    if (!fs::copy_file(hd_path, out_dir / "IMG_HD.BIN",
                       fs::copy_options::overwrite_existing, ec)
        || !fs::copy_file(bd_path, out_dir / "IMG_BD.BIN",
                          fs::copy_options::overwrite_existing, ec))
    {
        std::fprintf(stderr, "copy failed: %s\n", ec.message().c_str());
        return 1;
    }

    std::vector<uint8_t> packed;
    if (!ReadFile(pack_path, &packed) || packed.size() < 128)
    {
        std::fprintf(stderr, "cannot read %s\n", pack_path.string().c_str());
        return 1;
    }

    std::fstream out_bd(out_dir / "IMG_BD.BIN",
                        std::ios::binary | std::ios::in | std::ios::out);
    const uint32_t entry_count = sgd::Rd32(&packed[0x14]);
    const uint64_t directory = sgd::Rd32(&packed[0x20])
                               | ((uint64_t) sgd::Rd32(&packed[0x24]) << 32);

    for (uint32_t i = 0; i < entry_count; i++)
    {
        const uint8_t* e = &packed[directory + (uint64_t) i * 16];
        const uint64_t offset =
            sgd::Rd32(e) | ((uint64_t) sgd::Rd32(e + 4) << 32);
        const uint32_t size = sgd::Rd32(e + 8);

        if (size == 0)
        {
            continue;
        }

        const uint32_t sector = sgd::Rd32(&source.hd[i * 8]);
        out_bd.seekp((std::streamoff) (sector * kSectorSize));
        out_bd.write((const char*) &packed[offset], size);
    }

    /// 2D stamps: 16 bytes inside the picture headers of the 2D files.
    const uint64_t patches = sgd::Rd32(&packed[0x68])
                             | ((uint64_t) sgd::Rd32(&packed[0x6C]) << 32);
    const uint64_t ranges = sgd::Rd32(&packed[0x70])
                            | ((uint64_t) sgd::Rd32(&packed[0x74]) << 32);

    for (uint32_t i = 0; i < entry_count; i++)
    {
        const uint8_t* r = &packed[ranges + (uint64_t) i * 8];
        const uint32_t first = sgd::Rd32(r);
        const uint32_t count = sgd::Rd32(r + 4);
        const uint32_t sector = sgd::Rd32(&source.hd[i * 8]);

        for (uint32_t k = 0; k < count; k++)
        {
            const uint8_t* rec =
                &packed[patches + (uint64_t) (first + k) * 40];
            out_bd.seekp((std::streamoff) (sector * kSectorSize
                                           + sgd::Rd32(rec)));
            out_bd.write((const char*) rec + 24, 16);
        }
    }

    if (!out_bd)
    {
        std::fprintf(stderr, "writing the patched IMG_BD.BIN failed\n");
        return 1;
    }

    return 0;
}

}// namespace

int main(int argc, char** argv)
{
    Args args;

    if (!ParseArgs(argc, argv, &args))
    {
        PrintUsage();
        return 1;
    }

    if (args.mode == "archive")
    {
        return RunArchive(args, !args.dry_run);
    }

    if (args.mode == "file")
    {
        return RunFile(args, !args.dry_run);
    }

    if (args.mode == "bake")
    {
        return RunBake(args);
    }

    if (args.mode == "stats")
    {
        return args.positional.size() == 2 ? RunArchive(args, false)
                                           : RunFile(args, false);
    }

    PrintUsage();
    return 1;
}
