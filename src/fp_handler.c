/*
 * fp_handler.c
 *
 *  Created on: Jun 10, 2018
 *      Author: pchero
 */

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <aubio/aubio.h>
#include <math.h>
#include <jansson.h>
#include <openssl/md5.h>

#include "slog.h"
#include "db_ctx_handler.h"
#include "fp_handler.h"
#include "utils.h"

db_ctx_t* g_db_ctx;

#define DEF_SEARCH_RANGE		0.001

#define DEF_DATABASE_NAME			":memory:"
#define DEF_BACKUP_DATABASE			"audio_recongition.db"
#define DEF_WINDOW_SIZE_CONSTANCE	100

#define DEF_AUBIO_HOPSIZE		256
#define DEF_AUBIO_BUFSIZE		512
#define DEF_AUBIO_SAMPLERATE	0		// read samplerate from the file
#define DEF_AUBIO_FILTER		40
#define DEF_AUBIO_COEFS			13

static bool init_database(void);

static bool create_audio_list_info(const char* filename, const char* uuid);
static bool create_audio_fingerprint_info(const char* filename, const char* uuid);
static json_t* create_audio_fingerprints(const char* filename, const char* uuid);

static json_t* get_audio_list_info(const char* uuid);
static json_t* get_audio_list_info_by_hash(const char* hash);
static char* create_file_hash(const char* filename);

static bool create_temp_search_table(const char* tablename);
static bool delete_temp_search_table(const char* tablename);

bool fp_init(void)
{
	int ret;

	ret = init_database();
	if(ret == false) {
		slog(LOG_ERR, "Could not initiate database.");
		return false;
	}

	// load the data into memory
	ret = db_ctx_load_db_data(g_db_ctx, DEF_BACKUP_DATABASE);
	if(ret == false) {
		slog(LOG_ERR, "Could not load the database data.");
		return false;
	}

	return true;
}

bool fp_term(void)
{
	int ret;

	ret = db_ctx_backup(g_db_ctx, DEF_BACKUP_DATABASE);
	if(ret == false) {
		slog(LOG_ERR, "Could not write database.");
		return false;
	}

	db_ctx_term(g_db_ctx);

	return true;
}

bool fp_delete_fingerprint_info(const char* filename)
{
	int ret;
	json_t* j_tmp;
	char* hash;
	const char* uuid;
	char* sql;

	if(filename == NULL) {
		slog(LOG_WARNING, "Wrong input parameter.");
		return false;
	}

	hash = create_file_hash(filename);
	if(hash == NULL) {
		slog(LOG_ERR, "Could not create hash info.");
		return false;
	}

	// get audio list info
	j_tmp = get_audio_list_info_by_hash(hash);
	if(j_tmp == NULL) {
		slog(LOG_NOTICE, "Could not find audio list info.");
		return false;
	}

	uuid = json_string_value(json_object_get(j_tmp, "uuid"));

	asprintf(&sql, "delete from audio_list where uuid='%s';", uuid);
	ret = db_ctx_exec(g_db_ctx, sql);
	sfree(sql);
	if(ret == false) {
		slog(LOG_WARNING, "Could not delete audio list info. uuid[%s]", uuid);
		json_decref(j_tmp);
		return false;
	}

	asprintf(&sql, "delete from audio_fingerprint where uuid='%s';", uuid);
	ret = db_ctx_exec(g_db_ctx, sql);
	sfree(sql);
	if(ret == false) {
		slog(LOG_WARNING, "Could not delete audio fingerprint info. uuid[%s]", uuid);
		json_decref(j_tmp);
		return false;
	}

	return true;
}

bool fp_craete_fingerprint_info(const char* filename)
{
	int ret;
	char* uuid;

	if(filename == NULL) {
		slog(LOG_WARNING, "Wrong input parameter.");
		return false;
	}

	// create uuid
	uuid = utils_gen_uuid();

	// create audio list info
	ret = create_audio_list_info(filename, uuid);
	if(ret == false) {
		slog(LOG_NOTICE, "Could not create audio list info. May already exist.");
		sfree(uuid);
		return false;
	}

	// create audio fingerprint info
	ret = create_audio_fingerprint_info(filename, uuid);
	if(ret == false) {
		slog(LOG_NOTICE, "Could not create audio fingerprint info.");

		fp_delete_fingerprint_info(filename);
		sfree(uuid);
		return false;
	}

	return true;
}

json_t* fp_search_fingerprint_info(const char* filename, const int coefs)
{
	int ret;
	char* uuid;
	char* sql;
	char* tmp;
	char* tmp_max;
	char* tablename;
	json_t* j_fprints;
	json_t* j_tmp;
	json_t* j_search;
	json_t* j_res;
	int idx;
	int frame_count;
	int i;

	if(filename == NULL) {
		slog(LOG_WARNING, "Wrong input parameter.");
		return NULL;
	}
	slog(LOG_DEBUG, "Fired fp_search_fingerprint_info. filename[%s]", filename);

	if((coefs < 1) || (coefs > DEF_AUBIO_COEFS)) {
		slog(LOG_WARNING, "Wrong coefs count. max[%d], coefs[%d]", DEF_AUBIO_COEFS, coefs);
		return NULL;
	}

	uuid = utils_gen_uuid();

	// create tablename
	tmp = utils_string_replace_char(uuid, '-', '_');
	asprintf(&tablename, "temp_%s", tmp);
	sfree(tmp);

	// create tmp search table
	ret = create_temp_search_table(tablename);
	if(ret == false) {
		slog(LOG_WARNING, "Could not create temp search table. tablename[%s]", tablename);
		sfree(tablename);
		sfree(uuid);
		return NULL;
	}

	// create fingerprint info
	j_fprints = create_audio_fingerprints(filename, uuid);
	sfree(uuid);
	if(j_fprints == NULL) {
		slog(LOG_ERR, "Could not create fingerprint info.");
		delete_temp_search_table(tablename);
		sfree(tablename);
		return NULL;
	}

	// search
	frame_count = json_array_size(j_fprints);
	json_array_foreach(j_fprints, idx, j_tmp) {
		asprintf(&sql, "insert into %s select * from audio_fingerprint where "
				"max1 >= %f and max1 <= %f",
				tablename,
				json_real_value(json_object_get(j_tmp, "max1")) - DEF_SEARCH_RANGE,
				json_real_value(json_object_get(j_tmp, "max1")) + DEF_SEARCH_RANGE
				);

		// add more conditions if the more coefs has given.
		for(i = 1; i < coefs; i++) {
			asprintf(&tmp_max, "max%d", i + 1);

			asprintf(&tmp, "%s and %s >= %f and %s <= %f",
					sql,

					tmp_max,
					json_real_value(json_object_get(j_tmp, tmp_max)) - DEF_SEARCH_RANGE,

					tmp_max,
					json_real_value(json_object_get(j_tmp, tmp_max)) + DEF_SEARCH_RANGE
					);
			sfree(tmp_max);
			sfree(sql);
			sql = tmp;
		}

		asprintf(&tmp, "%s group by audio_uuid", sql);
		sfree(sql);
		sql = tmp;

		db_ctx_exec(g_db_ctx, sql);
		sfree(sql);
	}
	json_decref(j_fprints);

	// get result
	asprintf(&sql, "select *, count(*) from %s group by audio_uuid order by count(*) DESC", tablename);
	db_ctx_query(g_db_ctx, sql);
	sfree(sql);

	j_search = db_ctx_get_record(g_db_ctx);
	db_ctx_free(g_db_ctx);

	// delete temp search table
	ret = delete_temp_search_table(tablename);
	if(ret == false) {
		slog(LOG_WARNING, "Could not delete temp search table. tablename[%s]", tablename);
		sfree(tablename);
		json_decref(j_search);
		return false;
	}
	sfree(tablename);

	if(j_search == NULL) {
		// not found
		slog(LOG_NOTICE, "Could not find data.");
		return NULL;
	}

	// create result
	j_res = get_audio_list_info(json_string_value(json_object_get(j_search, "audio_uuid")));
	if(j_res == NULL) {
		slog(LOG_WARNING, "Could not find audio list info.");
		json_decref(j_search);
		return NULL;
	}

	json_object_set_new(j_res, "frame_count", json_integer(frame_count));
	json_object_set(j_res, "match_count", json_object_get(j_search, "count(*)"));
	json_decref(j_search);

	return j_res;
}

/**
 * Create audio list data and insert it.
 * If the file is already listed, return false.
 * @param filename
 * @param uuid
 * @return
 */
static bool create_audio_list_info(const char* filename, const char* uuid)
{
	int ret;
	char* hash;
	json_t* j_tmp;

	if((filename == NULL) || (uuid == NULL)) {
		slog(LOG_WARNING, "Wrong input parameter.");
		return false;
	}
	slog(LOG_DEBUG, "Fired create_audio_list_info. filename[%s], uuid[%s]", filename, uuid);

	// create file hash
	hash = create_file_hash(filename);
	if(hash == NULL) {
		slog(LOG_WARNING, "Could not create hash info.");
		return false;
	}
	slog(LOG_DEBUG, "Created hash. hash[%s]", hash);

	// check existence
	j_tmp = get_audio_list_info_by_hash(hash);
	if(j_tmp != NULL) {
		slog(LOG_NOTICE, "The given file is already inserted.");
		sfree(hash);
		return false;
	}

	// craete data
	j_tmp = json_pack("{s:s, s:s, s:s}",
			"uuid", 	uuid,
			"name",		filename,
			"hash",		hash
			);
	sfree(hash);

	// insert
	ret = db_ctx_insert(g_db_ctx, "audio_list", j_tmp);
	json_decref(j_tmp);
	if(ret == false) {
		slog(LOG_ERR, "Could not create fingerprint info.");
		return false;
	}

	return true;
}

/**
 * Create audio fingerprint data and insert it.
 * @param filename
 * @param uuid
 * @return
 */
static bool create_audio_fingerprint_info(const char* filename, const char* uuid)
{
	int ret;
	int idx;
	json_t* j_fprint;
	json_t* j_fprints;

	if((filename == NULL) || (uuid == NULL)) {
		slog(LOG_WARNING, "Wrong input parameter.");
		return false;
	}
	slog(LOG_DEBUG, "Fired create_audio_fingerprint_info. filename[%s], uuid[%s]", filename, uuid);

	// craete fingerprint data
	j_fprints = create_audio_fingerprints(filename, uuid);
	if(j_fprints == NULL) {
		slog(LOG_ERR, "Could not create fingerprint data.");
		return false;
	}

	// insert data
	json_array_foreach(j_fprints, idx, j_fprint) {
		ret = db_ctx_insert(g_db_ctx, "audio_fingerprint", j_fprint);
		if(ret == false) {
			slog(LOG_WARNING, "Could not insert fingerprint data.");
			continue;
		}
	}
	json_decref(j_fprints);

	return true;
}

static json_t* create_audio_fingerprints(const char* filename, const char* uuid)
{
	json_t* j_res;
	json_t* j_tmp;
	unsigned int reads;
	int count;
	int samplerate;
	char* source;
	char* tmp;
	int i;

	aubio_pvoc_t* pv;
	cvec_t*	fftgrain;
	aubio_mfcc_t* mfcc;
	fvec_t* mfcc_out;
	fvec_t* mfcc_buf;

	aubio_source_t* aubio_src;

	if((filename == NULL) || (uuid == NULL)) {
		fprintf(stderr, "Wrong input parameter.\n");
		return NULL;
	}
	slog(LOG_DEBUG, "Fired create_audio_fingerprints. filename[%s], uuid[%s]", filename, uuid);

	// initiate aubio src
	source = strdup(filename);
	aubio_src = new_aubio_source(source, DEF_AUBIO_SAMPLERATE, DEF_AUBIO_HOPSIZE);
	sfree(source);
	if(aubio_src == NULL) {
		slog(LOG_ERR, "Could not initiate aubio src.");
		return NULL;
	}

	// initiate aubio parameters
	samplerate = aubio_source_get_samplerate(aubio_src);
	pv = new_aubio_pvoc(DEF_AUBIO_BUFSIZE, DEF_AUBIO_HOPSIZE);
	fftgrain = new_cvec(DEF_AUBIO_BUFSIZE);
	mfcc = new_aubio_mfcc(DEF_AUBIO_BUFSIZE, DEF_AUBIO_FILTER, DEF_AUBIO_COEFS, samplerate);
	mfcc_buf = new_fvec(DEF_AUBIO_HOPSIZE);
	mfcc_out = new_fvec(DEF_AUBIO_COEFS);
	if((pv == NULL) || (fftgrain == NULL) || (mfcc == NULL) || (mfcc_buf == NULL) || (mfcc_out == NULL)) {
		slog(LOG_ERR, "Could not initiate aubio parameters.");

		del_aubio_pvoc(pv);
		del_cvec(fftgrain);
		del_aubio_mfcc(mfcc);
		del_fvec(mfcc_out);
		del_fvec(mfcc_buf);
		del_aubio_source(aubio_src);
		return NULL;
	}

	j_res = json_array();
	count = 0;
	while(1) {
		aubio_source_do(aubio_src, mfcc_buf, &reads);
		if(reads == 0) {
		  break;
		}

		// compute mag spectrum
		aubio_pvoc_do(pv, mfcc_buf, fftgrain);

		// compute mfcc
		aubio_mfcc_do(mfcc, fftgrain, mfcc_out);

		// create mfcc data
		j_tmp = json_pack("{s:i, s:s}",
				"frame_idx",	count,
				"audio_uuid",	uuid
				);
		for(i = 0; i < DEF_AUBIO_COEFS; i++) {
			asprintf(&tmp, "max%d", i + 1);
			json_object_set_new(j_tmp, tmp, json_real(10 * log10(fabs(mfcc_out->data[i]))));
			sfree(tmp);
		}

		if(j_tmp == NULL) {
			slog(LOG_ERR, "Could not create mfcc data.");
			continue;
		}

		json_array_append_new(j_res, j_tmp);
		count++;
	}

	del_aubio_pvoc(pv);
	del_cvec(fftgrain);
	del_aubio_mfcc(mfcc);
	del_fvec(mfcc_out);
	del_fvec(mfcc_buf);
	del_aubio_source(aubio_src);

	return j_res;
}

static bool init_database(void)
{
	int ret;
	char* sql;
	char* tmp;
	int i;

	g_db_ctx = db_ctx_init(DEF_DATABASE_NAME);
	if(g_db_ctx == NULL) {
		return false;
	}

	sql = "create table audio_list("

			"   uuid        varchar(255),"
			"   name        varchar(255),"
			"	hash        varchar(1023)"
			");";
	ret = db_ctx_exec(g_db_ctx, sql);
	if(ret == false) {
		slog(LOG_ERR, "Could not create auido_list table.\n");
		return false;
	}

	// create audio_fingerprint table
	asprintf(&sql, "create table audio_fingerprint("
			" audio_uuid     varchar(255),"
			" frame_idx      integer");
	for(i = 0; i < DEF_AUBIO_COEFS; i++) {
		asprintf(&tmp, "%s, max%d real", sql, i + 1);
		sfree(sql);
		sql = tmp;
	}
	asprintf(&tmp, "%s);", sql);
	sfree(sql);
	sql = tmp;
	ret = db_ctx_exec(g_db_ctx, sql);
	sfree(sql);
	if(ret == false) {
		slog(LOG_ERR, "Could not create fingerprint table.");
		return false;
	}

	// create indices
	for(i = 1; i <= DEF_AUBIO_COEFS; i++) {
		asprintf(&sql, "create index idx_audio_fingerprint_max%d on audio_fingerprint(max%d);", i, i);
		ret = db_ctx_exec(g_db_ctx, sql);
		sfree(sql);
		if(ret == false) {
			slog(LOG_ERR, "Could not create idx_audio_fingerprint_max%d table.", i);
			return false;
		}
	}

	return true;
}

static char* create_file_hash(const char* filename)
{
	unsigned char hash[MD5_DIGEST_LENGTH];
	int i;
	FILE* file;
	MD5_CTX md_ctx;
	int read;
	unsigned char data[1024];
	char* res;
	char* tmp;

	if(filename == NULL) {
		slog(LOG_WARNING, "Wrong input parameter.");
		return NULL;
	}

	file = fopen (filename, "rb");
	if(file == NULL) {
		slog(LOG_WARNING, "Could not open file. filename[%s]", filename);
		return NULL;
	}

	// create hash info
	MD5_Init (&md_ctx);
	while(1) {
		read = fread(data, 1, sizeof(data), file);
		if(read == 0) {
			break;
		}
		MD5_Update(&md_ctx, data, read);
	}
	fclose(file);

	// make hash
	MD5_Final(hash, &md_ctx);

	// create result
	tmp = NULL;
	res = NULL;
    for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
    	asprintf(&tmp, "%s%02x", res? : "", hash[i]);
    	sfree(res);
    	res = strdup(tmp);
    	sfree(tmp);
    }

	return res;
}

static json_t* get_audio_list_info_by_hash(const char* hash)
{
	char* sql;
	json_t* j_res;

	if(hash == NULL) {
		slog(LOG_WARNING, "Wrong input parameter.");
		return NULL;
	}

	asprintf(&sql, "select * from audio_list where hash = '%s';", hash);
	db_ctx_query(g_db_ctx, sql);
	sfree(sql);

	j_res = db_ctx_get_record(g_db_ctx);
	db_ctx_free(g_db_ctx);
	if(j_res == NULL) {
		return NULL;
	}

	return j_res;
}

static json_t* get_audio_list_info(const char* uuid)
{
	char* sql;
	json_t* j_res;

	if(uuid == NULL) {
		slog(LOG_WARNING, "Wrong input parameter.");
		return NULL;
	}

	asprintf(&sql, "select * from audio_list where uuid = '%s';", uuid);
	db_ctx_query(g_db_ctx, sql);
	sfree(sql);

	j_res = db_ctx_get_record(g_db_ctx);
	db_ctx_free(g_db_ctx);
	if(j_res == NULL) {
		return NULL;
	}

	return j_res;
}


static bool create_temp_search_table(const char* tablename)
{
	char* sql;
	char* tmp;
	int i;
	int ret;

	if(tablename == NULL) {
		slog(LOG_WARNING, "Wrong input parameter.");
		return false;
	}



	// create audio_fingerprint table
	asprintf(&sql, "create table %s("
			" audio_uuid     varchar(255),"
			" frame_idx      integer",
			tablename
			);
	for(i = 0; i < DEF_AUBIO_COEFS; i++) {
		asprintf(&tmp, "%s, max%d real", sql, i + 1);
		sfree(sql);
		sql = tmp;
	}
	asprintf(&tmp, "%s);", sql);
	sfree(sql);
	sql = tmp;
	ret = db_ctx_exec(g_db_ctx, sql);
	sfree(sql);
	if(ret == false) {
		slog(LOG_ERR, "Could not create fingerprint search table.");
		return false;
	}

	return true;
}

static bool delete_temp_search_table(const char* tablename)
{
	char* sql;

	if(tablename == NULL) {
		slog(LOG_WARNING, "Wrong input parameter.");
		return false;
	}

	asprintf(&sql, "drop table %s;", tablename);

	db_ctx_exec(g_db_ctx, sql);
	sfree(sql);

	return true;
}
