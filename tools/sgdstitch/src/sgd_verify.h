#ifndef SGDSTITCH_SGD_VERIFY_H
#define SGDSTITCH_SGD_VERIFY_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sgd
{

/// Everything the renderer would draw from one SGD, in draw order. Each
/// triangle is fingerprinted together with the state it is drawn with
/// (block/bone transform, material, bound texture, mesh type, skinning bones)
/// and every vertex attribute (pool reference or position/normal, ST after
/// MikuPan_FixUV, colour after MikuPan_FixColors). Units the tool does not
/// interpret are fingerprinted byte for byte.
struct DrawStream
{
    std::vector<uint64_t> records;
    uint64_t main_draws = 0;
    uint64_t shadow_draws = 0;
    uint64_t triangles = 0;
};

/// Walks the SGD with the same pointer arithmetic as MikuPan's walkers and
/// mesh renderers (sgsu.cpp, sgsup.cpp, shadow.cpp, mikupan_mesh_render.cpp)
/// and also enforces the renderer limits (signed strip count, objwork room).
bool SimulateSgd(const uint8_t* sgd, size_t size, DrawStream* stream,
                 std::string* error);

/// True when `after` draws exactly the same triangles with the same state as
/// `before` and every byte outside the block chains is unchanged.
bool VerifyEquivalent(const uint8_t* before, const uint8_t* after, size_t size,
                      std::string* difference);

}// namespace sgd

#endif// SGDSTITCH_SGD_VERIFY_H
