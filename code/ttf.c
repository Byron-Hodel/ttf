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
} Font;

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

typedef struct {
	b8  on_curve;
	i32 pos[2];
} Glyph_Point;

typedef struct {
	i32         min[2];
	i32         max[2];
	usize       num_contours;
	usize       num_points;
	u16         *contour_end_indices;
	Glyph_Point *points;
} Outline;

typedef struct {
	i32 num_points;
	i32 num_contours;
} Unpack_Result;

// I am not sure what the types for all of these are yet
typedef struct {
	b32 auto_flip;
	i32 ctrl_val_cut_in;
	i32 delta_base;
	i32 delta_shift;
	i32 dual_proj_vecs;
	i32 freedom_vec;
	i32 zp0;
	i32 zp1;
	i32 zp2;
	i32 instruct_ctrl;
	i32 loop;
	i32 min_dist;
	i32 proj_vec;
	i32 round_state;
	i32 rp0;
	i32 rp1;
	i32 rp2;
	b32 scan_ctrl;
	i32 single_width_cut_in;
	i32 single_width_value;
} Graphics_State;

global const i32 MAX_POINTS_IN_GLYPH = 65536;
global const i32 MAX_COMPOUND_GLYPH_DEPTH = 32;

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

function Font font_make(U8_Slice data) {
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

	Font font = {0};
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

function u16 font_glyph_index_from_codepoint(Font font, u32 codepoint) {
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

function U8_Slice get_glyph_data_range(Font font, u16 glyph_index) {
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

function void process_glyf_instructions(
	void
	//Glyph_Point *point_buffer,
	//usize       point_buffer_len,
	//U8_Slice    instructions
) {
}

function i16 get_glyf_num_contours(Font font, u16 glyph_index) {
	i16 num_contours = 0;
	U8_Slice glyph_data = get_glyph_data_range(font, glyph_index);
	if (glyph_data.len >= 10) {
		num_contours = read_i16be_unchecked(glyph_data, 0);
	}
	return num_contours;
}

function Unpack_Result unpack_simple_glyf(
	Font        font,
	u16         glyph_index,
	Glyph_Point *point_buffer,
	usize       point_buffer_len,
	u16         *contour_buffer,
	usize       contour_buffer_len,
	i32         funits_to_pixels_scale, // 16.16 fixed point
	b32         enable_hinting
) {
	U8_Slice glyph_data = get_glyph_data_range(font, glyph_index);

	b32 get_data_arrays = FALSE;
	i16 num_contours = 0;
	if (glyph_data.len >= 10) {
		num_contours = read_i16be_unchecked(glyph_data, 0);
		glyph_data = u8_subslice(glyph_data, 10, glyph_data.len - 10);
		get_data_arrays = num_contours >= 0 && glyph_data.len > 0;
		get_data_arrays &= contour_buffer_len >= (usize)num_contours;
	}

	// get data arrays
	b32 process_contours = FALSE;
	U8_Slice contour_end_indices = {0};
	U8_Slice instructions = {0};
	U8_Slice flags = {0};
	U8_Slice coords[2] = {0};
	u16 num_points_in_glyph = 0;
	if (get_data_arrays) {
		// read the number of points in the glyph
		b32 get_num_instructions = FALSE;
		num_points_in_glyph = 0;
		contour_end_indices = u8_subslice(glyph_data, 0, 2 * (usize)num_contours);
		if (contour_end_indices.len == 2 * (usize)num_contours) {
			usize last_off = 2 * (usize)num_contours - 2;
			num_points_in_glyph = read_u16be_unchecked(contour_end_indices, last_off) + 1;
			get_num_instructions = TRUE;
		}

		usize num_instructions_off = (usize)num_contours * 2;
		usize instruction_data_off = num_instructions_off + 2;

		// read number of instructions
		b32 get_max_flags_subslice = FALSE;
		u16 num_instructions = 0;
		if (get_num_instructions && glyph_data.len >= num_instructions_off + 2) {
			num_instructions = read_u16be_unchecked(glyph_data, num_instructions_off);
			get_max_flags_subslice = TRUE;
		}

		usize flags_off = instruction_data_off + num_instructions;

		// get maximum sized flags subslice
		b32 read_flags = FALSE;
		if (get_max_flags_subslice) {
			usize max_flags = glyph_data.len - flags_off;
			if (max_flags > num_points_in_glyph) {
				max_flags = num_points_in_glyph;
			}

			flags = u8_subslice(glyph_data, flags_off, max_flags);
			read_flags = flags.len > 0;
		}

		// read flags to determine flags size as well as x & y coord sizes
		b32 get_arrays = read_flags;
		usize num_flags = 0;
		usize coords_len[2] = {0};
		if (read_flags) {
			usize num_points_from_flags = 0;
			while (num_flags < flags.len) {
				u8 flag = flags.base[num_flags];
				num_flags += 1;

				b8 short_vec[2];
				b8 same_or_positive[2];
				short_vec[0]        = (flag & (1 << 1)) != 0;
				short_vec[1]        = (flag & (1 << 2)) != 0;
				b8 repeat           = (flag & (1 << 3)) != 0;
				same_or_positive[0] = (flag & (1 << 4)) != 0;
				same_or_positive[1] = (flag & (1 << 5)) != 0;

				u8 num_repeats = 0;
				if (repeat && num_flags < flags.len) {
					num_repeats = flags.base[num_flags];
					num_flags += 1;
				}
				else if (repeat) {
					get_arrays = FALSE;
					break;
				}
				num_points_from_flags += 1 + num_repeats;

				for (i32 i = 0; i < 2; i += 1) {
					if (short_vec[i]) {
						coords_len[i] += 1 + num_repeats;
					}
					else if (!same_or_positive[i]) {
						coords_len[i] += 2 * num_repeats + 2;
					}
				}

				if (num_points_from_flags >= num_points_in_glyph) {
					break;
				}
			}

			get_arrays &= num_points_from_flags == num_points_in_glyph;
		}

		// subslice data arrays
		if (get_arrays) {
			usize coords_off[2];
			coords_off[0] = flags_off + num_flags;
			coords_off[1] = coords_off[0] + coords_len[0];
			contour_end_indices = u8_subslice(glyph_data, 0, (usize)num_contours);
			instructions        = u8_subslice(glyph_data, instruction_data_off, num_instructions);
			flags               = u8_subslice(glyph_data, flags_off, num_flags);
			coords[0]           = u8_subslice(glyph_data, coords_off[0], coords_len[0]);
			coords[1]           = u8_subslice(glyph_data, coords_off[1], coords_len[1]);
			process_contours  = TRUE;
			process_contours &= contour_end_indices.len == (usize)num_contours;
			process_contours &= instructions.len == num_instructions;
			process_contours &= flags.len == num_flags;
			process_contours &= coords[0].len == coords_len[0];
			process_contours &= coords[1].len == coords_len[1];
			process_contours &= point_buffer_len >= (usize)num_points_in_glyph;
		}
	}

	// process the glyph's contour end points
	b32 process_points = FALSE;
	if (process_contours) {
		b32 indices_valid = TRUE;
		u16 prev_index = 0;
		for (u16 i = 0; i < num_contours; i += 1) {
			u16 index = read_u16be_unchecked(contour_end_indices, 2 * i);
			contour_buffer[i] = index;
			if (index < prev_index) {
				indices_valid = FALSE;
				break;
			}
		}
		process_points = indices_valid;
	}

	Unpack_Result result = {0};
	if (process_points) {
		// unpack points
		{
			usize coord_read_off[2] = {0};
			i32 point_pos[2] = {0};
			usize point_buffer_index = 0;
			for (usize flag_index = 0; flag_index < flags.len; flag_index += 1) {
				u8 flag = flags.base[flag_index];

				b8 short_vec[2];
				b8 same_or_positive[2];
				b8 on_curve         = (flag & (1 << 0)) != 0;
				short_vec[0]        = (flag & (1 << 1)) != 0;
				short_vec[1]        = (flag & (1 << 2)) != 0;
				b8 repeat           = (flag & (1 << 3)) != 0;
				same_or_positive[0] = (flag & (1 << 4)) != 0;
				same_or_positive[1] = (flag & (1 << 5)) != 0;

				u8 repeat_count = 0;
				if (repeat) {
					flag_index += 1;
					repeat_count = flags.base[flag_index];
				}

				for (u32 repeat_index = 0; repeat_index < (u32)repeat_count + 1; repeat_index += 1) {
					// update point position
					for (usize i = 0; i < 2; i += 1) {
						i32 delta = 0;
						if (short_vec[i]) {
							delta = (i32)coords[i].base[coord_read_off[i]];
							if (!same_or_positive[i]) {
								delta = -delta;
							}
							coord_read_off[i] += 1;
						}
						else if (!same_or_positive[i]) {
							delta = read_i16be_unchecked(coords[i], coord_read_off[i]);
							coord_read_off[i] += 2;
						}
						point_pos[i] += delta;
					}

					// append point to buffer
					point_buffer[point_buffer_index].on_curve = on_curve;
					point_buffer[point_buffer_index].pos[0] = point_pos[0];
					point_buffer[point_buffer_index].pos[1] = point_pos[1];
					point_buffer_index += 1;
				}
			}
		}

		// TODO(byron): I think variation data is processed here (gvar table stuff)

		// convert points to 16.16 and scale points
		for (u16 i = 0; i < num_points_in_glyph; i += 1) {
			point_buffer[i].pos[0] *= funits_to_pixels_scale;
			point_buffer[i].pos[1] *= funits_to_pixels_scale;
			//point_buffer[i].pos[0] <<= 16;
			//point_buffer[i].pos[1] <<= 16;
		}

		// TODO(byron): Do not forget about phantom points read
		// from the hmtx and vmtx tables

		// apply hinting
		if (enable_hinting) {
			// TODO
		}

		result.num_points = num_points_in_glyph;
		result.num_contours = num_contours;
	}
	return result;
}

function Unpack_Result unpack_composite_glyf(
	Font        font,
	u16         glyph_index,
	Glyph_Point *point_buffer,
	usize       point_buffer_len,
	u16         *contour_buffer,
	usize       contour_buffer_len,
	i32         funits_to_pixels_scale, // 16.16 fixed point
	u8          max_depth,
	b32         enable_hinting
) {
	U8_Slice glyph_data = get_glyph_data_range(font, glyph_index);

	b32 process_components = FALSE;
	i32 num_contours = 0;
	if (glyph_data.len >= 10) {
		num_contours = read_i16be_unchecked(glyph_data, 0);
		glyph_data = u8_subslice(glyph_data, 10, glyph_data.len - 10);
		process_components = num_contours < 0 && glyph_data.len > 0 && max_depth > 0;
	}

	Unpack_Result unpack_result = {0};
	U8_Slice instructions = {0};
	if (process_components) {
		b32 has_instructions = FALSE;
		usize component_off = 0;
		while (glyph_data.len >= 4) {
			u16 component_flags = read_u16be_unchecked(glyph_data, component_off + 0);
			u16 component_index = read_u16be_unchecked(glyph_data, component_off + 2);

			b8 arg_1_and_2_are_words    = (component_flags & (1 << 0)) != 0;
			b8 args_are_xy_values       = (component_flags & (1 << 1)) != 0;
			b8 round_to_xy_grid         = (component_flags & (1 << 2)) != 0;
			b8 we_have_a_scale          = (component_flags & (1 << 3)) != 0;
			b8 we_have_an_x_and_y_scale = (component_flags & (1 << 6)) != 0;
			b8 we_have_a_two_by_two     = (component_flags & (1 << 7)) != 0;
			b8 we_have_instructions     = (component_flags & (1 << 8)) != 0;
			b8 use_my_metrics           = (component_flags & (1 << 9)) != 0;
			b8 more_components          = (component_flags & (1 << 5)) != 0;
			has_instructions |= we_have_instructions;

			usize args_size = arg_1_and_2_are_words ? 4 : 2;
			usize transform_data_off = component_off + 4 + args_size;

			U8_Slice args_data = u8_subslice(glyph_data, component_off + 4, args_size);

			// read offset arguments
			// NOTE(byron): not converted to fixed point when read in

			b32 offset_via_point_alignment = FALSE;
			i32 args[2] = {0};
			if (args_data.len == args_size) {
				if (arg_1_and_2_are_words && args_are_xy_values) {
					// values are signed
					args[0] = (i32)read_i16be_unchecked(args_data, 0);
					args[1] = (i32)read_i16be_unchecked(args_data, 2);
				}
				else if (!arg_1_and_2_are_words && args_are_xy_values) {
					// values are signed
					args[0] = (i32)(i8)args_data.base[0];
					args[1] = (i32)(i8)args_data.base[1];
				}
				else if (arg_1_and_2_are_words && !args_are_xy_values) {
					// values are unsigned
					args[0] = (i32)read_u16be_unchecked(args_data, 0);
					args[1] = (i32)read_u16be_unchecked(args_data, 2);
				}
				else if (!arg_1_and_2_are_words && !args_are_xy_values) {
					// values are unsigned
					args[0] = (i32)args_data.base[0];
					args[1] = (i32)args_data.base[1];
				}
			}

			// get transform data
			usize transform_data_size = 0;
			i32 xscale = 1 << 16;
			i32 scale01 = 0;
			i32 scale10 = 0;
			i32 yscale = 1 << 16;
			if (we_have_a_scale) {
				transform_data_size = 2;
				U8_Slice transform_data = u8_subslice(glyph_data, transform_data_off, 2);
				if (transform_data.len == 2) {
					xscale = (i32)read_i16be_unchecked(transform_data, 0) << 2;
					yscale = xscale;
				}
			}
			else if (we_have_an_x_and_y_scale) {
				transform_data_size = 4;
				U8_Slice transform_data = u8_subslice(glyph_data, transform_data_off, 4);
				if (transform_data.len == 4) {
					xscale = (i32)read_i16be_unchecked(transform_data, 0) << 2;
					yscale = (i32)read_i16be_unchecked(transform_data, 2) << 2;
				}
			}
			else if (we_have_a_two_by_two) {
				transform_data_size = 8;
				U8_Slice transform_data = u8_subslice(glyph_data, transform_data_off, 8);
				if (transform_data.len == 8) {
					xscale = (i32)read_i16be_unchecked(transform_data, 0) << 2;
					scale01 = (i32)read_i16be_unchecked(transform_data, 2) << 2;
					scale10 = (i32)read_i16be_unchecked(transform_data, 4) << 2;
					yscale = (i32)read_i16be_unchecked(transform_data, 6) << 2;
				}
			}

			// unpack and process component
			U8_Slice component_data = get_glyph_data_range(font, component_index);
			if (component_data.len >= 10) {
				i16 num_contours = read_i16be_unchecked(component_data, 0);

				Glyph_Point *component_point_buf = point_buffer + unpack_result.num_points;
				usize component_point_buf_len  = point_buffer_len - (usize)unpack_result.num_points;
				ASSERT(point_buffer_len > (usize)unpack_result.num_points);
				u16 *component_contour_buf = contour_buffer + unpack_result.num_contours;
				usize component_contour_buf_len = contour_buffer_len - (usize)unpack_result.num_contours;

				Unpack_Result component_unpack_result = {0};
				if (num_contours < 0) {
					component_unpack_result = unpack_composite_glyf(font, component_index,
					                                                component_point_buf,
					                                                component_point_buf_len,
					                                                component_contour_buf,
					                                                component_contour_buf_len,
					                                                funits_to_pixels_scale,
					                                                max_depth - 1, enable_hinting);
				}
				else {
					component_unpack_result = unpack_simple_glyf(font, component_index,
					                                             component_point_buf,
					                                             component_point_buf_len,
					                                             component_contour_buf,
					                                             component_contour_buf_len,
					                                             funits_to_pixels_scale, enable_hinting);
				}

				i32 offset[2];
				offset[0] = args[0] * funits_to_pixels_scale;
				offset[1] = args[1] * funits_to_pixels_scale;
				if (offset_via_point_alignment) {
					b32 parent_index_valid = args[0] < unpack_result.num_points;
					b32 child_index_valid = args[1] < component_unpack_result.num_points;
					if (parent_index_valid && child_index_valid) {
						Glyph_Point parent_point = point_buffer[args[0]];
						Glyph_Point child_point = component_point_buf[args[1]];
						offset[0] = parent_point.pos[0] - child_point.pos[0];
						offset[1] = parent_point.pos[1] - child_point.pos[1];
						offset[0] <<= 16;
						offset[1] <<= 16;
					}
				}

				// apply transform to component
				for (i32 i = 0; i < component_unpack_result.num_points; i += 1) {
					i32 x = component_point_buf[i].pos[0] >> 16;
					i32 y = component_point_buf[i].pos[1] >> 16;
					component_point_buf[i].pos[0] = xscale * x + scale10 * y + offset[0];
					component_point_buf[i].pos[1] = scale01 * x + yscale * y + offset[1];
				}

				// renumber component contour indices
				for (i32 i = 0; i < component_unpack_result.num_contours; i += 1) {
					component_contour_buf[i] += unpack_result.num_contours;
				}

				// update current unpack result
				unpack_result.num_points += component_unpack_result.num_points;
				unpack_result.num_contours += component_unpack_result.num_contours;
			}	

			component_off = transform_data_off + transform_data_size;

			if (!more_components) {
				break;
			}
		}
	}

	return unpack_result;
}

function Outline font_unpack_glyph(
	Arena *arena,
	Font  font,
	u16   glyph_index,
	i32   funits_to_pixels_scale, // 16.16 fixed point
	b32   enable_hinting
) {
	U8_Slice glyph_data = get_glyph_data_range(font, glyph_index);

	Outline outline = {0};
	i32 num_contours = 0;
	if (glyph_data.len >= 10) {
		num_contours = read_i16be_unchecked(glyph_data, 0);
		outline.min[0] = (i32)read_i16be_unchecked(glyph_data, 2) << 6;
		outline.min[1] = (i32)read_i16be_unchecked(glyph_data, 4) << 6;
		outline.max[0] = (i32)read_i16be_unchecked(glyph_data, 6) << 6;
		outline.max[1] = (i32)read_i16be_unchecked(glyph_data, 8) << 6;
	}

	Arena_Temp scratch = begin_scratch(&arena, 1);
	Glyph_Point *point_buffer = push_array_nz(scratch.arena, Glyph_Point, MAX_POINTS_IN_GLYPH);
	u16         *contour_buffer = push_array_nz(scratch.arena, u16, MAX_POINTS_IN_GLYPH);

	i32 num_points = 0;
	if (num_contours < 0) { // composite glyph
		Unpack_Result r = unpack_composite_glyf(font, glyph_index, point_buffer, MAX_POINTS_IN_GLYPH,
		                                       contour_buffer, MAX_POINTS_IN_GLYPH,
		                                       funits_to_pixels_scale, MAX_COMPOUND_GLYPH_DEPTH,
		                                       enable_hinting);
		num_points = r.num_points;
		num_contours = r.num_contours;
	}
	else if (num_contours > 0) { // simple glyph
		Unpack_Result r = unpack_simple_glyf(font, glyph_index, point_buffer, MAX_POINTS_IN_GLYPH,
		                                     contour_buffer, MAX_POINTS_IN_GLYPH, 
																				 funits_to_pixels_scale, enable_hinting);
		num_points = r.num_points;
		num_contours = r.num_contours;
	}

	if (num_points > 0 && num_contours > 0) {
		outline.num_points = (usize)num_points;
		outline.num_contours = (usize)num_contours;
		outline.contour_end_indices = push_array_nz(arena, u16, (usize)num_contours);
		outline.points = push_array_nz(arena, Glyph_Point, (usize)num_points);

		for (i32 i = 0; i < num_points; i += 1) {
			outline.points[i].on_curve = point_buffer[i].on_curve;
			outline.points[i].pos[0] = point_buffer[i].pos[0] >> 10;
			outline.points[i].pos[1] = point_buffer[i].pos[1] >> 10;
		}

		memcpy(outline.contour_end_indices , contour_buffer, outline.num_contours);
	}

	end_scratch(scratch);
	return outline;
}
