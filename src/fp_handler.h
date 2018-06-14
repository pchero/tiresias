/*
 * fp_handler.h
 *
 *  Created on: Jun 10, 2018
 *      Author: pchero
 */

#ifndef FP_HANDLER_H_
#define FP_HANDLER_H_

bool fp_init(void);
bool fp_term(void);

bool fp_delete_fingerprint_info(const char* filename);
bool fp_craete_fingerprint_info(const char* filename);
json_t* fp_search_fingerprint_info(const char* filename, const int coefs);

#endif /* FP_HANDLER_H_ */
