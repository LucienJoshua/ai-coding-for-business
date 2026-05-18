/*
  cJSON - JSON parser in C
*/

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include "cJSON.h"

static void *(*cJSON_malloc)(size_t) = malloc;
static void (*cJSON_free)(void *) = free;

/* Forward declarations for print functions */
static char *print_value(const cJSON *item, char **p);
static char *print_string(const char *str, char **p);

/* Forward declarations for parse functions */
static const char *parse_value(cJSON *item, const char *input);
static const char *parse_array(cJSON *item, const char *input);
static const char *parse_object(cJSON *item, const char *input);
static const char *parse_string(cJSON *item, const char *str);
static const char *parse_number(cJSON *item, const char *num);

void cJSON_InitHooks(cJSON_Hooks* hooks)
{
    if (!hooks)
    {
        cJSON_malloc = malloc;
        cJSON_free = free;
        return;
    }
    cJSON_malloc = (hooks->malloc_fn) ? hooks->malloc_fn : malloc;
    cJSON_free = (hooks->free_fn) ? hooks->free_fn : free;
}

static cJSON *cJSON_New_Item(void)
{
    cJSON* node = (cJSON*)cJSON_malloc(sizeof(cJSON));
    if (node)
        memset(node, 0, sizeof(cJSON));
    return node;
}

void cJSON_Delete(cJSON *item)
{
    cJSON *next;
    while (item)
    {
        next = item->next;
        if (item->child)
            cJSON_Delete(item->child);
        if (item->valuestring)
            cJSON_free(item->valuestring);
        if (item->string)
            cJSON_free(item->string);
        cJSON_free(item);
        item = next;
    }
}

/* Skip whitespace */
static const char *skip(const char *in)
{
    while (in && *in && (unsigned char)*in <= 32)
        in++;
    return in;
}

/* Parse number */
static const char *parse_number(cJSON *item, const char *num)
{
    double n = 0;
    int sign = 1;

    if (*num == '-')
    {
        sign = -1;
        num++;
    }

    for (; *num >= '0' && *num <= '9'; num++)
        n = n * 10.0 + (*num - '0');

    if (*num == '.')
    {
        double scale = 0.1;
        num++;
        for (; *num >= '0' && *num <= '9'; num++)
        {
            n += scale * (*num - '0');
            scale *= 0.1;
        }
    }

    item->valuefloat = sign * n;
    item->valueint = (int)n;
    item->type = cJSON_Number;
    return num;
}

/* Parse string */
static const char *parse_string(cJSON *item, const char *str)
{
    const char *p;
    char *out;
    int len;

    str = skip(str);
    if (*str != '\"')
        return NULL;

    str++;
    p = str;
    len = 0;
    while (*p && *p != '\"')
    {
        if (*p == '\\')
            p++;
        p++;
        len++;
    }

    out = (char*)cJSON_malloc(len + 1);
    if (!out)
        return NULL;

    item->valuestring = out;
    p = str;
    while (*p && *p != '\"')
    {
        if (*p == '\\')
        {
            p++;
            switch (*p)
            {
                case 'n': *out++ = '\n'; break;
                case 'r': *out++ = '\r'; break;
                case 't': *out++ = '\t'; break;
                default: *out++ = *p; break;
            }
        }
        else
            *out++ = *p;
        p++;
    }
    *out = '\0';

    return p + 1;
}

/* Parse array */
static const char *parse_array(cJSON *item, const char *input)
{
    cJSON *child;

    input = skip(input);
    if (*input != '[')
        return NULL;

    item->type = cJSON_Array;
    input++;
    input = skip(input);

    if (*input == ']')
        return input + 1;

    item->child = child = cJSON_New_Item();
    if (!child)
        return NULL;

    input = skip(input);
    input = parse_value(child, input);
    if (!input)
        return NULL;

    while (*input == ',')
    {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item)
            return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;

        input++;
        input = parse_value(child, input);
        if (!input)
            return NULL;
    }

    input = skip(input);
    if (*input != ']')
        return NULL;

    return input + 1;
}

/* Parse object */
static const char *parse_object(cJSON *item, const char *input)
{
    cJSON *child;

    input = skip(input);
    if (*input != '{')
        return NULL;

    item->type = cJSON_Object;
    input++;
    input = skip(input);

    if (*input == '}')
        return input + 1;

    item->child = child = cJSON_New_Item();
    if (!child)
        return NULL;

    input = skip(input);
    input = parse_string(child, input);
    if (!input)
        return NULL;

    child->string = child->valuestring;
    child->valuestring = NULL;

    input = skip(input);
    if (*input != ':')
        return NULL;

    input = skip(input);
    input = parse_value(child, input);
    if (!input)
        return NULL;

    while (*input == ',')
    {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item)
            return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;

        input++;
        input = parse_string(child, input);
        if (!input)
            return NULL;

        child->string = child->valuestring;
        child->valuestring = NULL;

        input = skip(input);
        if (*input != ':')
            return NULL;

        input = skip(input);
        input = parse_value(child, input);
        if (!input)
            return NULL;
    }

    input = skip(input);
    if (*input != '}')
        return NULL;

    return input + 1;
}

/* Parse value */
static const char *parse_value(cJSON *item, const char *input)
{
    input = skip(input);
    if (!input || !*input)
        return NULL;

    if (!strncmp(input, "null", 4))
    {
        item->type = cJSON_NULL;
        return input + 4;
    }
    if (!strncmp(input, "false", 5))
    {
        item->type = cJSON_False;
        return input + 5;
    }
    if (!strncmp(input, "true", 4))
    {
        item->type = cJSON_True;
        item->valueint = 1;
        return input + 4;
    }
    if (*input == '\"')
    {
        input = parse_string(item, input);
        return input;
    }
    if (*input == '[')
        return parse_array(item, input);
    if (*input == '{')
        return parse_object(item, input);
    if (*input == '-' || (*input >= '0' && *input <= '9'))
        return parse_number(item, input);

    return NULL;
}

cJSON *cJSON_Parse(const char *value)
{
    cJSON *c = cJSON_New_Item();
    const char *end;

    if (!c)
        return NULL;

    end = parse_value(c, value);
    if (!end)
    {
        cJSON_Delete(c);
        return NULL;
    }

    return c;
}

int cJSON_GetArraySize(const cJSON *array)
{
    cJSON *c = array->child;
    int i = 0;
    while (c)
    {
        i++;
        c = c->next;
    }
    return i;
}

cJSON *cJSON_GetArrayItem(const cJSON *array, int item)
{
    cJSON *c = array->child;
    while (c && item > 0)
    {
        item--;
        c = c->next;
    }
    return c;
}

cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string)
{
    cJSON *c = object->child;
    while (c)
    {
        if (!strcmp(c->string, string))
            return c;
        c = c->next;
    }
    return NULL;
}

int cJSON_IsNumber(const cJSON *item) { return item->type == cJSON_Number; }
int cJSON_IsString(const cJSON *item) { return item->type == cJSON_String; }
int cJSON_IsArray(const cJSON *item) { return item->type == cJSON_Array; }
int cJSON_IsObject(const cJSON *item) { return item->type == cJSON_Object; }

cJSON *cJSON_CreateNumber(double num)
{
    cJSON *item = cJSON_New_Item();
    if (item)
    {
        item->type = cJSON_Number;
        item->valuefloat = num;
        item->valueint = (int)num;
    }
    return item;
}

cJSON *cJSON_CreateString(const char *string)
{
    cJSON *item = cJSON_New_Item();
    if (item)
    {
        item->type = cJSON_String;
        item->valuestring = (char*)cJSON_malloc(strlen(string) + 1);
        if (item->valuestring)
            strcpy(item->valuestring, string);
    }
    return item;
}

cJSON *cJSON_CreateArray(void)
{
    cJSON *item = cJSON_New_Item();
    if (item)
        item->type = cJSON_Array;
    return item;
}

cJSON *cJSON_CreateObject(void)
{
    cJSON *item = cJSON_New_Item();
    if (item)
        item->type = cJSON_Object;
    return item;
}

void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item)
{
    cJSON *c = object->child;
    if (!c)
    {
        object->child = item;
        item->string = (char*)cJSON_malloc(strlen(string) + 1);
        strcpy(item->string, string);
        return;
    }

    while (c)
    {
        if (!strcmp(c->string, string))
        {
            if (c->child)
                cJSON_Delete(c->child);
            c->child = item;
            return;
        }
        if (!c->next)
        {
            c->next = item;
            item->prev = c;
            item->string = (char*)cJSON_malloc(strlen(string) + 1);
            strcpy(item->string, string);
            return;
        }
        c = c->next;
    }
}

char *cJSON_Print(const cJSON *item)
{
    static char buffer[65536];
    char *p = buffer;
    print_value(item, &p);
    *p = '\0';
    return buffer;
}

static char *print_value(const cJSON *item, char **p)
{
    switch (item->type)
    {
        case cJSON_NULL: strcpy(*p, "null"); *p += 4; break;
        case cJSON_True: strcpy(*p, "true"); *p += 4; break;
        case cJSON_False: strcpy(*p, "false"); *p += 5; break;
        case cJSON_Number: *p += sprintf(*p, "%.16g", item->valuefloat); break;
        case cJSON_String: print_string(item->valuestring, p); break;
        case cJSON_Array:
            **p = '['; (*p)++;
            {
                cJSON *c = item->child;
                while (c)
                {
                    print_value(c, p);
                    if (c->next)
                    {
                        **p = ','; (*p)++;
                    }
                    c = c->next;
                }
            }
            **p = ']'; (*p)++;
            break;
        case cJSON_Object:
            **p = '{'; (*p)++;
            {
                cJSON *c = item->child;
                while (c)
                {
                    print_string(c->string, p);
                    **p = ':'; (*p)++;
                    print_value(c, p);
                    if (c->next)
                    {
                        **p = ','; (*p)++;
                    }
                    c = c->next;
                }
            }
            **p = '}'; (*p)++;
            break;
    }
    return *p;
}

static char *print_string(const char *str, char **p)
{
    **p = '\"'; (*p)++;
    while (*str)
    {
        if (*str == '\"' || *str == '\\')
        {
            **p = '\\'; (*p)++;
            **p = *str; (*p)++;
        }
        else
        {
            **p = *str; (*p)++;
        }
        str++;
    }
    **p = '\"'; (*p)++;
    **p = '\0';
    return *p;
}

const char *cJSON_GetErrorPtr(void) { return NULL; }