#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "parsers.h"
#include "parser-ragel.h"
#include "parsers-utils.h"
#include "utils.h"

%%{
	machine statsd;
	write data;
}%%

/**
 * Ragel parser entry point
 * Parsers given buffer and populates datagram with parsed data if they are valid
 * @arg str - Buffer to be parsed
 * @arg datagram - Placeholder for parsed data
 * @return 1 on success, 0 on fail 
 */
int ragel_parser_parse(char* str, statsd_datagram** datagram) {
	*datagram = (struct statsd_datagram*) malloc(sizeof(struct statsd_datagram));
	*(*datagram) = (struct statsd_datagram) {0};
	ALLOC_CHECK("Not enough memory to save datagram");
	char length = strlen(str);
	char *p = str, *pe = str + length;
	char *eof = NULL;
	int cs;
	int current_index = 0;
	int current_segment_start_index = 0;
	int instance_identifier_offset = strlen("instance=");
	tag_collection* tags;
	char* tag_key = NULL;
	char* tag_value = NULL;
	int tag_key_allocated = 0;
	int tag_value_allocated = 0;
	int any_tags = 0;

	%%{

		action error_handler {
			goto error_clean_up;
		}

		action increment_character_counter {
			current_index++;
		}

		action name_parsed {
			int current_segment_length = current_index - current_segment_start_index;
			(*datagram)->metric = (char*) malloc(current_segment_length + 1);
			ALLOC_CHECK("Not enough memory to save metric name");
			if (!sanitize_string(&str[current_segment_start_index], current_segment_length)) {
				goto error_clean_up;
			}
			memcpy(
				(*datagram)->metric,
				&str[current_segment_start_index],
				current_segment_length
			);
			(*datagram)->metric[current_segment_length] = '\0';
			current_segment_start_index = current_index + 1; 
		}

		action instance_parsed {
			int current_segment_length = current_index - current_segment_start_index;
			(*datagram)->instance = (char*) malloc(current_segment_length + 1);
			ALLOC_CHECK("Not enough memory to save instance name");
			if (!sanitize_string(
				&str[current_segment_start_index + instance_identifier_offset],
				current_segment_length - (instance_identifier_offset + 1))) {
				goto error_clean_up;
			}
			memcpy(
				(*datagram)->instance,
				&str[current_segment_start_index + instance_identifier_offset],
				current_segment_length - instance_identifier_offset
			);
			(*datagram)->instance[current_segment_length] = '\0';
			current_segment_start_index = current_index + 1; 
		}		

		action tag_parsed {
			int key_len = strlen(tag_key);
			int value_len = strlen(tag_value);
			tag* t = (tag*) malloc(sizeof(tag));
			ALLOC_CHECK("Unable to allocate memory for tag.");
			t->key = (char*) malloc(key_len);
			ALLOC_CHECK("Unable to allocate memory for tag key.");
			memcpy(t->key, tag_key, key_len);
			t->value = (char*) malloc(value_len);
			ALLOC_CHECK("Unable to allocate memory for tag value.");
			memcpy(t->value, tag_value, value_len);
			if (any_tags == 0) {
				tags = (tag_collection*) malloc(sizeof(tag_collection));
				ALLOC_CHECK("Unable to allocate memory for tag collection.");
				*tags = (tag_collection) {0};
				any_tags = 1;
			}
			tags->values = (tag**) realloc(tags->values, sizeof(tag*) * (tags->length + 1));
			tags->values[tags->length] = t;
			tags->length++;
			free(tag_key);
			free(tag_value);
			tag_key = NULL;
			tag_value = NULL;
			tag_key_allocated = 0;
			tag_value_allocated = 0;
		}

		action value_parsed {
			int current_segment_length = current_index - current_segment_start_index;
			(*datagram)->value = (char*) malloc(current_segment_length + 1);
			ALLOC_CHECK("Not enough memory to save metric value");
			memcpy(
				(*datagram)->value,
				&str[current_segment_start_index],
				current_segment_length
			);
			(*datagram)->value[current_segment_length] = '\0';
			current_segment_start_index = current_index + 1;
		}

		action type_parsed {
			int current_segment_length = current_index - current_segment_start_index;
			(*datagram)->type = (char*) malloc(current_segment_length + 1);
			ALLOC_CHECK("Not enough memory to save metric type");
			memcpy(
				(*datagram)->type,
				&str[current_segment_start_index],
				current_segment_length
			);
			(*datagram)->type[current_segment_length] = '\0';
			current_segment_start_index = current_index + 1;
		}

		action sampling_parsed {
			int current_segment_length = current_index - current_segment_start_index;
			(*datagram)->sampling = (char*) malloc(current_segment_length + 1);
			ALLOC_CHECK("Not enough memory to save metric sampling");
			memcpy(
				(*datagram)->sampling,
				&str[current_segment_start_index],
				current_segment_length
			);
			(*datagram)->sampling[current_segment_length] = '\0';
			current_segment_start_index = current_index + 1;
		}

		action tag_key_parsed {
			int current_segment_length = current_index - current_segment_start_index;
			tag_key = (char *) realloc(tag_key, current_segment_length + 1);
			ALLOC_CHECK("Not enough memory for tag key buffer.");
			tag_key_allocated = 1;
			if (!sanitize_string(&str[current_segment_start_index], current_segment_length)) {
				goto error_clean_up;
			}
			memcpy(
				tag_key,
				&str[current_segment_start_index], 
				current_segment_length
			);
			tag_key[current_segment_length] = '\0';
			current_segment_start_index = current_index + 1;
		}

		action tag_value_parsed {
			int current_segment_length = current_index - current_segment_start_index;
			tag_value = (char *) realloc(tag_value, current_segment_length + 1);
			ALLOC_CHECK("Not enough memory for tag key buffer.");
			tag_value_allocated = 1;
			if (!sanitize_string(&str[current_segment_start_index], current_segment_length)) {
				goto error_clean_up;
			}
			memcpy(
				tag_value,
				&str[current_segment_start_index], 
				current_segment_length
			);
			tag_value[current_segment_length] = '\0';
			current_segment_start_index = current_index + 1;
		}

		str_value = [a-zA-Z0-9_\-/. ]{1,20};
		name = str_value(':'|',');
		instance = 'instance='str_value(':'|',');
		tags = (((str_value -- 'instance')'=')@tag_key_parsed(str_value(':'|',')@tag_value_parsed))+;
		value = ('+'|'-')?[0-9]{1,12}([.][0-9]{1,12})?('|');
		type = ('c'|'g'|([m][s]))[\0\n@]{1};
		sampling = ([0-9]{1,12}([.][0-9]{1,12})?[\0\n]{1});

		main := 
			  (( name@name_parsed )
			  ( instance@instance_parsed )?
			  ( tags@tag_parsed )?
			  ( value@value_parsed )
			  ( type@type_parsed )
			  ( sampling@sampling_parsed ))$~increment_character_counter$!error_handler;

		# Initialize and execute.
		write init;
		write exec;

	}%%

	if (any_tags) {
		char* json = tag_collection_to_json(tags);
		if (json != NULL) {
			(*datagram)->tags = malloc(strlen(json) + 1);
			(*datagram)->tags = json;
		} else {
			goto error_clean_up;
		}
		free(tags);
	}
	if (str[length - 1] == '\n')
        str[length - 1] = 0;
	verbose_log("Parsed: %s", str);
	return 1;

	error_clean_up:
	if (tag_key_allocated) free(tag_key);
	if (tag_value_allocated) free(tag_value);
	if (str[length - 1] == '\n')
        str[length - 1] = 0;
	verbose_log("Throwing away datagram. REASON: unable to parse: %s", str);
	return 0;
};
