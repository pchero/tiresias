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

static void create_fingerprint(const char* filename);
static void delete_fingerprint(const char* filename);
static void search_fingerprint(const char* filename);
static void show_fingerprints_all(void);

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
		fprintf(stdout, "%s -d <audio uuid> : Delete the audio fingerprint info.\n", argv[0]);
		fprintf(stdout, "%s -s <search filename> : Search audio fingerprint info.\n", argv[0]);
		fprintf(stdout, "%s -l : Show all list of fingerprinted lists.\n", argv[0]);
		return 0;
	}

	opt = getopt(argc, argv, "adsl");
	if(opt == -1) {
		return 1;
	}

	switch(opt)
	{
		case 'a': {
			create_fingerprint(argv[2]);
		}
		break;

		case 'd': {
			delete_fingerprint(argv[2]);
		}
		break;

		case 's': {
			search_fingerprint(argv[2]);
		}
		break;

		case 'l': {
			show_fingerprints_all();
		}
		break;

		default: {
			fprintf(stderr, "Unsupported option.\n");
		}
		break;

	}
	fp_term();

	return 0;
}

static void create_fingerprint(const char* filename)
{
	if(filename == NULL) {
		fprintf(stderr, "Wrong input filename.\n");
		return;
	}

	fp_craete_fingerprint_info(filename);
	return;
}

static void delete_fingerprint(const char* filename)
{
	if(filename == NULL) {
		fprintf(stderr, "Wrong input filename.\n");
		return;
	}

	fp_delete_fingerprint_info(filename);
	return;
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

	fprintf(stdout, "Audio name:  %s\n", json_string_value(json_object_get(j_res, "name")));
	fprintf(stdout, "Audio uuid:  %s\n", json_string_value(json_object_get(j_res, "uuid")));
	fprintf(stdout, "Audio hash:  %s\n", json_string_value(json_object_get(j_res, "hash")));
	fprintf(stdout, "Frame count: %lld\n", json_integer_value(json_object_get(j_res, "frame_count")));
	fprintf(stdout, "Match count: %lld\n", json_integer_value(json_object_get(j_res, "match_count")));
	json_decref(j_res);

	return;
}

static void show_fingerprints_all(void)
{
	json_t* j_res;
	json_t* j_tmp;
	int idx;

	j_res = fp_get_fingerprint_lists_all();
	if(j_res == NULL) {
		fprintf(stderr, "Could not get fingerprint list info.");
	}

	json_array_foreach(j_res, idx, j_tmp) {

		fprintf(stdout, "Fingerprint info. idx[%d]\n", idx);
		fprintf(stdout, "Audio name: %s\n", json_string_value(json_object_get(j_tmp, "name")));
		fprintf(stdout, "Audio uuid: %s\n", json_string_value(json_object_get(j_tmp, "uuid")));
		fprintf(stdout, "Audio hash: %s\n", json_string_value(json_object_get(j_tmp, "hash")));
		fprintf(stdout, "\n");
	}
	json_decref(j_res);

	return;
}
