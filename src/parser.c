//------------------------------------------------------------------------|
// Copyright (c) 2023 by Raymond M. Foulk IV (rfoulk@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//------------------------------------------------------------------------|

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>         // ssize_t
#include <stdbool.h>
#include <ctype.h>

#include "utils.h"          // generic_print_f, memzero()
#include "parser.h"

//------------------------------------------------------------------------|
typedef struct
{
    // The expression and pointers within the expression
    const char * expr;          // The expression
    char * ptr;                 // Current location being evaluated
    char * error_ptr;           // Where last error occurred

    // Current recursion depth
    unsigned char depth;

    // Pointers to string terms along with their associated lengths
    // (so that no strdup/malloc() need occur).  If these are NULL
    // the the terms are to be evaluated as numerical.
    const char * first;
    size_t first_length;

    const char * second;
    size_t second_length;

    // Function and object context for error reporting
    generic_print_f errprintf;
    void * errprintf_object;
}
iparser_data_t;

//------------------------------------------------------------------------|
// Forward declarations for functions that need them because of recursion
static long iparser_expression(iparser_data_t * iparser);
static long iparser_extract_term(iparser_data_t * iparser);
static long iparser_extract_factor(iparser_data_t * iparser);

//------------------------------------------------------------------------|
bool iparser_is_expression(const char * expr)
{
    // TODO: consider checking for any operator also...
    return strchr(expr, '(') && strchr(expr, ')');
}

// The entry point for evaluating expressions
long iparser_evaluate(generic_print_f errprintf,
                      void * errprintf_object,
                      const char * expr)
{
    // Short-lived stack object to help with parsing this expression
    iparser_data_t iparser;

    // Initialize the object
    memzero(&iparser, sizeof(iparser_data_t));
    iparser.expr = (char *) expr;
    iparser.ptr = (char *) expr;
    iparser.errprintf = errprintf;
    iparser.errprintf_object = errprintf_object;

    // Parse the expression.  Sparser evaporates.
    long result = iparser_expression(&iparser);

    // Check if an invalid expression was detected at some point
    if (iparser.error_ptr)
    {
        if (iparser.errprintf)
        {
            iparser.errprintf(iparser.errprintf_object,
                              "Invalid expression at \'%s\' offset %zu\n",
                              iparser.error_ptr,
                              iparser.error_ptr - iparser.expr);
        }

        return IPARSER_INVALID_EXPRESSION;
    }

    return result;
}

//------------------------------------------------------------------------|
// A default error printf function for projects that don't implement one.
int iparser_errprintf(void * stream, const char * format, ...)
{
    va_list args;
    int nchars = 0;

    va_start (args, format);
    nchars = vfprintf((FILE *) stream, format, args);
    va_end (args);

    return nchars;
}

static void iparser_update_final(iparser_data_t * iparser,
                                 const char * start,
                                 size_t length)
{
    // This is basically a 2-unit queue.
    // Move the queue down and add the new term.
    iparser->second = iparser->first;
    iparser->second_length = iparser->first_length;
    iparser->first = start;
    iparser->first_length = length;
}

// Helper function to skip whitespace
static void skip_whitespace(iparser_data_t * iparser)
{
    while (isspace(*iparser->ptr))
    {
        iparser->ptr++;
    }
}

// Looks ahead but does not consume the token
static bool peek_token(iparser_data_t * iparser, const char * token)
{
    skip_whitespace(iparser);
    return !strncmp(iparser->ptr, token, strlen(token));
}

// Matches and consumes the token
static bool match_token(iparser_data_t * iparser, const char * token)
{
    if (peek_token(iparser, token))
    {
        iparser->ptr += strlen(token);
        return true;
    }

    return false;
}

//------------------------------------------------------------------------|
// Look ahead to see if the next operation is addition or subtraction
static bool is_add_sub(iparser_data_t * iparser)
{
    return (peek_token(iparser, "+") ||
            peek_token(iparser, "-"));
}

// Look ahead to see if the next operation is multiplication or division
static bool is_mul_div(iparser_data_t * iparser)
{
    return (peek_token(iparser, "*") ||
            peek_token(iparser, "/"));
}

// Look ahead to see if the next operation is logical
static bool is_logical(iparser_data_t * iparser)
{
    return (peek_token(iparser, "&&") ||
            peek_token(iparser, "||"));
}

// Look ahead to see if the next operation is comparison
static bool is_comparison(iparser_data_t * iparser)
{
    return (peek_token(iparser, "==") ||
            peek_token(iparser, "!=") ||
            peek_token(iparser, ">=") ||
            peek_token(iparser, "<=") ||
            peek_token(iparser, ">") ||
            peek_token(iparser, "<"));
}

//------------------------------------------------------------------------|
static long iparser_handle_add_sub(iparser_data_t * iparser, long left)
{
    while (peek_token(iparser, "+") || peek_token(iparser, "-"))
    {
        char op = *iparser->ptr;
        iparser->ptr++;     // Consume '+' or '-'
        skip_whitespace(iparser);
        long right = iparser_extract_term(iparser);

        if (op == '+')
        {
            left += right;
        }
        else
        {
            left -= right;
        }

        skip_whitespace(iparser);
    }

    return left;
}

static long iparser_handle_mul_div(iparser_data_t * iparser, long left)
{
    while (peek_token(iparser, "*") || peek_token(iparser, "/"))
    {
        char op = *iparser->ptr;
        iparser->ptr++;     // Consume '*' or '/'
        skip_whitespace(iparser);
        long right = iparser_extract_factor(iparser);

        if (op == '*')
        {
            left *= right;
        }
        else
        {
            left /= right;
        }

        skip_whitespace(iparser);
    }

    return left;
}

static long iparser_handle_comparison(iparser_data_t * iparser, long left)
{
    if (match_token(iparser, "=="))
    {
        long right = iparser_expression(iparser);

        // Do string comparison if the last two terms were strings
        if (iparser->first && iparser->second)
        {
            // Unequal lengths cannot be equal strings
            if (iparser->first_length != iparser->second_length)
            {
                return 0;
            }
            else
            {
                return !strncmp(iparser->first, iparser->second, iparser->first_length);
            }
        }

        // Fall back to numeric comparison otherwise
        return left == right;
    }
    else if (match_token(iparser, "!="))
    {
        long right = iparser_expression(iparser);

        // Do string comparison if the last two terms were strings
        if (iparser->first && iparser->second)
        {
            // Unequal lengths cannot be equal strings
            if (iparser->first_length != iparser->second_length)
            {
                return iparser->first_length - iparser->second_length;
            }
            else
            {
                return strncmp(iparser->first, iparser->second, iparser->first_length);
            }
        }

        // Fall back to numeric comparison otherwise
        return left != right;
    }
    else if (match_token(iparser, ">="))
    {
        long right = iparser_expression(iparser);
        return left >= right;
    }
    else if (match_token(iparser, "<="))
    {
        long right = iparser_expression(iparser);
        return left <= right;
    }
    else if (match_token(iparser, ">"))
    {
        long right = iparser_expression(iparser);
        return left > right;
    }
    else if (match_token(iparser, "<"))
    {
        long right = iparser_expression(iparser);
        return left < right;
    }

    return left;
}

static long iparser_handle_logical(iparser_data_t * iparser, long left)
{
    if (match_token(iparser, "&&"))
    {
        long right = iparser_expression(iparser);
        return left && right;
    }
    else if (match_token(iparser, "||"))
    {
        long right = iparser_expression(iparser);
        return left || right;
    }

    return left;
}

// Parse a number - Terminal node in parse tree
static long iparser_final_number(iparser_data_t * iparser)
{
    long result = 0;

    const char * start = iparser->ptr;

    while (isdigit(*iparser->ptr))
    {
        result = 10 * result + (*iparser->ptr - '0');
        iparser->ptr++;
    }

    // If any numeric value was stored (even zero)
    // then track this as a number and not a string
    if (iparser->ptr != start)
    {
        iparser_update_final(iparser, NULL, 0);
    }

    return result;
}

// Parse a string - Terminal node in parse tree
static long iparser_final_string(iparser_data_t * iparser)
{
    // skip the opening double quote
    bool quoted = match_token(iparser, "\"");

    const char * start = iparser->ptr;

    //while (*iparser->ptr != '"' && **input != '\0')
    while (isalpha(*iparser->ptr) || *iparser->ptr == '_')
    {
        iparser->ptr++;
    }

    size_t length = iparser->ptr - start;

    // consume closing quote if present
    quoted &= match_token(iparser, "\"");

    // If any string value was stored (even empty "")
    // then track this as a string and not a number.
    if ((iparser->ptr != start) || quoted)
    {
        iparser_update_final(iparser, start, length);
    }

    // Allow for alphabetization up to 3 chars deep
    // when using greater/less-than comparators
    long result = length >= 1 ? (long) start[0] << 16 : 0;
    result += length >= 2 ? (long) start[1] << 8 : 0;
    result += length >= 3 ? (long) start[2] : 0;
    return result;
}

//------------------------------------------------------------------------|
// Parse an expression -- recursion entry point for all expressions.
static long iparser_expression(iparser_data_t * iparser)
{
    iparser->depth++;

    // limit recursion depth
    if (iparser->depth >= IPARSER_MAX_RECURSION_DEPTH)
    {
        if (iparser->errprintf)
        {
            iparser->errprintf(iparser->errprintf_object,
                               "Maximum recursion depth %d reached\n",
                               iparser->depth);
        }

        iparser->depth--;
        iparser->error_ptr = iparser->ptr;
        return IPARSER_INVALID_EXPRESSION;
    }

    long left = iparser_extract_term(iparser);
    skip_whitespace(iparser);

    // Check for other conditions that should stop any further parsing
    if (iparser->error_ptr)
    {
        // Return to top level early if an error occurred
        iparser->depth--;
        return IPARSER_INVALID_EXPRESSION;
    }
    else if (*iparser->ptr == ')' && iparser->depth <= 1)
    {
        // Return early when unexpected end-parenthesis
        if (iparser->errprintf)
        {
            iparser->errprintf(iparser->errprintf_object,
                               "Unexpected ')'\n");
        }

        iparser->error_ptr = iparser->ptr;
        return IPARSER_INVALID_EXPRESSION;
    }
    else if (!*iparser->ptr)
    {
        // Return early at end-of-expression for efficiency
        iparser->depth--;
        return left;
    }

    if (is_add_sub(iparser))
    {
        left = iparser_handle_add_sub(iparser, left);
    }
    else if (is_comparison(iparser))
    {
        left = iparser_handle_comparison(iparser, left);
    }
    else if(is_logical(iparser))
    {
        left = iparser_handle_logical(iparser, left);
    }

    skip_whitespace(iparser);
    iparser->depth--;
    return left;
}

static long iparser_extract_term(iparser_data_t * iparser)
{
    long left = iparser_extract_factor(iparser);
    skip_whitespace(iparser);

    if (is_mul_div(iparser))
    {
        left = iparser_handle_mul_div(iparser, left);
    }

    return left;
}

static long iparser_extract_factor(iparser_data_t * iparser)
{
    skip_whitespace(iparser);
    long result;

    // Check for parenthetical sub-expression
    if (*iparser->ptr == '(')
    {
        iparser->ptr++;     // Consume '('
        result = iparser_expression(iparser);
        skip_whitespace(iparser);
        if (*iparser->ptr == ')')
        {
            iparser->ptr++;  // Consume ')'
            return result;
        }
        else if (iparser->errprintf)
        {
            iparser->errprintf(iparser->errprintf_object,
                               "Expected ')'\n");
        }

        iparser->error_ptr = iparser->ptr;
        return IPARSER_INVALID_EXPRESSION;
    }
    else if (*iparser->ptr == '!')
    {
        iparser->ptr++;  // Consume '!'
        return !iparser_extract_factor(iparser);
    }
    else if (*iparser->ptr == '-')
    {
        iparser->ptr++;  // Consume '-'
        return -iparser_extract_factor(iparser);
    }
    else if (isdigit(*iparser->ptr))
    {
        result = iparser_final_number(iparser);
        skip_whitespace(iparser);
        return result;
    }
    else if (*iparser->ptr == '"' || isalpha(*iparser->ptr) || *iparser->ptr == '_')
    {
        result = iparser_final_string(iparser);
        skip_whitespace(iparser);
        return result;
    }
    else if (iparser->errprintf)
    {
        iparser->errprintf(iparser->errprintf_object,
                           "Invalid character: %c\n",
                           *iparser->ptr);
    }

    iparser->error_ptr = iparser->ptr;
    return IPARSER_INVALID_EXPRESSION;
}

//------------------------------------------------------------------------|
const iparser_t iparser_pub = {
    &iparser_is_expression,
    &iparser_evaluate,
    &iparser_errprintf,
};
