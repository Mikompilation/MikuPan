#ifndef SGDSTITCH_SGD_CONTAINER_H
#define SGDSTITCH_SGD_CONTAINER_H

#include "sgd_stitch.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sgd
{

/// Stitches every SGD found in `data`, in place. `data` is either an SGD
/// (items, doors, furniture), or a pak - [count][pad x3] followed by `count`
/// entries of [size][type][pad x2][payload] - whose entries are SGDs or
/// nested paks (character .mdl -> .mpk -> SGDs, room .pk2 -> SGDs). Sizes and
/// offsets never change, so the containers need no fix-ups. Returns true if
/// anything was modified; problems are appended to `log`.
bool StitchBlob(uint8_t* data, size_t size, const StitchOptions& options,
                StitchStats* stats, std::vector<std::string>* log,
                int depth = 0);

/// True if `data` holds something StitchBlob() would look into.
bool LooksStitchable(const uint8_t* data, size_t size);

}// namespace sgd

#endif// SGDSTITCH_SGD_CONTAINER_H
