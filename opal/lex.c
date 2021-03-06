#include "lex.h"

#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

int64_t digit_values[256] =
{
    ['0'] = 0,
    ['1'] = 1,
    ['2'] = 2,
    ['3'] = 3,
    ['4'] = 4,
    ['5'] = 5,
    ['6'] = 6,
    ['7'] = 7,
    ['8'] = 8,
    ['9'] = 9,
    ['a'] = 10, ['A'] = 10,
    ['b'] = 11, ['B'] = 11,
    ['c'] = 12, ['C'] = 12,
    ['d'] = 13, ['D'] = 13,
    ['e'] = 14, ['E'] = 14,
    ['f'] = 15, ['F'] = 15,
};

char escaped_character_chars[256] =
{
    ['0'] = '\0',
    ['n'] = '\n',
    ['t'] = '\t',
    ['r'] = '\r',
    ['b'] = '\b',
    ['\\'] = '\\',
    ['\''] = '\''
};

char escaped_string_chars[256] =
{
    ['0'] = '\0',
    ['n'] = '\n',
    ['t'] = '\t',
    ['r'] = '\r',
    ['b'] = '\b',
    ['\\'] = '\\',
    ['"'] = '"'
};

bool keywords_initilized = false;
const char * keywords[TOKEN_TYPE_KW_END_ - TOKEN_TYPE_KW_START_ - 1];

#define REGISTER_KEYWORD(NAME, name) keywords[TOKEN_TYPE_KW_##NAME - TOKEN_TYPE_KW_START_ - 1] = intern_string(name);
void init_keywords(void)
{
    if (keywords_initilized)
    {
        return;
    }

    REGISTER_KEYWORD(VAR, "var");
    REGISTER_KEYWORD(CONST, "const");
    REGISTER_KEYWORD(IF, "if");
    REGISTER_KEYWORD(ELSE, "else");
    REGISTER_KEYWORD(WHILE, "while");
    REGISTER_KEYWORD(FOR, "for");
    REGISTER_KEYWORD(SWITCH, "switch");
    REGISTER_KEYWORD(OTHERWISE, "otherwise");
    REGISTER_KEYWORD(RETURN, "return");
    REGISTER_KEYWORD(FN, "fn");
    REGISTER_KEYWORD(STRUCT, "struct");
    REGISTER_KEYWORD(UNION, "union");
    REGISTER_KEYWORD(ENUM, "enum");
    REGISTER_KEYWORD(TYPE, "type");
    REGISTER_KEYWORD(CONTINUE, "continue");
    REGISTER_KEYWORD(BREAK, "break");
    REGISTER_KEYWORD(CAST, "cast");
    keywords_initilized = true;
}
#undef REGISTER_KEYWORD

void scan_integer(lexer_t * l)
{
    l->token.type = TOKEN_TYPE_INTEGER;
    l->token.integer = 0;

    int base = 10;
    if (*l->stream == '0')
    {
        base = 8;
        l->stream++;
        switch (*l->stream)
        {
        case 'x': case 'X':
            base = 16;
            l->stream++;
            break;
        case 'b': case 'B':
            base = 2;
            l->stream++;
            break;
        }
    }

    while (digit_values[*l->stream] != 0 || *l->stream == '0' || *l->stream == '_')
    {
        if (*l->stream != '_')
        {
            uint64_t digit_value = digit_values[*l->stream];
            assert(digit_value < base && "Invalid integer digit");
            assert(l->token.integer < (UINT64_MAX - digit_value) / base && "Integer overflow");
            l->token.integer = l->token.integer * base + digit_value;
        }
        l->stream++;
    }
}

void scan_identifier(lexer_t * l)
{
    l->token.type = TOKEN_TYPE_IDENTIFIER;
    const char * start = l->stream;

    while (isalnum(*l->stream) || *l->stream == '_')
    {
        l->stream++;
    }

    uint64_t length = l->stream - start;
    l->token.identifier = intern_string_range(start, l->stream - 1);

    for (int32_t kw = TOKEN_TYPE_KW_START_ + 1; kw < TOKEN_TYPE_KW_END_; ++kw)
    {
        if (l->token.identifier == keywords[kw - TOKEN_TYPE_KW_START_ - 1])
        {
            l->token.type = kw;
            break;
        }
    }
} 

void scan_character(lexer_t * l)
{
    l->token.type = TOKEN_TYPE_INTEGER; // @Todo Parse \x.. and \0..
    l->stream++;
    assert(*l->stream != '\'' && "Invalid character literal");

    if (*l->stream == '\\')
    {
        *l->stream++;
        assert((escaped_character_chars[*l->stream] != 0 || *l->stream == '\0') && "Invalid escaped character literal");
        l->token.integer = escaped_character_chars[*l->stream];
    }
    else
    {
        l->token.integer = *l->stream;
    }

    *l->stream++;
    assert(*l->stream == '\'' && "Unexpected character after character literal");
    *l->stream++;
}

void scan_string(lexer_t * l)
{
    l->token.type = TOKEN_TYPE_STRING; // @Todo Parse \x.. and \0..
    l->stream++;

    sb_t(char) output_string = NULL;
    uint64_t length = 0;

    while (*l->stream != '"' && *l->stream != '\0')
    {
        if (*l->stream == '\\')
        {
            l->stream++;
            assert((escaped_string_chars[*l->stream] != 0 || l->stream == '\0') && "Invalid escaped character in string literal");
            sb_push(output_string, escaped_string_chars[*l->stream]);
        }
        else
        {
            sb_push(output_string, *l->stream);
        }

        l->stream++;
        length++;
    }

    assert(*l->stream == '"' && "End of stream reached inside a string literal");
    l->stream++;

    sb_push(output_string, '\0');
    l->token.string.str = output_string;
    l->token.string.length = length;
}

void scan_operator_2_1(lexer_t * l, token_type_t default_type,
        char char2, token_type_t token2)
{
    if (l->stream[1] == char2)
    {
        l->token.type = token2;
        l->stream += 2;
    }
    else
    {
        l->token.type = default_type;
        l->stream++;
    }
}

void scan_operator_2_2(lexer_t * l, token_type_t default_type,
        char char2_1, token_type_t token2_1,
        char char2_2, token_type_t token2_2)
{
    if (l->stream[1] == char2_1)
    {
        l->token.type = token2_1;
        l->stream += 2;
    }
    else
    {
        scan_operator_2_1(l, default_type, char2_2, token2_2);
    }
}

void scan_operator_2_3(lexer_t * l, token_type_t default_type,
        char char2_1, token_type_t token2_1,
        char char2_2, token_type_t token2_2,
        char char2_3, token_type_t token2_3)
{
    if (l->stream[1] == char2_1)
    {
        l->token.type = token2_1;
        l->stream += 2;
    }
    else
    {
        scan_operator_2_2(l, default_type,
                char2_2, token2_2,
                char2_3, token2_3);
    }
}

bool try_scan_operator_3(lexer_t * l, const char * chars, token_type_t token_type)
{
    if (l->stream[1] != chars[0] || l->stream[2] != chars[1])
    {
        return false;
    }

    l->token.type = token_type;
    l->stream += 3;
    return true;
}

#define HANDLE_CHAR_TOKEN(c, t) case c: { l->token.type = t; l->stream++; break; }
void next_token(lexer_t * l)
{
    assert(l);

    while (isspace(*l->stream))
    {
        l->stream++;
    }

    switch (*l->stream)
    {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        scan_integer(l);
        break;

    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
    case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
    case 's': case 't': case 'u': case 'v': case 'w': case 'x':
    case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
    case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
    case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
    case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
    case 'Y': case 'Z':
    case '_':
        scan_identifier(l);
        break;

    case '\'':
        scan_character(l);
        break;

    case '"':
        scan_string(l);
        break;

    case '-':
        scan_operator_2_3(l, TOKEN_TYPE_MINUS,
                '>', TOKEN_TYPE_ARROW,
                '=', TOKEN_TYPE_ASSIGN_SUB,
                '-', TOKEN_TYPE_DEC);
        break;

    case '&':
        scan_operator_2_2(l, TOKEN_TYPE_AND,
                '&', TOKEN_TYPE_LOGIC_AND,
                '=', TOKEN_TYPE_ASSIGN_AND);
        break;

    case '|':
        scan_operator_2_2(l, TOKEN_TYPE_OR,
                '|', TOKEN_TYPE_LOGIC_OR,
                '=', TOKEN_TYPE_ASSIGN_OR);
        break;

    case '~':
        scan_operator_2_1(l, TOKEN_TYPE_NOT,
                '=', TOKEN_TYPE_ASSIGN_NOT);
        break;

    case '^':
        scan_operator_2_1(l, TOKEN_TYPE_XOR,
                '=', TOKEN_TYPE_ASSIGN_XOR);
        break;

    case '+':
        scan_operator_2_2(l, TOKEN_TYPE_PLUS,
                '=', TOKEN_TYPE_ASSIGN_ADD,
                '+', TOKEN_TYPE_INC);
        break;

    case '*':
        scan_operator_2_1(l, TOKEN_TYPE_MULT,
                '=', TOKEN_TYPE_ASSIGN_MULT);
        break;

    case '/':
        scan_operator_2_1(l, TOKEN_TYPE_DIV,
                '=', TOKEN_TYPE_ASSIGN_DIV);
        break;

    case '%':
        scan_operator_2_1(l, TOKEN_TYPE_MOD,
                '=', TOKEN_TYPE_ASSIGN_MOD);
        break;

    case '=':
        scan_operator_2_1(l, TOKEN_TYPE_ASSIGN,
                '=', TOKEN_TYPE_EQ);
        break;

    case '!':
        scan_operator_2_1(l, TOKEN_TYPE_LOGIC_NOT,
                '=', TOKEN_TYPE_NE);
        break;

    case '<':
        if (!try_scan_operator_3(l, "<=", TOKEN_TYPE_ASSIGN_SHL))
        {
            scan_operator_2_2(l, TOKEN_TYPE_LT,
                    '=', TOKEN_TYPE_LE, 
                    '<', TOKEN_TYPE_SHL);
        }
        break;

    case '>':
        if (!try_scan_operator_3(l, ">=", TOKEN_TYPE_ASSIGN_SHR))
        {
            scan_operator_2_2(l, TOKEN_TYPE_GT,
                    '=', TOKEN_TYPE_GE, 
                    '>', TOKEN_TYPE_SHR);
        }
        break;

    HANDLE_CHAR_TOKEN('?', TOKEN_TYPE_QUESTION);
    HANDLE_CHAR_TOKEN(':', TOKEN_TYPE_COLON);
    HANDLE_CHAR_TOKEN(';', TOKEN_TYPE_SEMICOLON);
    HANDLE_CHAR_TOKEN(',', TOKEN_TYPE_COMMA);
    HANDLE_CHAR_TOKEN('{', TOKEN_TYPE_BRACE_OPEN);
    HANDLE_CHAR_TOKEN('}', TOKEN_TYPE_BRACE_CLOSE);
    HANDLE_CHAR_TOKEN('[', TOKEN_TYPE_BRACKET_OPEN);
    HANDLE_CHAR_TOKEN(']', TOKEN_TYPE_BRACKET_CLOSE);
    HANDLE_CHAR_TOKEN('(', TOKEN_TYPE_PARENTHESIS_OPEN);
    HANDLE_CHAR_TOKEN(')', TOKEN_TYPE_PARENTHESIS_CLOSE);
    HANDLE_CHAR_TOKEN('.', TOKEN_TYPE_DOT);
    HANDLE_CHAR_TOKEN('\0', TOKEN_TYPE_EOF);

    default:
        {
            printf("Invalid character '%c'\n", *l->stream);
            exit(1);
        }
        break;
    }
}
#undef HANDLE_CHAR_TOKEN

void init_lexer(lexer_t * l, const char * input)
{
    assert(l);
    assert(input);
    assert(input[strlen(input)] == '\0');

    init_keywords();
    l->stream = input;
    next_token(l);
}

void test_lexer(void)
{
    lexer_t lexer;

    {
        init_lexer(&lexer, "hello  +  \t\n12_3 world 7");
        assert(lexer.token.type == TOKEN_TYPE_IDENTIFIER);
        assert(strcmp(lexer.token.identifier, "hello") == 0);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_PLUS);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_INTEGER);
        assert(lexer.token.integer == 123);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_IDENTIFIER);
        assert(strcmp(lexer.token.identifier, "world") == 0);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_INTEGER);
        assert(lexer.token.integer == 7);

        next_token(&lexer);
        assert(lexer.token.type == 0);
    }

    {
        init_lexer(&lexer, "0xa_e 015_4 0b0110_0001");
        assert(lexer.token.type == TOKEN_TYPE_INTEGER);
        assert(lexer.token.integer == 174);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_INTEGER);
        assert(lexer.token.integer == 108);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_INTEGER);
        assert(lexer.token.integer == 97);

        next_token(&lexer);
        assert(lexer.token.type == 0);
    }

    {
        init_lexer(&lexer, " '@' '\\'' '\\\\' '\\n'");
        assert(lexer.token.type == TOKEN_TYPE_INTEGER);
        assert(lexer.token.character == '@');

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_INTEGER);
        assert(lexer.token.character == '\'');

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_INTEGER);
        assert(lexer.token.character == '\\');

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_INTEGER);
        assert(lexer.token.character == '\n');

        next_token(&lexer);
        assert(lexer.token.type == 0);
    }

    {
        init_lexer(&lexer, "  \"hello world \\\\ \\\" \n \\n\" \"abc\" ");
        assert(lexer.token.type == TOKEN_TYPE_STRING);
        assert(lexer.token.string.length == 19);
        assert(strcmp("hello world \\ \" \n \n", lexer.token.string.str) == 0);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_STRING);
        assert(lexer.token.string.length == 3);
        assert(strcmp("abc", lexer.token.string.str) == 0);

        next_token(&lexer);
        assert(lexer.token.type == 0);
    }

    {
        init_lexer(&lexer, "> >= >> >>= || &= & -- /= ->");
        assert(lexer.token.type == TOKEN_TYPE_GT);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_GE);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_SHR);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_ASSIGN_SHR);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_LOGIC_OR);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_ASSIGN_AND);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_AND);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_DEC);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_ASSIGN_DIV);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_ARROW);
    }

    {
        init_lexer(&lexer, "if else elseif while const");
        assert(lexer.token.type == TOKEN_TYPE_KW_IF);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_KW_ELSE);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_IDENTIFIER);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_KW_WHILE);

        next_token(&lexer);
        assert(lexer.token.type == TOKEN_TYPE_KW_CONST);
    }
}
