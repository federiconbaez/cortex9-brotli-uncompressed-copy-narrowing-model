/* CLI field parsing, shared with the tests. */
#ifndef CX9_CLI_PARSE_H
#define CX9_CLI_PARSE_H
#include "brotli_copy.h"
/* f[0..5] = AVAIL_IN ACC_BYTES META_LEN WBITS POS SIZE_T_BITS; rb_size is
 * set to 1 << WBITS. 0 on success, -1 on any malformed or out-of-domain
 * field (then *in is unspecified). */
int cx9_parse_fields(char **f, cx9_input *in);
/* pre|fix|size_t_min -> version, anything else -> -1 */
int cx9_parse_version(const char *s);
#endif
