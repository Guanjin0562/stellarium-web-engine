/* Stellarium Web Engine - Copyright (c) 2022 - Stellarium Labs SRL
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#include "swe.h"
#include <regex.h>


static constellation_infos_t *get_constellation(
        constellation_infos_t *csts, int size, const char *id)
{
    int i;

    // Small fix for SER1 and SER2!
    if (strcasecmp(id, "SER1") == 0 || strcasecmp(id, "SER2") == 0)
        id = "SER";

    for (i = 0; i < size; i++) {
        if (strcasecmp(csts[i].iau, id) == 0)
            return &csts[i];
    }
    // Some skycultures (e.g. chinese_mdn) do not define the "iau" field in
    // their constellations.  Fallback to matching the last id component
    // ("P01" in "CON chinese_mdn P01").
    for (i = 0; i < size; i++) {
        const char *sep = strrchr(csts[i].id, ' ');
        if (sep && strcasecmp(sep + 1, id) == 0)
            return &csts[i];
    }
    return NULL;
}

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
                           constellation_infos_t *csts, int size)
{
    constellation_infos_t *info = NULL;
    const json_value *line;
    char cst[2][8];
    char edge_type = '_', edge_dir = '_';
    int i, j, ra1_h, ra1_m, ra1_s, ra2_h, ra2_m, ra2_s, nb = 0;
    char dec1_sign, dec2_sign;
    int dec1_d, dec1_m, dec1_s, dec2_d, dec2_m, dec2_s;
    double ra1, dec1, ra2, dec2;

    for (i = 0; i < edges->u.array.length; i++) {
        line = edges->u.array.values[i];
        if (line->type != json_string) {
            LOG_W("Cannot parse skyculture edge line");
            continue;
        }
        if (sscanf(line->u.string.ptr,
                         "%*s %c%c "
                         "%d:%d:%d %c%d:%d:%d "
                         "%d:%d:%d %c%d:%d:%d "
                         "%7s %7s",
                         &edge_type, &edge_dir,
                         &ra1_h, &ra1_m, &ra1_s,
                         &dec1_sign, &dec1_d, &dec1_m, &dec1_s,
                         &ra2_h, &ra2_m, &ra2_s,
                         &dec2_sign, &dec2_d, &dec2_m, &dec2_s,
                         cst[0], cst[1]) != 18) {
            LOG_W("Cannot parse skyculture edge line: %.16s...",
                  line->u.string.ptr);
            continue;
        }
        eraTf2a('+', ra1_h, ra1_m, ra1_s, &ra1);
        eraTf2a('+', ra2_h, ra2_m, ra2_s, &ra2);
        eraAf2a(dec1_sign, dec1_d, dec1_m, dec1_s, &dec1);
        eraAf2a(dec2_sign, dec2_d, dec2_m, dec2_s, &dec2);
        for (j = 0; j < 2; j++) {
            info = get_constellation(csts, size, cst[j]);
            if (!info) continue;
            if (info->nb_edges >= info->edges_cap) {
                int new_cap = info->edges_cap ? 2 * info->edges_cap : 64;
                info->edges = realloc(info->edges,
                                      new_cap * sizeof(*info->edges));
                info->edges_cap = new_cap;
            }
            info->edges[info->nb_edges].ra1 = ra1;
            info->edges[info->nb_edges].de1 = dec1;
            info->edges[info->nb_edges].ra2 = ra2;
            info->edges[info->nb_edges].de2 = dec2;
            info->edges[info->nb_edges].type = edge_type;
            info->edges[info->nb_edges].dir = edge_dir;
            info->nb_edges++;
        }
        nb++;
    }
    return nb;
}

/*
 * Function: skyculture_compute_edges_rot
 * Compute the rotation matrix from an edges_epoch string to J2000.
 *
 * The input epoch follows the same conventions as the desktop Stellarium
 * loadBoundaries(): "Bxxxx.x" (Besselian epoch), "Jxxxx.x" (Julian epoch),
 * "JDxxxxx" or a plain double (Julian date) and "J2000" which is the
 * identity.  NULL / empty / unknown values fall back to "B1875", the
 * historical default of this engine.
 */
void skyculture_compute_edges_rot(const char *epoch, double out[3][3])
{
    double djm0 = 0.0, djm;

    if (epoch && epoch[0] && strcasecmp(epoch, "J2000") == 0) {
        mat3_set_identity(out);
        return;
    }
    if (!epoch || !epoch[0]) {
        // Skycultures without an edges_epoch field (e.g. western) define
        // their boundaries in B1875 coordinates, matching the historical
        // engine behavior which hardcoded eraPnm06a(1875).
        eraEpb2jd(1875.0, &djm0, &djm);
        eraPnm06a(djm0, djm, out);
        return;
    }
    if (strcasecmp(epoch, "B1875") == 0) {
        eraEpb2jd(1875.0, &djm0, &djm);
    } else if (epoch[0] == 'B' || epoch[0] == 'b') {
        eraEpb2jd(atof(epoch + 1), &djm0, &djm);
    } else if (epoch[0] == 'J' || epoch[0] == 'j') {
        eraEpj2jd(atof(epoch + 1), &djm0, &djm);
    } else {
        // "JDxxxxx" or a pure Julian date.
        const char *jd = (epoch[0] == 'D' || epoch[0] == 'd') ? epoch + 2
                                                              : epoch;
        djm = atof(jd);
    }
    // eraPnm06a computes the precession-nutation (frame bias included)
    // matrix from J2000 to the given date.  We store it *un-transposed*:
    // the consumer applies it through mat3_mul_vec3 which computes M^T . v,
    // so the effective transformation is rnpb^T = date -> J2000, exactly
    // what we need to convert the skyculture edges coordinates to J2000.
    // (This matches the original engine which hardcoded eraPnm06a(1875).)
    eraPnm06a(djm0, djm, out);
}

/*
 * Function: parse_color_hex
 * Parse a "#rrggbb" color string into rgb float components (0..1).
 *
 * Return 0 on success, -1 otherwise.
 */
static int parse_color_hex(const char *hex, float out[3])
{
    unsigned int r, g, b;
    if (!hex || sscanf(hex, "#%2x%2x%2x", &r, &g, &b) != 3)
        return -1;
    out[0] = r / 255.0f;
    out[1] = g / 255.0f;
    out[2] = b / 255.0f;
    return 0;
}

static double json_to_double(const json_value *v)
{
    if (v->type == json_integer) return v->u.integer;
    if (v->type == json_double) return v->u.dbl;
    return 0;
}

/*
 * Parse the lines array of a constellation or asterism feature.
 * Each segment is a chain of star references that is expanded into
 * consecutive couples, e.g. [a, b, c] becomes (a,b) and (b,c).
 *
 * Star references can be either a HIP number (integer) or a "DSO:xxx"
 * designation string.  Other strings (e.g. gaia ids) are skipped.
 */
static int parse_line_point(const json_value *pt,
                            constellation_infos_t *feature,
                            constellation_line_t *line, int idx);
static int parse_lines_json(const json_value *v,
                            constellation_infos_t *feature)
{
    int i, j, n, nb = 0;
    const json_value *seg;
    uint8_t line_weight;
    json_value **values;
    constellation_line_t *lines = feature->lines;
    const int max_lines = ARRAY_SIZE(feature->lines);

    if (v->type != json_array) return -1;
    for (i = 0; i < v->u.array.length; i++) {
        seg = v->u.array.values[i];
        if (seg->type != json_array) return -1;
        n = seg->u.array.length - 1;
        line_weight = LINE_WEIGHT_NORMAL;
        values = seg->u.array.values;

        for (j = 0; j < n; j++) {
            // The first value can be a string defining the line style.
            if (j == 0 && values[0]->type == json_string &&
                (strcmp(values[0]->u.string.ptr, "thin") == 0 ||
                 strcmp(values[0]->u.string.ptr, "bold") == 0)) {
                line_weight = strcmp(values[0]->u.string.ptr, "thin") == 0 ?
                              LINE_WEIGHT_THIN : LINE_WEIGHT_BOLD;
                continue;
            }
            if (nb >= max_lines) {
                LOG_E("Too many lines in sky culture feature");
                return nb;
            }
            memset(&lines[nb], 0, sizeof(lines[nb]));
            lines[nb].dso[0] = lines[nb].dso[1] = -1;
            if (parse_line_point(values[j], feature, &lines[nb], 0)) continue;
            if (parse_line_point(values[j + 1], feature, &lines[nb], 1))
                continue;
            lines[nb].line_weight = line_weight;
            nb++;
        }
    }
    return nb;
}

/*
 * Set one endpoint of a constellation line from a json value.
 * Returns 0 on success, or -1 if the value cannot be used as a line
 * endpoint (the line must then be skipped).
 */
static int parse_line_point(const json_value *pt,
                            constellation_infos_t *feature,
                            constellation_line_t *line, int idx)
{
    int k;
    if (pt->type == json_integer) {
        line->hip[idx] = pt->u.integer;
        return 0;
    }
    if (pt->type == json_string &&
        strncmp(pt->u.string.ptr, "DSO:", 4) == 0) {
        const char *dsn = pt->u.string.ptr + 4;
        // Find or add the designation in the feature dso names array.
        for (k = 0; k < feature->nb_dso_names; k++)
            if (strcmp(feature->dso_names[k], dsn) == 0) break;
        if (k == feature->nb_dso_names) {
            if (k >= (int)ARRAY_SIZE(feature->dso_names)) {
                LOG_E("Too many DSO references in sky culture feature");
                return -1;
            }
            feature->dso_names[k] = strdup(dsn);
            feature->nb_dso_names++;
        }
        line->hip[idx] = 0;
        line->dso[idx] = k;
        return 0;
    }
    return -1; // Unsupported reference, skip this line.
}

// True if the lines array is a coordinate based (dark) polyline list, i.e.
// lines[i][j] are [ra, dec] arrays instead of star ids.
static bool lines_are_dark(const json_value *lines)
{
    const json_value *seg;
    if (!lines || lines->type != json_array || lines->u.array.length == 0)
        return false;
    seg = lines->u.array.values[0];
    return seg->type == json_array && seg->u.array.length > 0 &&
           seg->u.array.values[0]->type == json_array;
}

/*
 * Flatten dark coordinate polylines ([[ra, dec], ...] segments) into
 * consecutive [ra, dec] couples in feature->coords.  The coordinates are
 * stored as radians, with the right ascension in hours converted through
 * the same "* 15 * DD2R" factor used by the desktop version
 * (Constellation.cpp: RA * M_PI_180 * 15).
 */
static void parse_dark_coords(const json_value *lines,
                              constellation_infos_t *feature)
{
    int i, j;
    const json_value *seg, *pt;
    double prev[2];
    bool has_prev = false;
    for (i = 0; i < lines->u.array.length; i++) {
        seg = lines->u.array.values[i];
        if (seg->type != json_array) continue;
        has_prev = false;
        for (j = 0; j < seg->u.array.length; j++) {
            pt = seg->u.array.values[j];
            if (pt->type != json_array || pt->u.array.length < 2)
                continue;
            if (feature->nb_coords + 2 > ARRAY_SIZE(feature->coords))
                break;
            double ra = json_to_double(pt->u.array.values[0]) * 15 * DD2R;
            double de = json_to_double(pt->u.array.values[1]) * DD2R;
            if (has_prev) {
                feature->coords[feature->nb_coords][0] = prev[0];
                feature->coords[feature->nb_coords][1] = prev[1];
                feature->coords[feature->nb_coords + 1][0] = ra;
                feature->coords[feature->nb_coords + 1][1] = de;
                feature->nb_coords += 2;
            }
            prev[0] = ra;
            prev[1] = de;
            has_prev = true;
        }
    }
}

// Parse the label_positions array (each entry is a [ra (hours), dec
// (degrees)] couple) into feature->label_pos.
static void parse_label_positions_json(const json_value *label_positions,
                                       constellation_infos_t *feature)
{
    int i;
    const json_value *seg;
    for (i = 0; label_positions && label_positions->type == json_array &&
               i < label_positions->u.array.length &&
               i < (int)ARRAY_SIZE(feature->label_pos); i++) {
        seg = label_positions->u.array.values[i];
        if (seg->type != json_array || seg->u.array.length < 2)
            continue;
        feature->label_pos[i][0] =
                json_to_double(seg->u.array.values[0]) * 15 * DD2R;
        feature->label_pos[i][1] =
                json_to_double(seg->u.array.values[1]) * DD2R;
        feature->nb_label_pos++;
    }
}

// Parse the label_offset array ([dRA (degrees), dDec (degrees)]) into
// feature->label_offset.
static void parse_label_offset_json(const json_value *label_offset,
                                    constellation_infos_t *feature)
{
    if (!label_offset || label_offset->type != json_array ||
        label_offset->u.array.length < 2)
        return;
    feature->label_offset[0] =
            json_to_double(label_offset->u.array.values[0]) * DD2R;
    feature->label_offset[1] =
            json_to_double(label_offset->u.array.values[1]) * DD2R;
    feature->has_label_offset = true;
}

/*
 * Function: parse_image_json
 * Parse the "image" object shared by constellations and asterisms.
 */
static int parse_image_json(const json_value *image,
                            constellation_infos_t *feature)
{
    const char *img = NULL;
    int r, i, w, h, x, y, hip;
    json_value *anchors[3];

    r = jcon_parse(image, "{",
        "file", JCON_STR(img),
        "size", "[", JCON_INT(w, 0), JCON_INT(h, 0), "]",
        "anchors", "[",
            JCON_VAL(anchors[0]),
            JCON_VAL(anchors[1]),
            JCON_VAL(anchors[2]),
        "]",
    "}");
    if (r) return r;

    snprintf(feature->img, sizeof(feature->img), "%s", img);
    for (i = 0; i < 3; i++) {
        r = jcon_parse(anchors[i], "{",
            "pos", "[", JCON_INT(x, 0), JCON_INT(y, 0), "]",
            "hip", JCON_INT(hip, 0),
        "}");
        if (r) return r;
        feature->anchors[i].hip = hip;
        feature->anchors[i].uv[0] = (double)x / w;
        feature->anchors[i].uv[1] = (double)y / h;
    }
    return 0;
}

int skyculture_parse_feature_json(skyculture_name_t** names_hash,
                                  const json_value *v,
                                  constellation_infos_t *feature)
{
    const char *id, *iau = NULL;
    const char *english = NULL, *native = NULL, *pronounce = NULL;
    const char *ipa = NULL;
    const char *name_description = NULL, *description = NULL, *color = NULL;
    int r;
    double line_width = 0;
    const json_value *lines = NULL, *common_name = NULL, *image = NULL;
    const json_value *label_positions = NULL, *label_offset = NULL;
    skyculture_name_t *entry;

    feature->type = SC_TYPE_CONSTELLATION;
    feature->line_color[0] = -1; // Use the default line color.
    feature->line_width = 0;

    r = jcon_parse(v, "{",
        "id", JCON_STR(id),
        "?iau", JCON_STR(iau),
        "?common_name", JCON_VAL(common_name),
        "?lines", JCON_VAL(lines),
        "?label_positions", JCON_VAL(label_positions),
        "?label_offset", JCON_VAL(label_offset),
        "?description", JCON_STR(description),
        "?image", JCON_VAL(image),
        "?line_color", JCON_STR(color),
        "?line_width", JCON_DOUBLE(line_width, 0),
    "}");
    if (r) goto error;

    snprintf(feature->id, sizeof(feature->id), "%s", id);
    if (color) {
        if (parse_color_hex(color, feature->line_color))
            LOG_W("Cannot parse line_color in constellation %s", id);
    }
    feature->line_width = line_width;

    // Loads common name directly in the names hash
    if (common_name) {
        r = jcon_parse(common_name, "{",
            "?english", JCON_STR(english),
            "?native", JCON_STR(native),
            "?pronounce", JCON_STR(pronounce),
            "?IPA", JCON_STR(ipa),
            "?description", JCON_STR(name_description),
        "}");
        if (r) goto error;
        entry = calloc(1, sizeof(*entry));
        entry->main_id = strdup(id);
        if (english)
            entry->name_english = strdup(english);
        if (native)
            entry->name_native = strdup(native);
        if (pronounce)
            entry->name_pronounce = strdup(pronounce);
        if (ipa)
            entry->name_ipa = strdup(ipa);
        if (name_description)
            entry->name_description = strdup(name_description);
        HASH_ADD_KEYPTR(hh, *names_hash, entry->main_id,
                        strlen(entry->main_id), entry);
    }
    if (description)
        feature->description = strdup(description);
    if (iau)
        snprintf(feature->iau, sizeof(feature->iau), "%s", iau);

    if (lines) {
        // Constellations may be defined by [ra, dec] coordinate polylines
        // instead of star references (e.g. the manchu "manchutest" dark
        // constellations).  They use the exact same storage and rendering
        // path as the dark asterisms.
        if (lines_are_dark(lines))
            parse_dark_coords(lines, feature);
        else {
            feature->nb_lines = parse_lines_json(lines, feature);
            if (feature->nb_lines < 0) goto error;
        }
    }

    // Custom label positions / offset (same fields as the asterisms).
    parse_label_positions_json(label_positions, feature);
    parse_label_offset_json(label_offset, feature);

    if (image) {
        r = parse_image_json(image, feature);
        if (r) goto error;
    }
    return 0;

error:
    LOG_E("Cannot parse json feature");
    return -1;
}

skyculture_name_t *skyculture_parse_names_json(const json_value *v)
{
    int i, j, r;
    const char *key;
    skyculture_name_t *ret = NULL, *entry, *prev_entry;
    const json_value *names_obj;
    const char *english = NULL, *native = NULL, *pronounce = NULL;
    const char *ipa = NULL;

    if (v->type != json_object) goto error;
    for (i = 0; i < v->u.object.length; i++) {
        key = v->u.object.values[i].name;

        if (v->u.object.values[i].value->type != json_array)
            goto error;

        for (j = 0; j < v->u.object.values[i].value->u.array.length; j++) {
            names_obj = v->u.object.values[i].value->u.array.values[j];

            r = jcon_parse(names_obj, "{",
                "?english", JCON_STR(english),
                "?native", JCON_STR(native),
                "?pronounce", JCON_STR(pronounce),
                "?IPA", JCON_STR(ipa),
            "}");
            if (r) goto error;
            entry = calloc(1, sizeof(*entry));
            entry->main_id = strdup(key);
            if (english)
                entry->name_english = strdup(english);
            if (native)
                entry->name_native = strdup(native);
            if (pronounce)
                entry->name_pronounce = strdup(pronounce);
            if (ipa)
                entry->name_ipa = strdup(ipa);

            if (j > 0) {
                prev_entry->alternative = entry;
            } else {
                HASH_ADD_KEYPTR(hh, ret, entry->main_id,
                                strlen(entry->main_id), entry);
            }
            prev_entry = entry;
        }
    }

    return ret;

error:

    LOG_E("Cannot parse skyculture names");
    return NULL;
}

/*
 * Function: skyculture_parse_asterism_json
 * Parse an asterism entry from the "asterisms" array of a skyculture.
 *
 * Asterisms are stored in a constellation_infos_t and are rendered through
 * the exact same pipeline as constellations.  Depending on their content they
 * are classified as:
 *   - ray helpers (is_ray_helper), drawn as yellow lines without labels;
 *   - regular asterisms (hip based polylines), drawn with a blue-ish color;
 *   - dark asterisms (coordinate based polylines, e.g. chinese walls and
 *     lunar mansions boundaries), drawn as [ra, dec] outlines.
 */
int skyculture_parse_asterism_json(skyculture_name_t **names_hash,
                                   const json_value *v,
                                   constellation_infos_t *asterism)
{
    const char *id, *english = NULL, *native = NULL, *pronounce = NULL;
    const char *ipa = NULL;
    const char *name_description = NULL, *color = NULL;
    bool is_ray_helper = false;
    double line_width = 0, single_star_radius = 0;
    int r;
    const json_value *lines = NULL, *common_name = NULL, *image = NULL,
                     *label_positions = NULL, *label_offset = NULL;
    skyculture_name_t *entry;

    asterism->type = SC_TYPE_ASTERISM;
    asterism->line_color[0] = -1;

    r = jcon_parse(v, "{",
        "id", JCON_STR(id),
        "?lines", JCON_VAL(lines),
        "?common_name", JCON_VAL(common_name),
        "?is_ray_helper", JCON_BOOL(is_ray_helper, false),
        "?image", JCON_VAL(image),
        "?label_positions", JCON_VAL(label_positions),
        "?label_offset", JCON_VAL(label_offset),
        "?single_star_radius", JCON_DOUBLE(single_star_radius, 0),
        "?line_color", JCON_STR(color),
        "?line_width", JCON_DOUBLE(line_width, 0),
    "}");
    if (r) goto error;

    snprintf(asterism->id, sizeof(asterism->id), "%s", id);
    asterism->line_width = line_width;
    asterism->single_star_radius = single_star_radius;

    // Detect the feature type.  Dark asterisms are detected by their
    // coordinate based lines: [[ra, dec], ...].
    if (lines_are_dark(lines))
        asterism->type = SC_TYPE_DARK_ASTERISM;
    if (is_ray_helper && asterism->type != SC_TYPE_DARK_ASTERISM)
        asterism->type = SC_TYPE_RAY_HELPER;

    // Default line colors, matching the desktop version.
    if (asterism->type == SC_TYPE_RAY_HELPER)
        vec3_set(asterism->line_color, 1.0, 1.0, 0.0);
    else
        vec3_set(asterism->line_color, 0.4, 0.4, 0.8);
    if (color) {
        if (parse_color_hex(color, asterism->line_color))
            LOG_W("Cannot parse line_color in asterism %s", id);
    }

    if (lines) {
        if (asterism->type == SC_TYPE_DARK_ASTERISM) {
            // Each line is a polyline of [ra (hours), dec (degrees)]
            // coordinates that we flatten into consecutive segments.
            parse_dark_coords(lines, asterism);
        } else {
            asterism->nb_lines = parse_lines_json(lines, asterism);
            if (asterism->nb_lines < 0) goto error;
        }
    }

    if (common_name) {
        r = jcon_parse(common_name, "{",
            "?english", JCON_STR(english),
            "?native", JCON_STR(native),
            "?pronounce", JCON_STR(pronounce),
            "?IPA", JCON_STR(ipa),
            "?description", JCON_STR(name_description),
        "}");
        if (r) goto error;
        entry = calloc(1, sizeof(*entry));
        entry->main_id = strdup(id);
        if (english)
            entry->name_english = strdup(english);
        if (native)
            entry->name_native = strdup(native);
        if (pronounce)
            entry->name_pronounce = strdup(pronounce);
        if (ipa)
            entry->name_ipa = strdup(ipa);
        if (name_description)
            entry->name_description = strdup(name_description);
        HASH_ADD_KEYPTR(hh, *names_hash, entry->main_id,
                        strlen(entry->main_id), entry);
    }

    if (image) {
        r = parse_image_json(image, asterism);
        if (r) goto error;
    }

    // Custom label positions, as [ra (hours), dec (degrees)] couples.
    parse_label_positions_json(label_positions, asterism);
    parse_label_offset_json(label_offset, asterism);
    return 0;

error:
    LOG_E("Cannot parse json asterism");
    return -1;
}
