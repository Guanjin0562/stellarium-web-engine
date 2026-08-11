/* Stellarium Web Engine - Copyright (c) 2022 - Stellarium Labs SRL
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * File: Skyculture
 *
 * Some basic functions to parse skyculture data files.
 * Still experimental, probably going to change.
 *
 * The actual skyculture struct is in src/modules/skycultures.c
 */

#ifndef SKYCULTURE_H
#define SKYCULTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "json.h"
#include "uthash.h"

/*
 * Weight used for displaying constellation lines.
 */
enum {
    LINE_WEIGHT_NORMAL    = 0,
    LINE_WEIGHT_THIN      = 1,
    LINE_WEIGHT_BOLD      = 2
};

/*
 * Type of a sky culture feature (constellation or asterism).
 * Asterisms use the exact same rendering pipeline as constellations, only
 * the default line style and some behaviors differ.
 */
enum {
    SC_TYPE_CONSTELLATION    = 0, // A regular constellation.
    SC_TYPE_ASTERISM         = 1, // A named asterism (e.g. indian nakshatras).
    SC_TYPE_RAY_HELPER       = 2, // is_ray_helper (yellow helper lines).
    SC_TYPE_DARK_ASTERISM    = 3, // Coordinates based outline (e.g. chinese
                                  //   walls / lunar mansions boundaries).
};

typedef struct constellation_line
{
    // Endpoints of the line.  Each endpoint is either a Hipparcos star
    // number (hip) or a reference into the parent constellation_infos_t
    // dso_names array (dso, -1 when not used).
    int     hip[2];
    int8_t  dso[2];
    uint8_t line_weight;
} constellation_line_t;

/*
 * Type: constellation_edge_t
 * A constellation boundary segment, expressed in the skyculture
 * edges_epoch frame.  (ra1, de1) -> (ra2, de2) are the two endpoints
 * (radians).  type and dir follow the desktop Stellarium
 * ConstellationMgr::loadBoundaries() convention: 'P' means the segment
 * follows a parallel (RA varies, DE constant), 'M' a meridian (DE varies,
 * RA constant) and '_' no interpolation; dir is '+' or '-' (or '_') and
 * gives the traversal direction, used to resolve the RA/DE wrap-around.
 */
typedef struct constellation_edge
{
    double ra1, de1, ra2, de2;
    uint8_t type, dir;
} constellation_edge_t;

/*
 * Type: constellation_infos_t
 * Information about a given constellation (or asterism).
 */
typedef struct constellation_infos
{
    char id[128];
    constellation_line_t lines[128];
    int  nb_lines;
    // Ra/dec boundaries (in edges_epoch).  Dynamically allocated array:
    // some skycultures (e.g. chinese_mdn) define constellations with more
    // than 1000 boundary segments.
    constellation_edge_t *edges;
    int nb_edges;
    int edges_cap;
    // Rotation matrix from the edges epoch (edges_epoch) to J2000, used
    // when rendering the boundaries.  The default for skycultures without
    // an edges_epoch field is B1875, matching the historical engine
    // behavior.  For edges already in J2000 this is the identity.
    double edges_rot[3][3];
    // Feature type, one of the SC_TYPE_* values.
    int type;
    // Custom line rendering style.  line_color is set to a negative value in
    // its first component when the default color must be used.  A zero
    // line_width means the default width.
    float line_color[3];
    float line_width;
    // Dark asterism polyline: an array of [ra, dec] pairs (radians, J2000),
    // flattened as start/end point couples like the edges array.
    double coords[128][2];
    int nb_coords;
    // DSO references used as line endpoints.  Each entry is a designation
    // without the "DSO:" prefix (e.g. "M45").  constellation_line_t::dso
    // stores the index of the reference in this array.
    char *dso_names[8];
    int nb_dso_names;
    // Optional custom label positions (radians, J2000).  Several positions
    // are allowed: the desktop version draws the name at every position
    // (e.g. Serpens has two parts, or chinese_mdn's "Pillars" has five).
    // The automatically computed position is discarded when any manual
    // position is defined.
    double label_pos[8][2];
    int nb_label_pos;
    // Optional label offset (radians) applied to the auto computed position.
    double label_offset[2];
    bool has_label_offset;
    // Radius in degrees used for single star asterisms.
    float single_star_radius;
    char *description;
    char iau[8]; // IAU abbreviation. Zero padded.
    char img[128];  // Name of the image file.
    struct {
        double  uv[2]; // Texture UV position.
        int     hip;   // Star HIP.
    } anchors[3];
    const char *base_path;
} constellation_infos_t;

/*
 * Type: skyculture_name
 * Structure to hold hash table of object name and oid.
 *
 * Used as result of skyculture names file parsing.
 */
typedef struct skyculture_name
{
    UT_hash_handle  hh;
    // The ID to use when calling sky_cultures_get_name:
    //   - for bright stars use "HIP XXXX"
    //   - for constellations use "CON culture_name XXX"
    //   - for planets use "NAME Planet"
    //   - for DSO use the first identifier of the names list
    char           *main_id;
    char           *name_english;
    char           *name_native;
    char           *name_pronounce;
    char           *name_ipa;
    char           *name_description;
    // Pointer to a secondary name, or NULL
    struct skyculture_name* alternative;
} skyculture_name_t;

/*
 * Type: cultural_name
 * Structure to hold information about a cultural name.
 */
typedef struct cultural_name
{
    // The english cultural name e.g. 'Great Bear'
    char           *name_english;
    // The translated version of this cultural name. It is translated according
    // to the current user language as defined in sys_get_lang()
    char           *name_translated;
    // The native name using native spelling, e.g. for western constellation
    // it's the latin name like 'Ursa Major'. For Chinese constellations it's
    // the chinese name like '座旗'
    // other
    char           *name_native;
    // Contains the pronounciation of the native name if it's not ascii, e.g.
    // if native is '座旗', name_pronounce will be the pidgin 'Zuòqí'
    char           *name_pronounce;
    // True if the user prefers to see the native names instead of the
    // translated ones. For example english speaker prefer to see the latin
    // native constellation names like 'Ursa Major' instead of 'Great Bear'
    bool           user_prefer_native;
} cultural_name_t;

/*
 * Function: skyculture_parse_edge
 * Parse constellation edges.
 *
 * Parameters:
 *   edges  - json array of the edges (see western skyculture json).
 *   infos  - Constellation info to update with the edge data.
 *   size   - Size of the infos array.
 *
 * Return:
 *   The number of lines parsed, or -1 in case of error.
 */
int skyculture_parse_edges(const json_value *edges,
                           constellation_infos_t *infos, int size);

/*
 * Function: skyculture_compute_edges_rot
 * Compute the rotation matrix from an edges_epoch string to J2000.
 *
 * Parameters:
 *   epoch - The edges_epoch value, e.g. "B1875", "J2000", "J2017.5278633264887"
 *           or "JDxxxxx".  NULL or an empty string default to "B1875" (the
 *           historical engine default).
 *   out   - The resulting 3x3 rotation matrix (epoch -> J2000).
 */
void skyculture_compute_edges_rot(const char *epoch, double out[3][3]);


// Experimental parsing functions for the new json format.

int skyculture_parse_feature_json(skyculture_name_t **names_hash,
                                  const json_value *v,
                                  constellation_infos_t *feature);

// Parse an asterism entry (see the "asterisms" array of the skyculture
// json).  Asterisms are stored in a constellation_infos_t and use the same
// rendering pipeline as constellations.
int skyculture_parse_asterism_json(skyculture_name_t **names_hash,
                                   const json_value *v,
                                   constellation_infos_t *asterism);

skyculture_name_t *skyculture_parse_names_json(const json_value *v);

#endif // SKYCULTURE_H
