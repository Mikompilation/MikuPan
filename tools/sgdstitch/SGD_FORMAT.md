# SGD 3D format (Fatal Frame / Project Zero)

This document describes how characters, items, furniture, doors and rooms are
stored, as observed in the NTSC-U data and in the MikuPan loaders
(`sgsgd.cpp`, `sgsu.cpp`, `sgsup.cpp`, `shadow.cpp`, `sglight.cpp`) and mesh
renderers (`mikupan_mesh_render.cpp`). All values are little-endian. All
offsets are relative to the start of the SGD unless stated otherwise.

## Containers

| File                     | Content                                            |
|--------------------------|----------------------------------------------------|
| `IMG_HD.BIN`             | `{u32 start_sector, u32 size}` per file (1897 NTSC-U, 2169 PAL), sectors are 0x800 bytes of `IMG_BD.BIN`; names are in `fnameee.h` |
| `item/*.sgd`, `door/*.sgd`, `furniture/*.sgd` | one bare SGD                  |
| `man/mdl/*.mdl`          | pak: entry type 5 = MPK (models), type 6 = PK2 (TIM2 textures) |
| `man/mdl/*.mpk`          | pak of SGDs (body, head, hair, accessories... Miku has 10) |
| `room/*.pk2`             | pak of 2 or 4 SGDs: near model, far LOD, and on some rooms two shadow models (`ss_sgd`, `sh_sgd`) |
| `room/*.lit`             | SGD that only holds lights                          |

A pak is `u32 count; u32 pad[3];` followed by `count` entries of
`u32 size; u32 type; u32 pad[2]; u8 data[size];` (`GetFileInPak`,
`MpkMapUnit`, `InitializeRoomUnder`). Loaders place these files back to back
in fixed memory blocks (rooms get 0x2F0000 bytes for the room, its furniture
and its doors), so files must never grow.

## HeaderSection

| Offset | Type  | Field          | Notes                                          |
|-------:|-------|----------------|------------------------------------------------|
| 0x00   | u32   | VersionID      | always 0x1050                                  |
| 0x04   | u8    | MAPFLAG        | 0 in files, set to 1 by `SgMapUnit`            |
| 0x05   | u8    | kind           | bit 0: preset (map) model, drawn by `SgSortUnitP`; bit 1 set at runtime for mirror rooms |
| 0x06   | u16   | materials      | number of `SgMaterial`                         |
| 0x08   | u32   | coordp         | `SgCOORDUNIT[blocks-1]`, 0xE0 bytes each (bones / cells) |
| 0x0C   | u32   | matp           | `SgMaterial[materials]`, 0xB0 bytes each       |
| 0x10   | u32   | phead          | PHEAD, 13 words                                |
| 0x14   | u32   | blocks         | number of block chains                         |
| 0x18   | u32[] | primitives     | chain offset per block, 0 = empty              |

`SgMapUnit` turns every offset into a PS2 address in place, relocates the
vertex references inside `VUVN` units and the material index of `MATERIAL`
units, and is a no-op if MAPFLAG is already set.

## PHEAD and vertex pools

PHEAD holds three sections (unique, common, weighted), each
`{header size, vertex pool, normal pool, list}`:

- **Unique pool**: static vertices and normals (`sceVu0FVECTOR`). `.mim`
  morph data (blinks, hand grips, moving furniture) edits this pool by index,
  `vtx_num = (pUniqNormal - pUniqVertex) / 16`.
- **Weighted pool**: 32-byte records (position in bone 0 and bone 1 space,
  weights). The bone pair is stored at bytes 0x1C/0x1D of the record.
- **Weighted list**: `VERTEXLIST {int count; ONELIST {s16 cn0, cn1; u16 vIdx,
  vOff;}}`, once for positions, once for normals. `CalcVertexBuffer` uses it
  to skin the pool into `vertex_buffer`/`normal_buffer` on the CPU.
  `SgMapUnit` drops the list when the unique list is present and a list does
  not fit the 2000-entry VN buffer.

## Block chains and procedure units

Each block is a chain of procedure units (PU), each 16-byte aligned:

```
u32 pNext;      // byte offset to the next unit, 0 on the terminating sentinel
u32 category;   // prim[1]
u8  desc[8];    // unit specific
u8  payload[];  // usually a PS2 VIF1 packet
```

The chain ends with a 16-byte sentinel whose `pNext` is 0.

| Category | Unit           | Payload / use                                             |
|---------:|----------------|-----------------------------------------------------------|
| 0        | VUVN           | vertex batch, see below                                   |
| 1        | MESH           | triangle strips, **one draw call in MikuPan**             |
| 2        | MATERIAL       | `prim[2]` = material index                                |
| 3        | COORDINATE     | `prim[2]` = coordinate, `prim[3]` = 1 to run skinning     |
| 4        | BOUNDING_BOX   | `prim[2]` = coordinate, 8 corners; culling, light selection, sets the model matrix |
| 5        | GS_IMAGE       | GS upload                                                 |
| 10/13    | TRI2           | textures / black & white CLUTs (block 0)                  |
| 11       | light editor   | `leditor.cpp`                                             |
| 12       | texture anim.  | `GetTextureAnimation`                                     |
| 14       | model groups   | block lists for `SgSortGroupP`                            |
| 0x40     | mirror         | `prim[2]` = block whose first mesh is the mirror surface  |
| 0x41     | stop           | `SgSortUnitPrimP` returns                                 |

Block layout by model kind:

- **Characters, items, kind 0 furniture**: block 0 holds textures. Blocks
  1…n−2 are one bone each (`COORDINATE`, `BOUNDING_BOX`, then
  `MATERIAL`/`VUVN`/`MESH`), drawn rigidly with the bone matrix by
  `SgSortUnitPrim`. The last block is the skinned block (`COORDINATE` with
  flag 1, then material and mesh units), drawn by `SgSortUnitPrimPost`.
- **Rooms, doors, preset furniture (kind 1)**: block 0 holds textures, every
  other block is a spatial cell with its own `BOUNDING_BOX` used for culling
  and for selecting the cell's point/spot lights. A room's near model has
  about 140 cells; most hold a single mesh.

Every `MESH` unit uses the `VUVN` unit directly before it (all 24 319 meshes in
the NTSC-U data follow their own VUVN).

## VUVN units (vertex batches)

Descriptor: `s16 vnum; s16 nnum; u8 qwc; u8 vtype; u8 kind (5 inline,
6 referenced); u8 0`.

The VU1 header qword `{type, A, A2, nmesh}` describes the batch's layout in
VU1 memory: vertex data from qword 1, the texture packet at `A2 = 1 + vertex
qwords` and the strips from `A` (`A2 + 2` with a texture packet, `A2` without).
Each strip takes 1 qword plus 3 qwords per vertex (ST, colour or normal, XYZ),
or 2 per vertex when untextured. VU1 only has 1024 qwords, double buffered,
which is why the exporter cut meshes into small batches: no batch in the
shipped data exceeds 108 vertices, and Miku's skinned batches hold 36 to 87.

| Variant            | Used by                | Payload                                       |
|--------------------|------------------------|-----------------------------------------------|
| referenced         | mesh 0x02/0x0A/0x00/0x08 | `NOP NOP STCYCL(4,4) UNPACK V4-32 num=2·vnum+1`, header `{1, A, A2, nmesh}`, then `vnum × {u32 vertex ref, u32 normal ref}` into the pools; `qwc = 2·vnum+3` is what `SetVUVNData` writes to `objwork` |
| inline             | mesh 0x82/0x80         | `STCYCL UNPACK V4-32 num=1`, header `{0x12 or 0x10, A, A2, nmesh}`, `STCYCL STMASK 0x40404040 UNPACK V3-32 num=2·vnum`, then `vnum × {xyz, normal}` floats |
| preset smooth      | mesh 0x12              | as inline, header type 0x12                   |
| preset flat        | mesh 0x32              | as inline but `nnum` (= strips) normals first, then `vnum` positions |
| position only      | room far LOD           | `STCYCL(2,1)`, positions only; never drawn by MikuPan |

`vtype` selects the pool for referenced batches:

| vtype | Pool / skinning                                                         |
|------:|-------------------------------------------------------------------------|
| 0     | unique pool, copied as is (rigid parts, morph targets)                  |
| 2     | weighted pool; without a weighted list the whole batch is skinned with the bone pair of its **first** vertex (`SkinCalcLoopType2`, GPU `SkinPreparePlain`, shadow `SetVUVNDataShadowModel`) |
| 3     | weighted pool, per-vertex bone pair                                      |
| 4     | CPU-skinned `vertex_buffer` produced from the weighted list              |

## MESH units (strips)

Descriptor: `i32 tag qwc; u8 pad; u8 mesh type; u8 strip count; u8 pad`. The
strip count is read with `GET_NUM_MESH`, a plain `char`, so it must stay at or
below 127.

Mesh type bits (`SGDVUMESHTYPE`): 0x02 textured, 0x0C vertex type (8 =
weighted, drawn with the 0xA shader), 0x10 pre-lit colours, 0x20 flat
normals, 0x40 multi-pass, 0x80 inline vertices.

| Type        | Drawn by                                   | Layout |
|-------------|--------------------------------------------|--------|
| 0x02, 0x0A, 0x82 | main pass + shadow caster              | textured |
| 0x00, 0x08, 0x80 | shadow caster only                     | untextured |
| 0x12, 0x32  | main pass (maps) + shadow receiver         | preset |
| 0x10, 0x30  | not drawn (no texture)                     | preset, untextured |

**Textured** (`MikuPan_RenderMeshType0x2`/`0x82`):

```
STCYCL(4,4)  UNPACK V4-32 num=2 @A2  { GIF tag (A+D), TEX0_1 value, reg 0x06 }   ; TEX0 at +0x28
STMASK 0xfcfcfcfc
nmesh × UNPACK S-32 M { vertex count }                                            ; +0x40, SGDVUMESHPOINTNUM
STMASK 0xd0d0d0d0  STCYCL(3,1)                                                    ; SGDVUMESHSTREGSET
nmesh × UNPACK V2-32 M num=count { S,T floats × count }
NOP padding to 16 bytes
```

`SgdAddTexOffset` patches `TEX0` in place (words 10/11 when word 12 is 6).

**Untextured** (`MikuPan_RenderShadowSilhouette*`): `STMASK 0xfcfcfcfc`, then
`nmesh × UNPACK S-32 M {count}` read from +0x18.

**Preset** (`MikuPan_RenderMeshType0x32`, `SgPreRender`):

```
SGDVUMESHDATA_PRESET { s16 aOffset (8 if the A+D packet exists, else 0),
                       s16 tag section offset, s16 sOffsetToST, s16 sOffsetToPrim, u32 0, u32 0 }
[STCYCL(4,4) UNPACK V4-32 num=2 { GIF tag, TEX0_1 } NOP NOP]                      ; TEX0 at +0x38
STMASK 0xc0c0c0c0  STCYCL(3,1)                                                    ; sOffsetToPrim
nmesh × UNPACK V3-32 M num=count { R,G,B floats × count }                         ; filled by SgPreRender
NOP padding
UNPACK V3-32 {NLOOP|EOP, PRIM, REGS}  (nmesh-1) × UNPACK S-32 {NLOOP|EOP}         ; per-strip GIF tags
NOP padding
STMASK 0xd0d0d0d0  STCYCL(3,1)
nmesh × UNPACK V2-32 M num=count { S,T × count }                                  ; sOffsetToST
NOP padding
```

A preset mesh without the A+D packet keeps the texture bound by the previous
mesh, because the renderer only binds `TEX0` when it lies before the colours.

A V of exactly `0x00000001` in ST data, or an R of `0x00000001` in colour data,
is a PS2 "no kick" marker; `MikuPan_FixUV`/`MikuPan_FixColors` replace it with
the value two vertices back in the same strip.

## Cost in MikuPan

Every `MESH` unit is one draw call: strips are expanded to a triangle list
(`MikuPan_SetTriangleIndex`, alternating winding), the batch is uploaded or
cached (`mikupan_meshcache.cpp`), and it is drawn. The shadow passes draw the
same units again. A character is therefore drawn with one call per
VU1-sized batch: Miku is 138 calls in the main pass and Mafuyu 128. Most of
those batches share material and texture with their neighbour, which is what
`sgdstitch` exploits.
