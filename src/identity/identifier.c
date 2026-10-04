/* Shared by all account creation and authentication boundaries. */
#include "../../include/xadmin/identifier.h"
#include <string.h>

static bool XA_AsciiLetter(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
static char XA_AsciiLower(unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : (char)c;
}
static bool XA_AccountKey(const char* input, size_t size, char output[65])
{
    size_t i; bool letter = false;
    if (!output) return false;
    output[0] = 0;
    if (!input || size < 3 || size > 64) return false;
    for (i = 0; i < size; i++) {
        unsigned char c = (unsigned char)input[i];
        if (XA_AsciiLetter(c)) letter = true;
        else if (!(c >= '0' && c <= '9') && c != '_' && c != '-' && c != '.') return false;
    }
    if (!letter) return false;
    for (i = 0; i < size; i++) output[i] = XA_AsciiLower((unsigned char)input[i]);
    output[size] = 0;
    return true;
}
static bool XA_EmailKey(const char* input, size_t size, char* output)
{
    const char* at; size_t i, local, label = 0; bool dot = false;
    if (size > 254 || !(at = memchr(input, '@', size))) return false;
    local = (size_t)(at - input);
    if (!local || local > 64 || local + 1 >= size || input[0] == '.' || at[-1] == '.') return false;
    /* Deliberately support unquoted ASCII mailbox syntax. Reject whitespace,
     * controls, embedded NUL and quoted/comment syntax rather than guessing. */
    for (i = 0; i < local; i++) {
        unsigned char c = (unsigned char)input[i];
        if (!XA_AsciiLetter(c) && !(c >= '0' && c <= '9') &&
            !strchr(".!#$%&'*+-/=?^_`{|}~", c)) return false;
        if (!c || (c == '.' && i && input[i - 1] == '.')) return false;
    }
    for (i = local + 1; i < size; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c == '.') {
            if (!label || input[i - 1] == '-') return false;
            label = 0; dot = true;
        } else {
            if (!XA_AsciiLetter(c) && !(c >= '0' && c <= '9') && c != '-') return false;
            if ((!label && c == '-') || ++label > 63) return false;
        }
    }
    if (!dot || !label || input[size - 1] == '-') return false;
    memcpy(output, input, local + 1);
    for (i = local + 1; i < size; i++) output[i] = XA_AsciiLower((unsigned char)input[i]);
    output[size] = 0;
    return true;
}
static bool XA_PhoneKey(const char* input, size_t size, const char* country, char* output)
{
    size_t i = 0, used = 1;
    output[0] = '+';
    if (size && input[0] == '+') i = 1;
    else if (size >= 2 && input[0] == '0' && input[1] == '0') i = 2;
    else {
        size_t n = country ? strlen(country) : 0;
        if (n < 2 || n > 4 || country[0] != '+' || country[1] < '1' || country[1] > '9') return false;
        for (i = 1; i < n; i++) if (country[i] < '0' || country[i] > '9') return false;
        memcpy(output + 1, country + 1, n - 1); used = n; i = 0;
        /* National trunk prefixes are ambiguous without a numbering-plan
         * library. Require explicit international form for leading-zero input. */
        if (!size || input[0] == '0') return false;
    }
    for (; i < size; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c == ' ' || c == '-' || c == '(' || c == ')') continue;
        if (c < '0' || c > '9' || used >= 16) return false;
        output[used++] = (char)c;
    }
    if (used < 9 || used > 16 || output[1] == '0') return false;
    output[used] = 0; return true;
}
static bool XA_IdentifierParse(const char* input, size_t size, const char* country, XAIdentifier* output)
{
    char account[65]; XAIdentifier result = {0};
    if (!output) return false;
    memset(output, 0, sizeof(*output));
    if (!input || !size || size > 254 || memchr(input, 0, size)) return false;
    if (memchr(input, '@', size)) {
        if (!XA_EmailKey(input, size, result.key)) return false;
        result.kind = XA_IDENTIFIER_EMAIL;
    } else if (XA_AccountKey(input, size, account)) {
        strcpy(result.key, account); result.kind = XA_IDENTIFIER_ACCOUNT;
    } else {
        if (!XA_PhoneKey(input, size, country, result.key)) return false;
        result.kind = XA_IDENTIFIER_PHONE;
    }
    *output = result; return true;
}
