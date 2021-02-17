/*
 * Copyright 2020 Uber Technologies, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/** @file  vertex.h
 *  @brief Functions for working with cell vertexes.
 */

#include "h3_vertex.h"

#include "h3_baseCells.h"
#include "h3_faceijk.h"
#include "h3_geoCoord.h"
#include "h3_h3Index.h"

#define DIRECTION_INDEX_OFFSET 2

/** @brief Table of direction-to-face mapping for each pentagon
 *
 * Note that faces are in directional order, starting at J_AXES_DIGIT.
 * This table is generated by the generatePentagonDirectionFaces script.
 */
static const PentagonDirectionFaces pentagonDirectionFaces[NUM_PENTAGONS] = {
    {4, {4, 0, 2, 1, 3}},       {14, {6, 11, 2, 7, 1}},
    {24, {5, 10, 1, 6, 0}},     {38, {7, 12, 3, 8, 2}},
    {49, {9, 14, 0, 5, 4}},     {58, {8, 13, 4, 9, 3}},
    {63, {11, 6, 15, 10, 16}},  {72, {12, 7, 16, 11, 17}},
    {83, {10, 5, 19, 14, 15}},  {97, {13, 8, 17, 12, 18}},
    {107, {14, 9, 18, 13, 19}}, {117, {15, 19, 17, 18, 16}},
};

/**
 * Get the number of CCW rotations of the cell's vertex numbers
 * compared to the directional layout of its neighbors.
 * @return Number of CCW rotations for the cell
 */
int vertexRotations(H3Index cell) {
    // Get the face and other info for the origin
    FaceIJK fijk;
    _h3ToFaceIjk(cell, &fijk);
    int baseCell = H3_EXPORT(h3GetBaseCell)(cell);
    int cellLeadingDigit = _h3LeadingNonZeroDigit(cell);

    // get the base cell face
    FaceIJK baseFijk;
    _baseCellToFaceIjk(baseCell, &baseFijk);

    int ccwRot60 = _baseCellToCCWrot60(baseCell, fijk.face);

    if (_isBaseCellPentagon(baseCell)) {
        // Find the appropriate direction-to-face mapping
        PentagonDirectionFaces dirFaces;
        for (int p = 0; p < NUM_PENTAGONS; p++) {
            if (pentagonDirectionFaces[p].baseCell == baseCell) {
                dirFaces = pentagonDirectionFaces[p];
                break;
            }
        }

        // additional CCW rotation for polar neighbors or IK neighbors
        if (fijk.face != baseFijk.face &&
            (_isBaseCellPolarPentagon(baseCell) ||
             fijk.face ==
                 dirFaces.faces[IK_AXES_DIGIT - DIRECTION_INDEX_OFFSET])) {
            ccwRot60 = (ccwRot60 + 1) % 6;
        }

        // Check whether the cell crosses a deleted pentagon subsequence
        if (cellLeadingDigit == JK_AXES_DIGIT &&
            fijk.face ==
                dirFaces.faces[IK_AXES_DIGIT - DIRECTION_INDEX_OFFSET]) {
            // Crosses from JK to IK: Rotate CW
            ccwRot60 = (ccwRot60 + 5) % 6;
        } else if (cellLeadingDigit == IK_AXES_DIGIT &&
                   fijk.face ==
                       dirFaces.faces[JK_AXES_DIGIT - DIRECTION_INDEX_OFFSET]) {
            // Crosses from IK to JK: Rotate CCW
            ccwRot60 = (ccwRot60 + 1) % 6;
        }
    }
    return ccwRot60;
}

/** @brief Hexagon direction to vertex number relationships (same face).
 *         Note that we don't use direction 0 (center).
 */
static const int directionToVertexNumHex[NUM_DIGITS] = {
    INVALID_DIGIT, 3, 1, 2, 5, 4, 0};

/** @brief Pentagon direction to vertex number relationships (same face).
 *         Note that we don't use directions 0 (center) or 1 (deleted K axis).
 */
static const int directionToVertexNumPent[NUM_DIGITS] = {
    INVALID_DIGIT, INVALID_DIGIT, 1, 2, 4, 3, 0};

/**
 * Get the first vertex number for a given direction. The neighbor in this
 * direction is located between this vertex number and the next number in
 * sequence.
 * @returns The number for the first topological vertex, or INVALID_VERTEX_NUM
 *          if the direction is not valid for this cell
 */
int vertexNumForDirection(const H3Index origin, const Direction direction) {
    int isPentagon = H3_EXPORT(h3IsPentagon)(origin);
    // Check for invalid directions
    if (direction == CENTER_DIGIT || direction >= INVALID_DIGIT ||
        (isPentagon && direction == K_AXES_DIGIT))
        return INVALID_VERTEX_NUM;

    // Determine the vertex rotations for this cell
    int rotations = vertexRotations(origin);

    // Find the appropriate vertex, rotating CCW if necessary
    if (isPentagon) {
        return (directionToVertexNumPent[direction] + NUM_PENT_VERTS -
                rotations) %
               NUM_PENT_VERTS;
    } else {
        return (directionToVertexNumHex[direction] + NUM_HEX_VERTS -
                rotations) %
               NUM_HEX_VERTS;
    }
}