/* Stellarium Web Engine - Copyright (c) 2022 - Stellarium Labs SRL
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */


/*
 * Function: u8_char_len
 * Return the size of a given utf-8 character.
 */
int u8_char_len(const char *c);

/*
 * Function: u8_char_code
 * Return the unicode point for a given utf-8 char.
 */
int u8_char_code(const char *c);

/*
 * Function: u8_lower
 * Make an utf-8 string lowercase.
 *
 * Parameters:
 *   dst    - destination buffer.
 *   str    - source string.
 *   size   - size of the destination buffer.  We always NULL terminate the
 *            string.
 */
void u8_lower(char *dst, const char *str, int size);

/*
 * Function: u8_upper
 * Make an utf-8 string uppercase.
 *
 * Parameters:
 *   dst    - destination buffer.
 *   str    - source string.
 *   size   - size of the destination buffer.  We always NULL terminate the
 *            string.
 */
void u8_upper(char *dst, const char *str, int size);

/*
 * Function: u8_remove_accents
 * Replace accents to non accentuated letters in a utf8 string
 *
 * dst and str can be the same.
 */
void u8_remove_accents(char *dst, const char *str, int n);

/*
 * Function: u8_char_len
 * Return the number of characters in a utf-8 string.
 */
int u8_len(const char *str);

/*
 * Function: u8_split_line
 * Split a single utf-8 string into several lines.
 *
 * Return the (visual) length of the longest line.
 *
 * Parameters:
 *   dst        - destination buffer, can be equal to src for in-place work
 *   len        - size of the destination buffer
 *   src        - source string
 *   min_chars  - minimum number of charaters of a line
 */
int u8_split_line(char *dst, int len, const char *src, int min_chars);

/*
 * Function: u8_is_mongolian_visible
 * True for the visible characters of the Mongolian block (U+1800-18AF),
 * i.e. the Mongolian/Manchu letters.  The control and variant selectors
 * (Nirugu, FVS1-3, MVS, reserved) are excluded: they must never be
 * stacked as their own vertical line.
 */
int u8_is_mongolian_visible(int code);

/*
 * Function: u8_verticalize_manchu
 * Layout a label containing Manchu (Mongolian script) text vertically:
 * Mongolian characters are stacked one per line (glyphs of a vertical
 * script font are already oriented vertically, so no rotation is needed),
 * while any non-Mongolian part (e.g. the english name) is kept horizontal
 * on its own line below the vertical column.
 *
 * Return 1 if the text contained Mongolian characters (dst was
 * verticalized), 0 otherwise (dst is a plain copy of src).
 *
 * Parameters:
 *   dst   - destination buffer
 *   size  - size of the destination buffer
 *   src   - source string
 *
 * NOTE (phase 2): correct Manchu rendering needs glyph shaping (e.g.
 * HarfBuzz) to select the proper joined glyph forms from the font GSUB
 * table; stb_truetype only fetches the default (isolated) forms.  This
 * function is the hook where shaped glyph sequences should be fed in
 * before applying the vertical layout.
 */
int u8_verticalize_manchu(char *dst, int size, const char *src);
