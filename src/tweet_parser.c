#include "tweet_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>

char* non_evil_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static char* remove_links(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* out = (char*)malloc(len + 1);

    size_t j = 0;
    for (size_t i = 0; i < len; i++)
    {
        if ((i + 7 < len && strncmp(&s[i], "http://", 7) == 0) ||
            (i + 8 < len && strncmp(&s[i], "https://", 8) == 0))
        {
            while (i < len && s[i] != ' ' && s[i] != '\n' && s[i] != '\r')
                i++;

            i--;
            continue;
        }

        out[j++] = s[i];
    }
    out[j] = '\0';
    return out;
}

static char* parse_date(const char* s)
{
    if (!s) return NULL;

    static const char* months[] =
    {
        "jan.", "feb.", "mar.", "apr.", "may", "jun.",
        "jul.", "aug.", "sep.", "oct.", "nov.", "dec."
    };

    char buf[64];
    size_t len = strlen(s);

    if (len >= sizeof(buf)) len = sizeof(buf) - 1;

    memcpy(buf, s, len);
    buf[len] = '\0';

    for (size_t i = 0; i < len; i++)
    {
        if (buf[i] == ' ' && i + 1 < len && buf[i + 1] == '+')
        {
            buf[i] = '\0';
            break;
        }
    }

    int year, mon, day, hour, min, sec;

    if (sscanf(buf, "%d-%d-%d %d:%d:%d", &year, &mon, &day, &hour, &min, &sec) != 6)
    {
        return non_evil_strdup(s);
    }

    char* out = malloc(32);

    snprintf(out, 32,
        "%02d %s %d, %02d:%02d",
        day,
        months[mon - 1],
        year,
        hour,
        min
    );

    return out;
}

static char** parse_csv_line(const char* line, int* out_count) {
    int cap = 32;
    char** fields = malloc(sizeof(char*) * cap);
    int count = 0;

    char buf[16384];
    int buf_idx = 0;

    int in_quotes = 0;

    for (const char* p = line; /* omit */; p++)
    {
        char c = *p;

        int end = (c == '\0' || c == '\n' || c == '\r');

        if (end && !in_quotes)
        {
            buf[buf_idx] = '\0';

            if (count >= cap)
            {
                cap *= 2;
                fields = realloc(fields, sizeof(char*) * cap);
            }

            fields[count++] = non_evil_strdup(buf);
            break;
        }

        if (c == '"')
        {
            if (in_quotes && p[1] == '"')
            {
                buf[buf_idx++] = '"';
                p++;
            }
            else in_quotes = !in_quotes;
        }
        else if (c == ',' && !in_quotes)
        {
            buf[buf_idx] = '\0';

            if (count >= cap)
            {
                cap *= 2;
                fields = realloc(fields, sizeof(char*) * cap);
            }

            fields[count++] = non_evil_strdup(buf);
            buf_idx = 0;
        }
        else if (!end) buf[buf_idx++] = c;
    }

    *out_count = count;
    return fields;
}

static TweetData build_tweet(char** f, int n) {
    TweetData t = {0};

    if (n > 0) t.id             = non_evil_strdup(f[0]);
    if (n > 1)
    {
        char* tmp = parse_date(f[1]);
        t.created_at = non_evil_strdup(tmp);
        free(tmp);
    }
    if (n > 2)
    {
        char* tmp = remove_links(f[2]);
        t.full_text = non_evil_strdup(tmp);
        free(tmp);
    }
    if (n > 3) t.media_json     = non_evil_strdup(f[3]);
    if (n > 10) t.quote_of_id   = non_evil_strdup(f[10]);

    return t;
}

static void free_fields(char** f, int n) {
    for (int i = 0; i < n; i++) free(f[i]);
    free(f);
}

TweetData* parse_csv_file(const char* filename, size_t* out_count) {
    FILE* fp = fopen(filename, "r");
    if (!fp) return NULL;

    size_t cap = 128;
    size_t count = 0;

    TweetData* arr = malloc(sizeof(TweetData) * cap);

    char line[65536];
    fgets(line, sizeof(line), fp);

    while (fgets(line, sizeof(line), fp))
    {
        int n = 0;
        char** fields = parse_csv_line(line, &n);

        if (count >= cap)
        {
            cap *= 2;
            arr = realloc(arr, sizeof(TweetData) * cap);
        }

        arr[count++] = build_tweet(fields, n);
        free_fields(fields, n);
    }

    fclose(fp);

    if (out_count) *out_count = count;
    return arr;
}

TweetData* find_by_id(TweetData* tweets, size_t count, const char* id) {
    for (size_t i = 0; i < count; i++)
    {
        if (tweets[i].id && strcmp(tweets[i].id, id) == 0)
        {
            return &tweets[i];
        }
    }

    return NULL;
}

void free_TweetData_array(TweetData* arr, size_t count) {
    if (!arr) return;

    for (size_t i = 0; i < count; i++)
    {
        free(arr[i].id);
        free(arr[i].created_at);
        free(arr[i].full_text);
        free(arr[i].media_json);
        free(arr[i].quote_of_id);
    }

    free(arr);
}

TweetMedia parse_media_object(const char* json_object) {
    TweetMedia m = {0};

    cJSON* root = cJSON_Parse(json_object);
    if (!root || !cJSON_IsArray(root))
    {
        cJSON_Delete(root);
        return m;
    }

    if (cJSON_GetArraySize(root) == 0)
    {
        cJSON_Delete(root);
        return m;
    }

    cJSON* item = cJSON_GetArrayItem(root, 0);
    if (item)
    {
        cJSON* type = cJSON_GetObjectItem(item, "type");
        cJSON* orig = cJSON_GetObjectItem(item, "original");

        if (cJSON_IsString(type)) m.type         = non_evil_strdup(type->valuestring);
        if (cJSON_IsString(orig)) m.original_url = non_evil_strdup(orig->valuestring);
    }

    cJSON_Delete(root);

    m.valid = (m.type != NULL && m.original_url != NULL);
    return m;
}
