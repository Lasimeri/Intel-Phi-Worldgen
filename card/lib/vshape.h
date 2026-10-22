/* vshape.h: the 3D octave noise of the terrain shape on the card's vector unit. See vshape.md. */
#ifndef VSHAPE_H
#define VSHAPE_H

#include "shape.h"

/*
 * The three 3D octave calls of func_147423_a (field_147429_l, field_147431_j, field_147432_k) for chunk cx, cz,
 * into o1, o2, o3 (825 doubles each, (x * 5 + z) * 33 + y), bit for bit as shape_octaves; 16 columns per vector
 * pass. Needs the x87 precision control at 53 bits (the scalar parts are Java's doubles too). Built with the KNC
 * vector clang (Intel-Phi-LLVM S3).
 */
void vshape_octaves3d(const world *w, int cx, int cz, double o1[825], double o2[825], double o3[825]);

#endif
