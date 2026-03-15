typedef struct {
	u8    *base;
	usize len;
} U8_Slice;

typedef struct {
	u16      units_per_em;
	u16      index_to_loca_format;
	u16      num_glyphs;
	U8_Slice cmap_subtable;
	U8_Slice loca_table;
	U8_Slice glyf_table;
} TTF_Font;

typedef struct {
	b16 on_curve;
	u16 prev_offset_plus_one;
	i32 x; // 26.6 fixed-point
	i32 y; // 26.6 fixed-point
} Glyph_Vertex;

typedef struct {
	i32          x_min;
	i32          y_min;
	i32          x_max;
	i32          y_max;
	usize        num_verts;
	Glyph_Vertex *verts;
} Glyph_Outline;

function U8_Slice u8_subslice(U8_Slice slice, usize offset, usize len) {
	U8_Slice subslice = {0};
	if (len <= slice.len && offset <= slice.len - len) {
		subslice.base = slice.base + offset;
		subslice.len = len;
	}
	return subslice;
}

function i16 read_i16be_unchecked(U8_Slice slice, usize offset) {
	u8 *ptr = slice.base + offset;
	return (i16)(ptr[0] << 8) | ptr[1];
}

function u16 read_u16be_unchecked(U8_Slice slice, usize offset) {
	u8 *ptr = slice.base + offset;
	return (u16)(ptr[0] << 8) | ptr[1];
}

function u32 read_u32be_unchecked(U8_Slice slice, usize offset) {
	u8 *ptr = slice.base + offset;
	return (u32)(ptr[0] << 24) | (u32)(ptr[1] << 16) | (u32)(ptr[2] << 8) | ptr[3];
}

function TTF_Font font_make(U8_Slice data) {
	u16 num_tables = 0;
	// read offset subtable
	if (data.len >= 12) {
		num_tables = read_u16be_unchecked(data, 4);
	}

	// read table directory
	U8_Slice table_directory = u8_subslice(data, 12, (usize)num_tables * 16);
	U8_Slice head_table = {0};
	U8_Slice hhea_table = {0};
	U8_Slice hmtx_table = {0};
	U8_Slice maxp_table = {0};
	U8_Slice cmap_table = {0};
	U8_Slice loca_table = {0};
	U8_Slice glyf_table = {0};
	for (usize entry = 0; entry < table_directory.len; entry += 16) {
		u32 tag = read_u32be_unchecked(table_directory, entry + 0);
		u32 offset = read_u32be_unchecked(table_directory, entry + 8);
		u32 length = read_u32be_unchecked(table_directory, entry + 12);

		U8_Slice table = u8_subslice(data, offset, length);

		switch (tag) {
		case 0x68656164: head_table = u8_subslice(table, 0, 54); break;
		case 0x68686561: hhea_table = table; break;
		case 0x686D7478: hmtx_table = table; break;
		case 0x6D617870: maxp_table = table; break;
		case 0x636D6170: cmap_table = table; break;
		case 0x6C6F6361: loca_table = table; break;
		case 0x676C7966: glyf_table = table; break;
		}
	}
	b32 has_required_tables = head_table.len != 0;
	has_required_tables &= hhea_table.len != 0;
	has_required_tables &= hmtx_table.len != 0;
	has_required_tables &= maxp_table.len != 0;
	has_required_tables &= cmap_table.len != 0;
	has_required_tables &= loca_table.len != 0;
	has_required_tables &= glyf_table.len != 0;

	// process required tables
	b32 required_tables_ok = FALSE;
	u16 units_per_em;
	u16 index_to_loca_format;
	u16 num_glyphs;
	U8_Slice cmap_subtable;
	if (has_required_tables) {
		// read head table
		b32 head_ok = FALSE;
		{
			ASSERT(head_table.len == 54);
			units_per_em = read_u16be_unchecked(head_table, 18);
			index_to_loca_format = read_u16be_unchecked(head_table, 50);
			head_ok = units_per_em >= 64 && units_per_em <= 16348;
			head_ok &= index_to_loca_format == 0 || index_to_loca_format == 1;
		}

		// read maxp table
		b32 maxp_ok = FALSE;
		if (maxp_table.len >= 6) {
			num_glyphs = read_u16be_unchecked(maxp_table, 4);
			maxp_ok = TRUE;
		}

		// validate loca table
		b32 loca_ok = maxp_ok;
		if (maxp_ok && index_to_loca_format == 0) {
			usize expected_size = ((usize)num_glyphs + 1) * 2;
			loca_table = u8_subslice(loca_table, 0, expected_size);
			loca_ok = loca_table.len == expected_size;

			u32 prev_glyf_tbl_offset = 0;
			for (usize offset = 0; offset < loca_table.len; offset += 2) {
				u32 glyf_tbl_offset = (u32)read_u16be_unchecked(loca_table, offset) * 2;
				loca_ok &= glyf_tbl_offset <= glyf_table.len;
				loca_ok &= prev_glyf_tbl_offset <= glyf_tbl_offset;
				prev_glyf_tbl_offset = glyf_tbl_offset;

				if (!loca_ok) {
					break;
				}
			}
		}
		else if (maxp_ok && index_to_loca_format == 1) {
			usize expected_size = ((usize)num_glyphs + 1) * 4;
			loca_table = u8_subslice(loca_table, 0, expected_size);
			loca_ok = loca_table.len == expected_size;

			u32 prev_glyf_tbl_offset = 0;
			for (usize offset = 0; offset < loca_table.len; offset += 4) {
				u32 glyf_tbl_offset = read_u32be_unchecked(loca_table, offset);
				loca_ok &= glyf_tbl_offset <= glyf_table.len;
				loca_ok &= prev_glyf_tbl_offset <= glyf_tbl_offset;
				prev_glyf_tbl_offset = glyf_tbl_offset;

				if (!loca_ok) {
					break;
				}
			}
		}

		// select cmap subtable
		b32 cmap_ok = FALSE;
		{
			U8_Slice subtable_directory = {0};
			if (cmap_table.len >= 4) {
				u16 num_subtables = read_u16be_unchecked(cmap_table, 2);
				subtable_directory = u8_subslice(cmap_table, 4, (usize)num_subtables * 8);
			}

			b32 subtable_selected = FALSE;
			u8 selected_subtable_priority = 0;
			for (u16 entry = 0; entry < subtable_directory.len; entry += 8) {
				u16 platform_id = read_u16be_unchecked(subtable_directory, entry + 0);
				u16 encoding_id = read_u16be_unchecked(subtable_directory, entry + 2);
				u32 subtable_off = read_u32be_unchecked(subtable_directory, entry + 4);

				U8_Slice subtable = u8_subslice(cmap_table, subtable_off, 4);
				if (subtable.len == 4) {
					u16 subtable_size = read_u16be_unchecked(subtable, 2);
					subtable = u8_subslice(cmap_table, subtable_off, subtable_size);
				}

				b32 format_ok = FALSE;
				if (subtable.len > 4) {
					u16 format = read_u16be_unchecked(subtable, 0);
					format_ok = format == 0;
					format_ok |= format == 2;
					format_ok |= format == 4;
					format_ok |= format == 6;
					format_ok |= format == 8;
					format_ok |= format == 10;
					format_ok |= format == 12;
					format_ok |= format == 13;
					// format_ok |= format == 14;
				}

				u8 priority = 0;
				if (format_ok) {
					ASSERT(subtable.len > 4);
					switch (platform_id) {
					case 0:
						switch (encoding_id) {
						case 6: priority = 13; break; // Unicode full repertoire
						case 4: priority = 12; break; // Unicode 2.0 full
						case 3: priority = 11; break; // Unicode 2.0 BMP
						case 2: priority = 10; break; // ISO 10646
						case 1: priority = 9; break;  // Unicode 1.1
						case 0: priority = 8; break;  // Unicode 1.0
						case 5: priority = 7; break;  // Variation sequences
						}
						break;
					case 3:
						switch (encoding_id) {
						case 10: priority = 14; break; // UCS-4 (BEST)
						case 1: priority = 9; break;   // Unicode BMP
						case 0: priority = 5; break;   // Symbol
						case 2: priority = 2; break;   // ShiftJIS
						case 3: priority = 2; break;   // PRC
						case 4: priority = 2; break;   // Big5
						case 5: priority = 2; break;   // Wansung
						}
						break;
					}
				}

				if (priority > selected_subtable_priority) {
					cmap_subtable = subtable;
					selected_subtable_priority = priority;
					subtable_selected = TRUE;
				}
			}
			cmap_ok = subtable_selected;
		}

		required_tables_ok = head_ok;
		required_tables_ok &= maxp_ok;
		required_tables_ok &= loca_ok;
		required_tables_ok &= cmap_ok;
	}

	// process optional tables
	if (required_tables_ok) {
	}

	TTF_Font font = {0};
	if (required_tables_ok) {
		font.units_per_em = units_per_em;
		font.index_to_loca_format = index_to_loca_format;
		font.num_glyphs = num_glyphs;
		font.cmap_subtable = cmap_subtable;
		font.loca_table = loca_table;
		font.glyf_table = glyf_table;
	}
	return font;
}

function u16 font_glyph_index_from_codepoint(TTF_Font font, u32 codepoint) {
	U8_Slice subtable = font.cmap_subtable;

	b32 format_ok = FALSE;
	u16 format = 0;
	if (font.cmap_subtable.len > 4) {
		format = read_u16be_unchecked(subtable, 0);
		format_ok = TRUE;
	}

	u16 glyph_index = 0;
	if (format_ok) {
		switch (format) {
		case 0:
		{
			U8_Slice index_array = u8_subslice(subtable, 6, 256);
			if (index_array.len == 256 && codepoint < 256) {
				glyph_index = read_u16be_unchecked(index_array, codepoint);
			}
		} break;
		case 2:
		{
			// TODO(byron): implement this
			ASSERT(FALSE);
		} break;
		case 4:
		{
			// get arrays
			b32 arrays_ok = FALSE;
			U8_Slice end_codes;
			U8_Slice start_codes;
			U8_Slice deltas;
			U8_Slice range_offsets;
			usize range_offsets_off;
			if (subtable.len >= 16) {
				u16 num_segments_x2 = read_u16be_unchecked(subtable, 6);
				usize end_codes_off = 14;
				usize start_codes_off = end_codes_off + num_segments_x2 + 2;
				usize deltas_off = start_codes_off + num_segments_x2;
				range_offsets_off = deltas_off + num_segments_x2;
				end_codes = u8_subslice(subtable, end_codes_off, num_segments_x2);
				start_codes = u8_subslice(subtable, start_codes_off, num_segments_x2);
				deltas = u8_subslice(subtable, deltas_off, num_segments_x2);
				range_offsets = u8_subslice(subtable, range_offsets_off, num_segments_x2);
				arrays_ok = range_offsets.len == num_segments_x2;
				arrays_ok &= (num_segments_x2 & 0x1) == 0;
			}

			// lookup codepoint
			if (arrays_ok) {
				// find segment offset
				usize offset = 0;
				for (; offset < end_codes.len; offset += 2) {
					u16 end_code = read_u16be_unchecked(end_codes, offset);
					if (codepoint <= end_code) {
						break;
					}
				}

				if (offset < end_codes.len) {
					u16 start_code = read_u16be_unchecked(start_codes, offset);
					u16 delta = read_u16be_unchecked(deltas, offset);
					u16 range_off = read_u16be_unchecked(range_offsets, offset);
					if (codepoint >= start_code && range_off == 0) {
						glyph_index = (codepoint + delta) & 0xFFFF;
					}
					else if (codepoint >= start_code) {
						ASSERT(subtable.len > 1); // no reason this should ever fail
						usize lookup_offset = (range_offsets_off + offset) + range_off +
						                      2 * (codepoint - start_code);
						if (lookup_offset < subtable.len - 1) {
							glyph_index = read_u16be_unchecked(subtable, lookup_offset);
						}

						if (glyph_index != 0) {
							glyph_index += delta;
						}
					}
				}
			}
		} break;

		case 6:
		{
			// TODO(byron): implement this
			ASSERT(FALSE);
		} break;
		}
	}
	return glyph_index;
}

function U8_Slice get_glyph_data_range(TTF_Font font, u16 glyph_index) {
	// The loca table is validated within the font_make function
	U8_Slice slice = {0};
	if (glyph_index < font.num_glyphs) {
		u32 start;
		u32 end;
		if (font.index_to_loca_format == 0) {
			ASSERT(font.loca_table.len >= 2 && font.loca_table.len - 2 >= glyph_index * 2 + 2);
			start = read_u16be_unchecked(font.loca_table, (usize)glyph_index * 2);
			end = read_u16be_unchecked(font.loca_table, (usize)glyph_index * 2 + 2);
			start *= 2;
			end *= 2;
		}
		else {
			ASSERT(font.loca_table.len >= 4 && font.loca_table.len - 4 >= glyph_index * 4 + 4);
			start = read_u32be_unchecked(font.loca_table, (usize)glyph_index * 4);
			end = read_u32be_unchecked(font.loca_table, (usize)glyph_index * 4 + 4);
		}
		ASSERT(end >= start);
		slice = u8_subslice(font.glyf_table, start, end - start);
	}
	return slice;
}

function i32 glyf_read_coordinate(
	U8_Slice coords,
	usize *offset,
	b32 short_vec,
	b32 same_or_positive
) {
	i32 coord = 0;
	usize off = *offset;
	if (short_vec) {
		coord = (i32)coords.base[off];
		if (!same_or_positive) {
			coord = -coord;
		}
		off += 1;
	}
	else if (!same_or_positive) {
		coord = read_i16be_unchecked(coords, off);
		off += 2;
	}
	*offset = off;
	return coord;
}

function void unpack_simple_glyf_outline(
  Arena         *arena,
  Glyph_Outline *outline,
  TTF_Font      font,
	u16           glyph_index
) {
	U8_Slice glyph_data = get_glyph_data_range(font, glyph_index);

	i16 num_contours = 0;
	if (glyph_data.len >= 10) {
		num_contours = read_i16be_unchecked(glyph_data, 0);
		glyph_data = u8_subslice(glyph_data, 10, glyph_data.len - 10);
	}

	// get glyf table arrays
	b32 arrays_ok = FALSE;
	U8_Slice end_points;
	U8_Slice instructions;
	U8_Slice flags;
	U8_Slice x_coords;
	U8_Slice y_coords;
	if (num_contours > 0) {
		usize num_instructions_off = (usize)num_contours * 2;
		usize instructions_off = num_instructions_off + 2;

		u16 num_instructions = 0;
		if (glyph_data.len >= num_instructions_off + 2) {
			num_instructions = read_u16be_unchecked(glyph_data, num_instructions_off);
		}

		usize flags_off = instructions_off + num_instructions;

		end_points   = u8_subslice(glyph_data, 0, (usize)num_contours * 2);
		instructions = u8_subslice(glyph_data, instructions_off, num_instructions);
		flags        = u8_subslice(glyph_data, flags_off, glyph_data.len - flags_off);

		b32 end_points_ok = FALSE;
		u16 num_points = 0;
		if (end_points.len == (usize)num_contours * 2) {
			end_points_ok = TRUE;
			num_points = read_u16be_unchecked(end_points, (usize)(num_contours - 1) * 2) + 1;
		}

		b32 flags_ok = end_points_ok;
		usize num_counted_points = 0;
		usize flags_len = 0;
		usize x_coords_len = 0;
		usize y_coords_len = 0;
		for (; flags_len < flags.len;) {
			u8 flag = flags.base[flags_len];
			flags_len += 1;

			b8 x_short_vec        = (flag & (1 << 1)) != 0;
			b8 y_short_vec        = (flag & (1 << 2)) != 0;
			b8 repeat             = (flag & (1 << 3)) != 0;
			b8 x_same_or_positive = (flag & (1 << 4)) != 0;
			b8 y_same_or_positive = (flag & (1 << 5)) != 0;

			u8 num_repeats = 0;
			if (repeat && flags_len < flags.len) {
				num_repeats = flags.base[flags_len];
				flags_len += 1;
			}
			else if (repeat) {
				flags_ok = FALSE;
				break;
			}
			usize num_verts = 1 + num_repeats;
			num_counted_points += num_verts;

			if (x_short_vec) {
				x_coords_len += num_verts;
			}
			else if (!x_same_or_positive) {
				x_coords_len += num_verts * 2;
			}

			if (y_short_vec) {
				y_coords_len += num_verts;
			}
			else if (!y_same_or_positive) {
				y_coords_len += num_verts * 2;
			}

			if (num_counted_points >= num_points) {
				break;
			}
		}

		flags = u8_subslice(glyph_data, flags_off, flags_len);
		flags_ok &= num_counted_points == num_points;
		flags_ok &= flags.len == flags_len;

		usize x_coords_off = flags_off + flags_len;
		usize y_coords_off = x_coords_off + x_coords_len;
		x_coords = u8_subslice(glyph_data, x_coords_off, x_coords_len);
		y_coords = u8_subslice(glyph_data, y_coords_off, y_coords_len);

		arrays_ok = flags_ok;
		arrays_ok &= x_coords.len == x_coords_len;
		arrays_ok &= y_coords.len == y_coords_len;
	}

	// unpack x & y coordinates
	if (arrays_ok) {
		usize x_coord_off = 0;
		usize y_coord_off = 0;
		i32 x = 0, y = 0;
		for (usize flag_off = 0; flag_off < flags.len;) {
			u8 flag = flags.base[flag_off];
			flag_off += 1;

			b8 on_curve           = (flag & (1 << 0)) != 0;
			b8 x_short_vec        = (flag & (1 << 1)) != 0;
			b8 y_short_vec        = (flag & (1 << 2)) != 0;
			b8 repeat             = (flag & (1 << 3)) != 0;
			b8 x_same_or_positive = (flag & (1 << 4)) != 0;
			b8 y_same_or_positive = (flag & (1 << 5)) != 0;

			u8 num_repeats = 0;
			if (repeat && flag_off < flags.len) {
				num_repeats = flags.base[flag_off];
				flag_off += 1;
			}

			usize num_verts = 1 + (usize)num_repeats;
			Glyph_Vertex *verts = (Glyph_Vertex*)push_array_no_alignment_nz(arena, Glyph_Vertex,
			                                                                num_verts);
			for (usize i = 0; i < num_verts; i += 1) {
				x += glyf_read_coordinate(x_coords, &x_coord_off, x_short_vec, x_same_or_positive) << 6;
				y += glyf_read_coordinate(y_coords, &y_coord_off, y_short_vec, y_same_or_positive) << 6;
				verts[i].on_curve = on_curve;
				verts[i].prev_offset_plus_one = 0;
				verts[i].x = x;
				verts[i].y = y;
			}
			outline->num_verts += num_verts;
		}
	}
}

function void unpack_compound_glyf_outline(
  Arena         *arena,
  Glyph_Outline *outline,
  TTF_Font      font,
  u16           glyph_index,
	u8            max_depth
) {
}

function Glyph_Outline font_unpack_glyph_outline(Arena *arena, TTF_Font font, u16 glyph_index) {
	// IMPORTANT(byron): only push vertices to the arena
	Glyph_Outline outline = {0};
	outline.num_verts = 0;
	outline.verts = (Glyph_Vertex*)push_array(arena, Glyph_Vertex, 0);

	U8_Slice glyph_data = get_glyph_data_range(font, glyph_index);

	i16 num_contours = 0;
	if (glyph_data.len >= 10) {
		num_contours = read_i16be_unchecked(glyph_data, 0);
		outline.x_min = (i32)read_i16be_unchecked(glyph_data, 2) << 6;
		outline.y_min = (i32)read_i16be_unchecked(glyph_data, 4) << 6;
		outline.x_max = (i32)read_i16be_unchecked(glyph_data, 6) << 6;
		outline.y_max = (i32)read_i16be_unchecked(glyph_data, 8) << 6;
	}

	if (num_contours < 0) { // compound glyph
		unpack_compound_glyf_outline(arena, &outline, font, glyph_index, 16);
	}
	else if (num_contours > 0) { // simple glyph
		unpack_simple_glyf_outline(arena, &outline, font, glyph_index);
	}
	return outline;
}
