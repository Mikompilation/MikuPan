# sgdstitch

Offline tool that rewrites the Fatal Frame SGD models (characters, items,
furniture, doors and rooms) so that MikuPan issues far fewer draw calls, while
keeping the SGD format intact: the renderer and the loaders are not changed.

## Why the models cost so many draw calls

SGD models were authored for the PS2 VU1, whose data memory only holds about
one hundred vertices at a time. The exporter therefore cut every mesh into many
small `VUVN` (vertex batch) + `MESH` (triangle strips) procedure-unit pairs,
often several in a row with the same material and texture. MikuPan draws every
`MESH` unit with its own draw call (`MikuPan_RenderMeshType0x2`,
`MikuPan_RenderMeshType0x82`, `MikuPan_RenderMeshType0x32`), and again in the
shadow passes. Miku's body alone is 83 `MESH` units; its skinned block is cut
into 50 batches of at most 87 vertices, many of them just 28 single-triangle
strips.

`sgdstitch` folds consecutive compatible pairs back into one pair. A stitched
pair is a perfectly ordinary `VUVN` + `MESH` pair, just bigger, so every
existing code path (mapping, skinning, pre-lighting, shadows, mesh cache,
texture offsets) consumes it unchanged.

See [SGD_FORMAT.md](SGD_FORMAT.md) for the format analysis.

## Results (NTSC-U data)

Draw calls are the `MESH` units drawn by the main colour pass. The shadow caster
and receiver passes redraw the same units, so they shrink by the same factor.

| Assets     | SGDs | Draw calls before | After  | Reduction |
|------------|-----:|------------------:|-------:|----------:|
| Characters |  474 |             4 793 |  2 528 |    −47.3% |
| Doors      |   80 |               272 |    195 |    −28.3% |
| Furniture  |  155 |               699 |    521 |    −25.5% |
| Items      |   33 |               177 |    150 |    −15.3% |
| Rooms      |  165 |            10 801 |  9 638 |    −10.8% |
| **Total**  |  907 |            16 742 | 13 032 |    −22.2% |

Examples: Miku 138 → 67 (−51%), Mafuyu 128 → 68, room r000 262 → 211.
Rooms gain less because each of their ~140 culling cells (one `BOUNDING_BOX`
each, which also selects the cell's lights) usually holds a single mesh, and
cells are never merged.

## Building

Standalone:

```bash
cmake -S tools/sgdstitch -B build-sgdstitch
cmake --build build-sgdstitch
```

Or as part of MikuPan with `-DMIKUPAN_BUILD_TOOLS=ON` (the binary lands next to
the `MikuPan` executable). The tool is plain C++17 with no dependencies.

## Usage

```bash
sgdstitch archive IMG_HD.BIN IMG_BD.BIN --out stitched --names src/os/eeiop/cdvd/fnameee.h
```

writes `stitched/IMG_HD.BIN` and `stitched/IMG_BD.BIN`. Copy both next to the
`MikuPan` executable in place of the originals (keep the originals as a
backup). Every file keeps its size and its sectors, so `IMG_HD.BIN` is
identical to the input and `IMG_BD.BIN` is only patched inside the rewritten
files. NTSC-U and PAL archives both work; `--names` is optional and only labels
the report.

Other modes:

```bash
sgdstitch stats IMG_HD.BIN IMG_BD.BIN --names src/os/eeiop/cdvd/fnameee.h   # report only
sgdstitch file m000_miku.mdl m000_miku.stitched.mdl                        # one extracted file
```

| Option                 | Meaning                                                   |
|------------------------|-----------------------------------------------------------|
| `--names <fnameee.h>`  | label archive entries, enables per-category totals        |
| `--only <prefix>`      | only process names starting with prefix, e.g. `bin/3ddata/man/` |
| `--dry-run`            | analyse only                                              |
| `--max-strips <n>`     | strips per stitched mesh, 1–127 (default 127)             |
| `--max-skin-verts <n>` | vertices per stitched referenced batch, ≤ 500 (default 480) |
| `--no-verify`          | skip the equivalence check (not recommended)              |
| `--verbose`            | one line per archive entry                                |

## Texture baking (`bake`)

MikuPan no longer feeds textures through a PS2 GS emulation. On first launch
it decodes every model texture once into `IMG_TEX.BIN` (about 200 MB, next to
`mikupan.ini`): each mesh names its texture by id, and so does each TIM2
picture of the 2D files. The baker is a library shared with the game
(`src/mikupan/bake/`); this mode runs the same bake offline:

```bash
sgdstitch bake IMG_HD.BIN IMG_BD.BIN --names src/os/eeiop/cdvd/fnameee.h --out baked
```

writes `baked/IMG_TEX.BIN`, which the game accepts in its user folder. The
pack holds the decoded images and every archive entry the bake rewrote (same
size, nothing moved); the game reads those entries from the pack, so
`IMG_BD.BIN` itself is never modified. `--archive` also writes
`baked/IMG_HD.BIN` and a patched `baked/IMG_BD.BIN` for offline tools.
`--names` is required (it selects the model files, the player costumes and
the 2D files).
`--stamp <n>` gives reproducible output; `--no-verify` skips the read-back
check.

A bake replays each model's GS uploads (TRI2 units, texture animation frames,
TIM2 files, player costumes and their black & white CLUTs) and decodes each
mesh's texture with the game's own GS code (`src/mikupan/gs/
mikupan_gs_memory.cpp`). It then checks every mesh, for every animation
frame, costume and black & white variant, against the pack it wrote. Black &
white mode becomes a shader flag; the bake fails if a black & white CLUT is
not gray(colour) within 1.

The 2D files (menus, HUD, fonts, effects; TIM2 files and pk2s of them) get a
2D texture id per picture. The pack holds 16-byte stamps that the game
writes into each picture header, in bytes it never reads, as the file loads
(`--archive` writes them into the patched `IMG_BD.BIN`). The game then
decodes a picture from its own data the first time it is drawn, without GS
memory (`src/mikupan/bake/mikupan_tim2.cpp`). Textures that show more than
one picture's data are stored as pixels: the few pictures whose TEX0 covers
other pictures, and the textures the game draws from inside other pictures
(message box, button icons, a number font, the title letters). The bake
checks every one against the GS decode of its file.

Stitch after baking, if at all: `sgdstitch archive` on a baked archive treats
the baked texture slots like any other A+D packet.

## What gets stitched

Two neighbouring pairs in the same block chain are folded when:

- they have the same mesh type, VUVN kind, vertex pool and unit header bytes;
- they draw with the same texture: identical GIF A+D `TEX0` packet, or a
  preset unit without a packet (it inherits the texture bound by the unit
  before it) following one with a packet;
- they draw with the same material. A `MATERIAL` unit that re-selects the
  material already in effect is dropped. `SetMaterialData` only depends on the
  material and the current lights, which only change at `COORDINATE` and
  `BOUNDING_BOX` units, and those always end a group;
- the result stays within the renderer limits:
  - at most 127 strips per mesh, because `GET_NUM_MESH` reads a signed `char`;
  - at most 480 vertices per referenced batch, so the expanded vertices still
    fit the 0x400-qword `objwork` headroom that `FlushModel` keeps.

Skinned `vtype 2` batches move every vertex with the bone pair of the batch's
first vertex. Batches with different bone pairs are only stitched when every
vertex record already carries its batch's bone pair. The stitched batch is then
emitted as `vtype 3` (per-vertex bone pair), which gives identical results on
the CPU, GPU and shadow skinning paths. This affects 791 batches.

Never touched:

- the mirror block named by a type `0x40` unit;
- room far-LOD SGDs (position-only vertices, mapped but never drawn);
- untextured preset meshes (`0x10`, `0x30`, not drawn);
- any unit whose layout the canonical builder cannot reproduce byte for byte.

Each original strip stays its own strip, so the renderer generates exactly
the same triangles (same winding, same `MikuPan_FixUV` / `MikuPan_FixColors`
marker handling). Draw order is preserved; nothing is reordered.

## Safety checks

1. **Round-trip check.** A pair is only stitchable if the tool's canonical
   builders reproduce the original `VUVN` and `MESH` bytes exactly. Every
   field it writes, including the PS2 VU1 addresses, DMA sizes, strip tags and
   preset offsets, is derived from a model that matched the original data.
2. **In-place rewrite.** A block chain is rebuilt into its own byte range,
   which only ever shrinks, and the freed tail is zeroed. Header, coordinates,
   materials, PHEAD and vertex pools are byte-identical, so morph (`.mim`) and
   motion data that index the pools keep working. Offsets, sizes and
   containers never change.
3. **Equivalence check.** Every stitched SGD is re-simulated with the same
   pointer arithmetic as MikuPan's walkers and mesh renderers. The simulation
   covers every triangle with its block, material, bound texture, mesh type,
   skinning bones, vertex reference or position/normal, ST and colour. The
   result must equal the original, or the SGD is restored and reported.

On the NTSC-U archive all 467 rewritten SGDs pass; an independent check also
confirmed 448 681 identical triangles with no byte changed outside the
rewritten chains.

## Limits and notes

- Stitched batches exceed what PS2 VU1 memory can hold, so the output is for
  MikuPan only, not for the original game or an emulator. The 8-bit VIF `NUM`
  and VUVN `qwc` fields wrap or clamp as documented in `sgd_units.cpp`;
  MikuPan does not use them, it reads the unit header counts.
- To check the result in game, open the debug UI's **Performance
  Diagnostics** window (or set `MIKUPAN_PERF_DIAG=1`) and compare `Draws`,
  `0x2 calls`/`0xA calls` and `Max packet` with the original archive.
