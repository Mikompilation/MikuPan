#ifndef MIKUPAN_GS_MEMORY_H
#define MIKUPAN_GS_MEMORY_H

#include <cstddef>
#include <cstdint>
#include <vector>

/// Dependency-free emulation of the PS2 GS local memory: the swizzled VRAM
/// layout, image uploads and the texture decode MikuPan performed for a TEX0
/// before its textures were baked. Only the texture baker uses it now (in
/// the game for the first-launch bake, and in tools/sgdstitch): the bake
/// decodes model textures with it and checks every baked texture against
/// it. Keep it free of SDL, logging and game headers.

enum PixelStorageFormat
{
    PSMCT32 = 0b000000,
    PSMCT24 = 0b000001,
    PSMCT16 = 0b000010,
    PSMCT16S = 0b001010,
    PSMT8 = 0b010011,
    PSMT4 = 0b010100,
    PSMT8H = 0b011011,
    PSMT4HL = 0b100100,
    PSMT4HH = 0b101100,
    PSMZ32 = 0b110000,
    PSMZ24 = 0b110001,
    PSMZ16 = 0b110010,
    PSMZ16S = 0b111010,
};

namespace GS
{
/// Size of the GS local memory. Region checks (texture hashes, cache
/// invalidation, "can this TEX0 be drawn") use this size.
constexpr int kVramSize = 4 * 1024 * 1024;

/// The PSMCT32/PSMCT16 address functions wrap at 4 MB, the PSMT8/PSMT4 ones
/// do not, so a T8/T4 access near the top of VRAM can land past 4 MB. The
/// memory is allocated twice as large so those accesses stay inside it.
constexpr int kVramAllocSize = 2 * kVramSize;

constexpr int kBlockTablePSMCT32[] = {
    0, 1, 4,  5,  16, 17, 20, 21, 2,  3,  6,  7,  18, 19, 22, 23,
    8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31,
};

constexpr int kColumnTablePSMCT32[] = {
    0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15,
};

constexpr int kBlockTablePSMT8[] = {
    0, 1, 4,  5,  16, 17, 20, 21, 2,  3,  6,  7,  18, 19, 22, 23,
    8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31,
};

constexpr int kColumnTablePSMT8[] = {
    0,   4,   16,  20,  32,  36,  48,  52,// Column 0
    2,   6,   18,  22,  34,  38,  50,  54,  8,   12,  24,  28,  40,  44,
    56,  60,  10,  14,  26,  30,  42,  46,  58,  62,  33,  37,  49,  53,
    1,   5,   17,  21,  35,  39,  51,  55,  3,   7,   19,  23,  41,  45,
    57,  61,  9,   13,  25,  29,  43,  47,  59,  63,  11,  15,  27,  31,
    96,  100, 112, 116, 64,  68,  80,  84,// Column 1
    98,  102, 114, 118, 66,  70,  82,  86,  104, 108, 120, 124, 72,  76,
    88,  92,  106, 110, 122, 126, 74,  78,  90,  94,  65,  69,  81,  85,
    97,  101, 113, 117, 67,  71,  83,  87,  99,  103, 115, 119, 73,  77,
    89,  93,  105, 109, 121, 125, 75,  79,  91,  95,  107, 111, 123, 127,
    128, 132, 144, 148, 160, 164, 176, 180,// Column 2
    130, 134, 146, 150, 162, 166, 178, 182, 136, 140, 152, 156, 168, 172,
    184, 188, 138, 142, 154, 158, 170, 174, 186, 190, 161, 165, 177, 181,
    129, 133, 145, 149, 163, 167, 179, 183, 131, 135, 147, 151, 169, 173,
    185, 189, 137, 141, 153, 157, 171, 175, 187, 191, 139, 143, 155, 159,
    224, 228, 240, 244, 192, 196, 208, 212,// Column 3
    226, 230, 242, 246, 194, 198, 210, 214, 232, 236, 248, 252, 200, 204,
    216, 220, 234, 238, 250, 254, 202, 206, 218, 222, 193, 197, 209, 213,
    225, 229, 241, 245, 195, 199, 211, 215, 227, 231, 243, 247, 201, 205,
    217, 221, 233, 237, 249, 253, 203, 207, 219, 223, 235, 239, 251, 255,
};

constexpr int kBlockTablePSMT4[] = {
    0, 2, 8, 10, 1, 3, 9, 11, 4, 6, 12, 14, 5, 7, 13, 15,
};

constexpr int kColumnTablePSMT4[] = {
    0,   8,   32,  40,  64,  72,  96,  104,// Column 0
    2,   10,  34,  42,  66,  74,  98,  106, 4,   12,  36,  44,  68,  76,  100,
    108, 6,   14,  38,  46,  70,  78,  102, 110, 16,  24,  48,  56,  80,  88,
    112, 120, 18,  26,  50,  58,  82,  90,  114, 122, 20,  28,  52,  60,  84,
    92,  116, 124, 22,  30,  54,  62,  86,  94,  118, 126, 65,  73,  97,  105,
    1,   9,   33,  41,  67,  75,  99,  107, 3,   11,  35,  43,  69,  77,  101,
    109, 5,   13,  37,  45,  71,  79,  103, 111, 7,   15,  39,  47,  81,  89,
    113, 121, 17,  25,  49,  57,  83,  91,  115, 123, 19,  27,  51,  59,  85,
    93,  117, 125, 21,  29,  53,  61,  87,  95,  119, 127, 23,  31,  55,  63,
    192, 200, 224, 232, 128, 136, 160, 168,// Column 1
    194, 202, 226, 234, 130, 138, 162, 170, 196, 204, 228, 236, 132, 140, 164,
    172, 198, 206, 230, 238, 134, 142, 166, 174, 208, 216, 240, 248, 144, 152,
    176, 184, 210, 218, 242, 250, 146, 154, 178, 186, 212, 220, 244, 252, 148,
    156, 180, 188, 214, 222, 246, 254, 150, 158, 182, 190, 129, 137, 161, 169,
    193, 201, 225, 233, 131, 139, 163, 171, 195, 203, 227, 235, 133, 141, 165,
    173, 197, 205, 229, 237, 135, 143, 167, 175, 199, 207, 231, 239, 145, 153,
    177, 185, 209, 217, 241, 249, 147, 155, 179, 187, 211, 219, 243, 251, 149,
    157, 181, 189, 213, 221, 245, 253, 151, 159, 183, 191, 215, 223, 247, 255,
    256, 264, 288, 296, 320, 328, 352, 360,// Column 2
    258, 266, 290, 298, 322, 330, 354, 362, 260, 268, 292, 300, 324, 332, 356,
    364, 262, 270, 294, 302, 326, 334, 358, 366, 272, 280, 304, 312, 336, 344,
    368, 376, 274, 282, 306, 314, 338, 346, 370, 378, 276, 284, 308, 316, 340,
    348, 372, 380, 278, 286, 310, 318, 342, 350, 374, 382, 321, 329, 353, 361,
    257, 265, 289, 297, 323, 331, 355, 363, 259, 267, 291, 299, 325, 333, 357,
    365, 261, 269, 293, 301, 327, 335, 359, 367, 263, 271, 295, 303, 337, 345,
    369, 377, 273, 281, 305, 313, 339, 347, 371, 379, 275, 283, 307, 315, 341,
    349, 373, 381, 277, 285, 309, 317, 343, 351, 375, 383, 279, 287, 311, 319,
    448, 456, 480, 488, 384, 392, 416, 424,// Column 3
    450, 458, 482, 490, 386, 394, 418, 426, 452, 460, 484, 492, 388, 396, 420,
    428, 454, 462, 486, 494, 390, 398, 422, 430, 464, 472, 496, 504, 400, 408,
    432, 440, 466, 474, 498, 506, 402, 410, 434, 442, 468, 476, 500, 508, 404,
    412, 436, 444, 470, 478, 502, 510, 406, 414, 438, 446, 385, 393, 417, 425,
    449, 457, 481, 489, 387, 395, 419, 427, 451, 459, 483, 491, 389, 397, 421,
    429, 453, 461, 485, 493, 391, 399, 423, 431, 455, 463, 487, 495, 401, 409,
    433, 441, 465, 473, 497, 505, 403, 411, 435, 443, 467, 475, 499, 507, 405,
    413, 437, 445, 469, 477, 501, 509, 407, 415, 439, 447, 471, 479, 503, 511,
};

/// Byte address of pixel (x, y) of a buffer at `block` (256-byte units) with
/// a buffer width of `width` * 64 pixels. PSMT4 returns a nibble address.
int GetPixelAddressPSMCT32(int block, int width, int x, int y);
int GetPixelAddressPSMCT16(int block, int width, int x, int y);
int GetPixelAddressPSMT8(int block, int width, int x, int y);
int GetPixelAddressPSMT4(int block, int width, int x, int y);

/// The TEX0_1 fields the texture decode reads.
struct Tex0
{
    int tbp0 = 0;///< texture base, 256-byte blocks
    int tbw = 0; ///< buffer width / 64
    int psm = 0;
    int tw = 0;  ///< log2 width
    int th = 0;  ///< log2 height
    int cbp = 0; ///< CLUT base, 256-byte blocks
    int csa = 0; ///< CLUT entry offset (PSMT4)
};

/// Splits a raw 64-bit GS TEX0 register value into its fields.
Tex0 DecodeTex0(uint64_t value);

/// Linear byte range [addr, addr + size) that starts at the texture's first
/// texel and covers width * height texels. The game hashes this range and
/// uses it for cache invalidation; a TEX0 whose range is not inside the 4 MB
/// of VRAM (or whose PSM is not a colour/depth format) is never bound.
bool GetRawTextureRegion(const Tex0& tex0, int* out_addr, int* out_size);

/// True when the game can decode this TEX0 into an RGBA8 image: both sides
/// at most 4096 and a PSM of PSMCT32, PSMCT16, PSMCT16S, PSMT8 or PSMT4.
bool IsDecodableTexture(const Tex0& tex0);

/// PS2 alpha (0x80 = opaque) to 8-bit alpha: a <= 127 ? a * 2 : 0xFF.
unsigned char AdjustPS2Alpha(unsigned char alpha);

/// Applies AdjustPS2Alpha to every pixel of an RGBA8 image, in place.
void ConvertImageAlpha(unsigned char* rgba, int width, int height);

class GSHelper
{

   public:
    GSHelper();
    ~GSHelper() = default;

    /// Stores an image transfer the way the game's GS uploads did: PSMZ32 as
    /// PSMCT32, every 16-bit format as PSMCT16, PSMT8 and PSMT4. Returns false
    /// (and writes nothing) for any other DPSM.
    bool Upload(int dpsm, int dbp, int dbw, int dsax, int dsay, int rrw,
                int rrh, const uint8_t* inbuf);

    void UploadPSMCT32(int dbp, int dbw, int dsax, int dsay, int rrw, int rrh,
                       const uint8_t *inbuf);
    void UploadPSMCT16(int dbp, int dbw, int dsax, int dsay, int rrw, int rrh,
                       const uint8_t *inbuf);
    void UploadPSMT8(int dbp, int dbw, int dsax, int dsay, int rrw, int rrh,
                     const uint8_t *inbuf);
    void UploadPSMT4(int dbp, int dbw, int dsax, int dsay, int rrw, int rrh,
                     const uint8_t *inbuf);
    void DownloadPSMCT32(unsigned char* outbuf, int dbp, int dbw, int dsax,
                         int dsay, int rrw, int rrh) const;
    void DownloadPSMCT16(unsigned char* outbuf, int dbp, int dbw, int dsax,
                         int dsay, int rrw, int rrh) const;
    void StorePSMCT16(unsigned char* outbuf, int dbp, int dbw, int dsax,
                      int dsay, int rrw, int rrh) const;
    void DownloadImagePSMT8(unsigned char* outbuf, int dbp, int dbw, int dsax,
                            int dsay, int rrw, int rrh, int cbp, int cbw,
                            int alpha_reg) const;
    void DownloadImagePSMT4(unsigned char* outbuf, int dbp, int dbw, int dsax,
                            int dsay, int rrw, int rrh, int cbp, int cbw,
                            int csa, int alpha_reg) const;

    /// Decodes the texture `tex0` names into `outbuf` (width * height RGBA8
    /// pixels, rows top-down) exactly as the game's texture binding did:
    /// de-swizzle, CLUT lookup (16x16 PSMCT32 CLUT with the CSM1 index
    /// shuffle for PSMT8, 8x2 entries at CSA for PSMT4), then
    /// ConvertImageAlpha(). Returns false when !IsDecodableTexture(tex0).
    bool DecodeTexture(const Tex0& tex0, unsigned char* outbuf) const;

    void Clear();

    std::vector<char> mem_;
};
}// namespace GS

#endif//MIKUPAN_GS_MEMORY_H
