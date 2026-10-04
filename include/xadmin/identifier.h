#ifndef XADMIN_IDENTIFIER_H
#define XADMIN_IDENTIFIER_H
#include <stdbool.h>
#include <stddef.h>

/* Input is counted UTF-8, never an implicitly truncated C string. Outputs are
 * caller owned. Account names are ASCII, 3..64 bytes, case insensitive; email
 * local parts are case sensitive, domains ASCII/case insensitive. EAI/IDN must
 * be converted by the caller. Phone defaults contain an explicit +country code.
 * No provider-specific email rewriting or country-specific trunk rewriting. */
typedef enum XAIdentifierKind {
    XA_IDENTIFIER_INVALID, XA_IDENTIFIER_ACCOUNT, XA_IDENTIFIER_PHONE,
    XA_IDENTIFIER_EMAIL
} XAIdentifierKind;
typedef struct XAIdentifier {
    XAIdentifierKind kind;
    char key[255];
} XAIdentifier;

static bool XA_AccountKey(const char* input, size_t size, char output[65]);
static bool XA_IdentifierParse(const char* input, size_t size,
                              const char* default_country_code, XAIdentifier* output);
#endif
