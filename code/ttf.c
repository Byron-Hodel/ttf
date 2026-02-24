typedef struct {
	u16 units_per_em;
	u16 index_to_loca_format;
	u32 cmap_subtable_size;
	u32 loca_table_size;
	u32 glyph_table_size;
	u8  *cmap_subtable;
	u8  *loca_table;
	u8  *glyf_table;
} Font;

typedef struct Glyph_Outline_Block {
	struct Glyph_Outline_Block *next;
} Glyph_Outline_Block;

function i16 read_i16(u8 *ptr) {
	return (i16)(ptr[0] << 8) | ptr[1];
}

function u16 read_u16(u8 *ptr) {
	return (u16)(ptr[0] << 8) | ptr[1];
}

//function i32 read_i32(u8 *ptr) {
//	return (i32)(ptr[0] << 24) | (i32)(ptr[1] << 16) |
//	       (i32)(ptr[2] << 8) | ptr[3];
//}

function u32 read_u32(u8 *ptr) {
	return (u32)(ptr[0] << 24) | (u32)(ptr[1] << 16) |
	       (u32)(ptr[2] << 8) | ptr[3];
}

function b32 range_is_valid(usize data_size, usize offset, usize size) {
	return data_size - offset >= size;
}

// TODO(byron): implement bounds checking of all data reads
function Font font_make(Arena *arena, u8 *data, usize data_size) {
	ASSERT(arena != 0);
	ASSERT(data != 0 && data_size > 0);

	u16 num_tables = 0;
	if (range_is_valid(data_size, 0, 12)) {
		num_tables = read_u16(data+4);
		if (!range_is_valid(data_size, 12, 16 * num_tables)) {
			num_tables = 0;
		}
	}

	u32 head_table_size = 0;
	u32 hhea_table_size = 0;
	u32 hmtx_table_size = 0;
	u32 loca_table_size = 0;
	u32 glyf_table_size = 0;
	u32 cmap_table_size = 0;
	u8 *head_table = 0;
	u8 *hhea_table = 0;
	u8 *hmtx_table = 0;
	u8 *loca_table = 0;
	u8 *glyf_table = 0;
	u8 *cmap_table = 0;
	for (usize i = 0; i < num_tables; i += 1) {
		u8 *dir_entry_ptr = data + 12 + i * 16;
		u32 tag = read_u32(dir_entry_ptr+0);
		u32 offset = read_u32(dir_entry_ptr+8);
		u32 len = read_u32(dir_entry_ptr+12);

		if (range_is_valid(data_size, offset, len)) {
			switch (tag) {
				case 0x68656164: head_table = data+offset, head_table_size = len; break;
				case 0x68686561: hhea_table = data+offset, hhea_table_size = len; break;
				case 0x686D7478: hmtx_table = data+offset, hmtx_table_size = len; break;
				case 0x6C6F6361: loca_table = data+offset, loca_table_size = len; break;
				case 0x676C7966: glyf_table = data+offset, glyf_table_size = len; break;
				case 0x636D6170: cmap_table = data+offset, cmap_table_size = len; break;
			}
		}
	}

	u16 units_per_em = 64;
	u16 index_to_loca_format = 0;
	if (head_table_size >= 54) {
		units_per_em = read_u16(head_table+18);
		//xmin = read_u16(head_table+36);
		//ymin = read_u16(head_table+38);
		//xmax = read_u16(head_table+40);
		//ymax = read_u16(head_table+42);
		index_to_loca_format = read_u16(head_table+50);
	}

	u8 *cmap_subtable = 0;
	u32 cmap_subtable_size = 0;
	if (cmap_table_size >= 4) {
		u16 num_subtables = read_u16(cmap_table+2);

		if (!range_is_valid(cmap_table_size, 4, 8 * num_subtables)) {
			num_subtables = 0;
		}

		u16 subtable_priority = 0;

		for (u16 i = 0; i < num_subtables; i += 1) {
			u8 *subtable_desc_base = cmap_table + 4 + 8 * i;
			u16 platform_id = read_u16(subtable_desc_base);
			u16 encoding_id = read_u16(subtable_desc_base+2);

			u32 priority = 0;
			switch (platform_id) {
				case 0: // Platform 0 — Unicode
					switch (encoding_id) {
						case 6:  priority = 13; break; // Unicode full repertoire
						case 4:  priority = 12; break; // Unicode 2.0 full
						case 3:  priority = 11; break; // Unicode 2.0 BMP
						case 2:  priority = 10; break; // ISO 10646
						case 1:  priority = 9;  break; // Unicode 1.1
						case 0:  priority = 8;  break; // Unicode 1.0
						case 5:  priority = 7;  break; // Variation sequences
						default: priority = 1;  break;
					}
					break;
				case 3: // Platform 3 — Windows
					switch (encoding_id) {
						case 10: priority = 14; break; // UCS-4 (BEST)
						case 1:  priority = 9;  break; // Unicode BMP
						case 0:  priority = 5;  break; // Symbol
						case 2:  priority = 2;  break; // ShiftJIS
						case 3:  priority = 2;  break; // PRC
						case 4:  priority = 2;  break; // Big5
						case 5:  priority = 2;  break; // Wansung
						default: priority = 1;  break;
					}
					break;
				default:
					priority = 0; break;
			}

			if (priority > subtable_priority) {
				u32 subtable_offset = read_u32(subtable_desc_base+4);
				if (range_is_valid(cmap_table_size, subtable_offset, 4)) {
					u8 *subtable = cmap_table + subtable_offset;
					u16 format = read_u16(subtable);
					if (format == 0 || format == 2 || format == 4 ||
					    format == 6 || format == 8 || format == 10 ||
					    format == 12 || format == 13 || format == 14) {
						priority = subtable_priority;
						cmap_subtable = subtable;
						cmap_subtable_size = read_u16(cmap_subtable+2);
					}
				}
			}
		}
	}

	Font font = {0};
	font.units_per_em = units_per_em;
	font.index_to_loca_format = index_to_loca_format;
	font.cmap_subtable_size = cmap_subtable_size;
	font.cmap_subtable = push_array_aligned_nz(arena, u8, cmap_subtable_size, 4);
	font.loca_table = push_array_aligned_nz(arena, u8, loca_table_size, 4);
	font.glyf_table = push_array_aligned_nz(arena, u8, glyf_table_size, 4);
	memcpy(font.cmap_subtable, cmap_subtable, cmap_subtable_size);
	memcpy(font.loca_table, loca_table, loca_table_size);
	memcpy(font.glyf_table, glyf_table, glyf_table_size);

	return font;
}

function u16 lookup_glyph_index(Font font, u32 codepoint) {
	u16 glyph_index = 0;
	if (font.cmap_subtable_size > 2) {
		u16 format = read_u16(font.cmap_subtable);
		switch (format) {
			case 4:
			{
				u16 num_segments = 0;
				if (range_is_valid(font.cmap_subtable_size, 6, 2)) {
					num_segments = read_u16(font.cmap_subtable+6) / 2;
				}
				u32 min_size = 8 * num_segments + 16;

				if (range_is_valid(font.cmap_subtable_size, 0, min_size)) {
					u8 *end_codes = font.cmap_subtable + 14;
					u8 *start_codes = end_codes + 2 * num_segments + 2;
					u8 *id_deltas = start_codes + 2 * num_segments;
					u8 *id_range_offsets = id_deltas + 2 * num_segments;

					u16 seg_index = 0;
					for (; seg_index < num_segments; seg_index += 1) {
						u16 endcode = read_u16(end_codes + 2 * seg_index);
						if (endcode >= codepoint) {	
							break;
						}
					}

					if (seg_index < num_segments) {
						u16 start_code = read_u16(start_codes + 2 * seg_index);
						u16 endcode = read_u16(end_codes + 2 * seg_index);
						u16 delta = read_u16(id_deltas + 2 * seg_index);
						u16 offset = read_u16(id_range_offsets + 2 * seg_index);
						if (offset == 0 && codepoint >= start_code) {
							glyph_index = (codepoint + delta) & 0xFFFF;
						}
						else if (codepoint >= start_code) {
							u32 index_offset = 2 * seg_index + offset + 2 * (codepoint - start_code);
							if (range_is_valid(font.cmap_subtable_size - min_size + 2 * seg_index, index_offset, 2)) {
								glyph_index = read_u16(id_range_offsets + index_offset);
							}
							if (glyph_index != 0) {
								glyph_index = (glyph_index + delta) & 0xFFFF;
							}
						}
					}
				}
			}
			break;
			default:
				debug_trap();
				break;
		}
	}
	return glyph_index;
}
