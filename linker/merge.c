#include "merge.h"
#include "linker.h"
#include "objects.h"
#include "errors.h"
#include "list.h"
#include "log.h"
#include <stdint.h>
#include <stdlib.h>
#include <strings.h>

area_t *get_area_by_name(object_t *object, char *name) {
	int i;
	for (i = 0; i < object->areas->length; ++i) {
		area_t *a = object->areas->items[i];
		if (strcasecmp(a->name, name) == 0) {
			return a;
		}
	}
	return NULL;
}

void relocate_area(area_t *area, uint64_t address) {
	int i;
	for (i = 0; i < area->symbols->length; ++i) {
		symbol_t *sym = area->symbols->items[i];
		if (sym->type != SYMBOL_LABEL) {
			continue;
		}
		sym->defined_address += address;
		sym->value += address;
	}
	for (i = 0; i < area->late_immediates->length; ++i) {
		late_immediate_t *imm = area->late_immediates->items[i];
		imm->address += address;
		imm->instruction_address += address;
		imm->base_address += address;
	}
	for (i = 0; i < area->source_map->length; ++i) {
		source_map_t *map = area->source_map->items[i];
		int j;
		for (j = 0; j < map->entries->length; ++j) {
			source_map_entry_t *entry = map->entries->items[j];
			entry->address += address;
		}
	}
}

void merge_areas(object_t *merged, object_t *source) {
	int i;
	for (i = 0; i < source->areas->length; ++i) {
		area_t *source_area = source->areas->items[i];
		area_t *merged_area = get_area_by_name(merged, source_area->name);
		if (merged_area == NULL) {
			scas_log(L_DEBUG, "Creating new area for '%s'", source_area->name);
			merged_area = create_area(source_area->name);
			list_add(merged->areas, merged_area);
		}
		uint64_t new_address = merged_area->data_length;
		relocate_area(source_area, new_address);

		append_to_area(merged_area, source_area->data, source_area->data_length);
		list_cat(merged_area->symbols, source_area->symbols);
		list_cat(merged_area->late_immediates, source_area->late_immediates);
		list_cat(merged_area->metadata, source_area->metadata);
		list_cat(merged_area->source_map, source_area->source_map);
	}
}

void merge_objects(FILE *output, list_t *objects, linker_settings_t *settings) {
	scas_log(L_INFO, "Merging %d objects into one", objects->length);
	object_t *merged = create_object();
	int i;
	for (i = 0; i < objects->length; ++i) {
		object_t *o = objects->items[i];
		scas_log(L_DEBUG, "Merging object %d", i);
		merge_areas(merged, o);
	}
	scas_log(L_INFO, "Writing merged object to output");
	fwriteobj(output, merged);
}
