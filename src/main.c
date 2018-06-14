/*
 * main.c
 *
 *  Created on: Jun 9, 2018
 *      Author: pchero
 */

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "slog.h"
#include "db_ctx_handler.h"
#include "utils.h"
#include "fp_handler.h"

static void search_fingerprint(const char* filename);

int main(int argc, char** argv)
{
	int ret;
	char opt;

	ret = fp_init();
	if(ret == false) {
		fprintf(stderr, "Could not initate.\n");
		return 1;
	}

	if(argc < 2) {
		fprintf(stdout, "Usage: \n");
		fprintf(stdout, "%s -a <fingerprint filename> : Add the audio fingerprint info.\n", argv[0]);
		fprintf(stdout, "%s -d <fingerprint filename> : Delete the audio fingerprint info.\n", argv[0]);
		fprintf(stdout, "%s -s <search filename> : Search audio fingerprint info.\n", argv[0]);
		return 0;
	}

	opt = getopt(argc, argv, "ads");
	if(opt == -1) {
		return 1;
	}

	switch(opt)
	{
		case 'a': {
			fp_craete_fingerprint_info(argv[2]);
		}
		break;

		case 'd': {
			fp_delete_fingerprint_info(argv[2]);
		}
		break;

		case 's': {
			search_fingerprint(argv[2]);
		}
		break;


		default: {
//			return 0;
		}
		break;

	}

	fp_term();


	return 0;
}

static void search_fingerprint(const char* filename)
{
	json_t* j_res;

	if(filename == NULL) {
		fprintf(stdout, "Wrong input parameter.\n");
		return;
	}

	j_res = fp_search_fingerprint_info(filename, 1);
	if(j_res == NULL) {
		fprintf(stdout, "Could not find fingerprint info. filename[%s]\n", filename);
		return;
	}

	char* tmp;
	tmp = json_dumps(j_res, JSON_ENCODE_ANY);
	fprintf(stdout, "%s\n", tmp);
	sfree(tmp);

	fprintf(stdout, "Found data.\n");
	fprintf(stdout, "Audio name:  %s\n", json_string_value(json_object_get(j_res, "name")));
	fprintf(stdout, "Audio uuid:  %s\n", json_string_value(json_object_get(j_res, "uuid")));
	fprintf(stdout, "Audio hash:  %s\n", json_string_value(json_object_get(j_res, "hash")));
	fprintf(stdout, "Frame count: %lld\n", json_integer_value(json_object_get(j_res, "frame_count")));
	fprintf(stdout, "Match count: %lld\n", json_integer_value(json_object_get(j_res, "match_count")));
	json_decref(j_res);

	return;
}
