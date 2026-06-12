#ifndef TWEET_PARSER_H
#define TWEET_PARSER_H

#include <stddef.h>

typedef struct
{
    char* id;
    char* created_at;
    char* full_text;
    char* media_json;
    char* quote_of_id;
} TweetData;

typedef struct
{
    char* type;
    char* original_url;
    int valid;
} TweetMedia;

char* non_evil_strdup(const char* s);

TweetData* parse_csv_file(const char* filename, size_t* out_count);
TweetData* find_by_id(TweetData* tweets, size_t count, const char* id);
void free_TweetData_array(TweetData* arr, size_t count);
TweetMedia parse_media_object(const char* json_object);


#endif