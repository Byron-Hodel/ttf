typedef struct {
	u8  *base;
	u32 size;
} TTF_Data_Range;

typedef struct {
	u16            units_per_em;
	u16            index_to_loca_format;
	TTF_Data_Range cmap_subtable;
	TTF_Data_Range loca_table;
	TTF_Data_Range glyf_table;
} Font;

function b32 ttf_is_subrange(TTF_Data_Range range, TTF_Data_Range subrange) {
	u64 subrange_offset = (u64)((usize)subrange.base - (usize)range.base);
	b32 subrange_is_nil = subrange.base == 0 && subrange.size == 0;
	b32 range_is_below = range.base <= subrange.base;
	b32 subrange_in_bounds = subrange_offset + (u64)subrange.size <= range.size;
	return subrange_is_nil || (range_is_below && subrange_in_bounds);
}

function TTF_Data_Range ttf_slice_range(TTF_Data_Range range, u32 offset, u32 size) {
	TTF_Data_Range subrange = {0};
	u64 subrange_end = (u64)offset + (u64)size;
	if (subrange_end <= range.size) {
		subrange.base = range.base + offset;
		subrange.size = size;
	}
	ASSERT(ttf_is_subrange(range, subrange));
	return subrange;
}
#define ttf_slice_remaining(range, offset) ttf_slice_range(range, offset, range.size - offset)

function i16 ttf_read_i16(TTF_Data_Range range, u32 offset) {
	ASSERT(range.size > offset + 1);
	u8 *ptr = range.base + offset;
	return (i16)((i16)ptr[0] << 8 | ptr[1]);
}
#define ttf_read_i16_array(range, index) ttf_read_i16(range, sizeof(u16)*(index))

function u16 ttf_read_u16(TTF_Data_Range range, u32 offset) {
	ASSERT(range.size > offset + 1);
	u8 *ptr = range.base + offset;
	return (u16)(ptr[0] << 8) | ptr[1];
}
#define ttf_read_u16_array(range, index) ttf_read_u16(range, sizeof(u16)*(index))

function u32 ttf_read_u32(TTF_Data_Range range, u32 offset) {
	ASSERT(range.size > offset + 3);
	u8 *ptr = range.base + offset;
	return (u32)ptr[0] << 24 | (u32)ptr[1] << 16 | (u32)ptr[2] << 8 | ptr[3];
}
#define ttf_read_u32_array(range, index) ttf_read_u32(range, sizeof(u32)*(index))

function Font font_make(Arena *arena, u8 *data, u32 data_size) {
	ASSERT(arena != 0);
	ASSERT(data_size > 0 || data == 0); // data_size > 0 -> data != 0

	b32 font_invalid = FALSE;
	TTF_Data_Range font_range;
	font_range.base = data;
	font_range.size = data_size;

	TTF_Data_Range offset_subtable = ttf_slice_range(font_range, 0, 12);
	TTF_Data_Range table_directory = {0};
	if (offset_subtable.size >= 12) {
		u16 _num_tables = ttf_read_u16(offset_subtable, 4);
 		table_directory = ttf_slice_range(font_range, 12, 16 * _num_tables);
	}

	TTF_Data_Range head_table = {0};
	TTF_Data_Range hhea_table = {0};
	TTF_Data_Range hmtx_table = {0};
	TTF_Data_Range cmap_table = {0};
	TTF_Data_Range loca_table = {0};
	TTF_Data_Range glyf_table = {0};
	for (u16 entry_offset = 0; entry_offset < table_directory.size; entry_offset += 16) {
		u32 tag    = ttf_read_u32(table_directory, entry_offset+0);
		u32 offset = ttf_read_u32(table_directory, entry_offset+8);
		u32 len    = ttf_read_u32(table_directory, entry_offset+12);

		TTF_Data_Range range = ttf_slice_range(font_range, offset, len);

		switch (tag) {
		case 0x68656164: head_table = range; break;
		case 0x68686561: hhea_table = range; break;
		case 0x686D7478: hmtx_table = range; break;
		case 0x636D6170: cmap_table = range; break;
		case 0x6C6F6361: loca_table = range; break;
		case 0x676C7966: glyf_table = range; break;
		}
	}

	// font is invalid if it is missing a required font table
	font_invalid = head_table.size == 0 || hhea_table.size == 0 || hmtx_table.size == 0 ||
	               cmap_table.size == 0 || loca_table.size == 0 || glyf_table.size == 0;

	u16 units_per_em = 0;
	u16 index_to_loca_format = 0;
	if (head_table.size >= 54) {
		u16 _units_per_em         = ttf_read_u16(head_table, 18);
		u16 _index_to_loca_format = ttf_read_u16(head_table, 50);

		if (_units_per_em < 64) {
			_units_per_em = 64;
		}
		else if (_units_per_em > 16384) {
			_units_per_em = 16384;
		}
		units_per_em = _units_per_em;
		index_to_loca_format = (_index_to_loca_format == 0) ? 0 : 1;
	}
	else {
		font_invalid = TRUE;
	}

	TTF_Data_Range cmap_subtable_directory = {0};
	if (cmap_table.size >= 4) {
		u16 _num_subtables = ttf_read_u16(cmap_table, 2);
		cmap_subtable_directory = ttf_slice_range(cmap_table, 4, 8 * _num_subtables);
	}

	TTF_Data_Range cmap_subtable = {0};
	u8 cmap_subtable_priority = 0;
	for (u32 entry_offset = 0; entry_offset < cmap_subtable_directory.size; entry_offset += 8) {
		u16 _platform_id = ttf_read_u16(cmap_subtable_directory, entry_offset+0);
		u16 _encoding_id = ttf_read_u16(cmap_subtable_directory, entry_offset+2);
		u32 _offset      = ttf_read_u32(cmap_subtable_directory, entry_offset+4);

		u16 _subtable_size = 0;
		TTF_Data_Range range = ttf_slice_range(cmap_table, _offset, 4);
		if (range.size >= 4) {
			_subtable_size = ttf_read_u16(range, 2);
		}
		range = ttf_slice_range(cmap_table, _offset, _subtable_size);

		u8 subtable_priority = 0;
		switch (_platform_id) {
		case 0:
			switch (_encoding_id) {
			case 6: subtable_priority = 13; break; // Unicode full repertoire
			case 4: subtable_priority = 12; break; // Unicode 2.0 full
			case 3: subtable_priority = 11; break; // Unicode 2.0 BMP
			case 2: subtable_priority = 10; break; // ISO 10646
			case 1: subtable_priority = 9;  break; // Unicode 1.1
			case 0: subtable_priority = 8;  break; // Unicode 1.0
			case 5: subtable_priority = 7;  break; // Variation sequences
			}
			break;
		case 3:
			switch (_encoding_id) {
			case 10: subtable_priority = 14; break; // UCS-4 (BEST)
			case 1:  subtable_priority = 9;  break; // Unicode BMP
			case 0:  subtable_priority = 5;  break; // Symbol
			case 2:  subtable_priority = 2;  break; // ShiftJIS
			case 3:  subtable_priority = 2;  break; // PRC
			case 4:  subtable_priority = 2;  break; // Big5
			case 5:  subtable_priority = 2;  break; // Wansung
			}
			break;
		}

		if (subtable_priority > cmap_subtable_priority && range.size > 4) {
			cmap_subtable = range;
			cmap_subtable_priority = subtable_priority;
		}
	}
	if (cmap_subtable.size == 0) {
		font_invalid = TRUE;
	}

	Font font = {0};
	if (!font_invalid) {
		font.units_per_em = units_per_em;
		font.index_to_loca_format = index_to_loca_format;

		font.cmap_subtable.base = push_array_aligned_nz(arena, u8, cmap_subtable.size, 4);
		font.cmap_subtable.size = cmap_subtable.size;
		memcpy(font.cmap_subtable.base, cmap_subtable.base, cmap_subtable.size);

		font.loca_table.base = push_array_aligned_nz(arena, u8, loca_table.size, 4);
		font.loca_table.size = loca_table.size;
		memcpy(font.loca_table.base, loca_table.base, loca_table.size);

		font.glyf_table.base = push_array_aligned_nz(arena, u8, glyf_table.size, 4);
		font.glyf_table.size = glyf_table.size;
		memcpy(font.glyf_table.base, glyf_table.base, glyf_table.size);
	}
	return font;
}

function u16 font_lookup_glyph_index(Font font, u32 codepoint) {
	u16 glyph_index = 0;
	TTF_Data_Range cmap_subtable = font.cmap_subtable;

	i32 _format = -1;
	if (cmap_subtable.size > 4) {
		_format = ttf_read_u16(cmap_subtable, 0);
	}

	switch (_format) {
	case 4: {
		u16 num_segments = 0;
		u32 offset_of_id_range_offsets = 0;
		TTF_Data_Range end_codes = {0};
		TTF_Data_Range start_codes = {0};
		TTF_Data_Range id_deltas = {0};
		TTF_Data_Range id_range_offsets = {0};
		if (cmap_subtable.size >= 16) {
			u16 _num_segments = ttf_read_u16(cmap_subtable, 6) / 2;
			end_codes        = ttf_slice_range(cmap_subtable, 14 + 2 * _num_segments * 0, 2 * _num_segments);
			start_codes      = ttf_slice_range(cmap_subtable, 16 + 2 * _num_segments * 1, 2 * _num_segments);
			id_deltas        = ttf_slice_range(cmap_subtable, 16 + 2 * _num_segments * 2, 2 * _num_segments);
			id_range_offsets = ttf_slice_range(cmap_subtable, 16 + 2 * _num_segments * 3, 2 * _num_segments);
			offset_of_id_range_offsets = 16 + 2 * _num_segments * 3;
			num_segments = (id_range_offsets.size > 0) ? _num_segments : 0;
		}

		u16 segment_index = 0;
		for (; segment_index < num_segments; segment_index += 1) {
			u16 _endcode = ttf_read_u16_array(end_codes, segment_index);
			if (_endcode >= codepoint) {
				break;
			}
		}

		if (segment_index < num_segments) {
			u16 _startcode    = ttf_read_u16_array(start_codes,      segment_index);
			u16 _range_offset = ttf_read_u16_array(id_range_offsets, segment_index);
			u16 _delta        = ttf_read_u16_array(id_deltas,        segment_index);
			if (_range_offset == 0 && _startcode <= codepoint) {
				glyph_index = (codepoint + _delta) & 0xFFFF;
			}
			else if (_startcode <= codepoint) {
				u32 glyph_index_offset = offset_of_id_range_offsets + 2 * segment_index +
					_range_offset + 2 * (codepoint - _startcode);
				if (glyph_index_offset + 1 < cmap_subtable.size) {
					glyph_index = ttf_read_u16(cmap_subtable, glyph_index_offset);
				}
				if (glyph_index != 0) {
					glyph_index = (glyph_index + _delta) & 0xFFFF;
				}
			}
		}
	} break;
	}

	return glyph_index;
}

void font_unpack_glyph(Arena *arena, Font font, u16 glyph_index) {
	TTF_Data_Range glyf_table = font.glyf_table;
	TTF_Data_Range glyph_data = {0};
	i16 _num_contours = 0;
	if (glyf_table.size >= 10) {
		_num_contours = ttf_read_i16(glyf_table, 0);
		glyph_data = ttf_slice_range(glyf_table, 10, glyph_data.size - 10);
	}

	if (_num_contours < 0) { // compound glyph

	}
	else if (_num_contours > 0) { // simple glyph
		TTF_Data_Range end_points_of_contours = {0};
		TTF_Data_Range instructions = {0};
		if (glyph_data.size >= (u32)_num_contours * 2 + 2) {
			end_points_of_contours = ttf_slice_range(glyph_data, 0, (u32)_num_contours * 2);
			u16 _num_instructions = ttf_read_u16(glyph_data, (u32)_num_contours * 2);
			instructions = ttf_slice_range(glyph_data, (u32)_num_contours * 2 + 2, _num_instructions);
		}
		
		u16 _num_points = 0;
		if (end_points_of_contours.size >= (u32)_num_contours * 2) {
			_num_points = ttf_read_u16_array(end_points_of_contours, (u32)_num_contours - 1);
		}

		u32 flags_offset = (u32)_num_contours * 2 + 2 + instructions.size;
		TTF_Data_Range flags = ttf_slice_remaining(glyph_data, flags_offset);

		u16 flags_size = 0;
		u32 x_coords_size = 0;
		u32 y_coords_size = 0;
		TTF_Data_Range x_coords = {0};
		TTF_Data_Range y_coords = {0};
		for (u16 flag_index = 0 ; flag_index < flags.size; flag_index += 1) {
			u8 flag = *(flags.base + flag_index);
		}
	}
}
