#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <cjson/cJSON.h>

#include "tweet_parser.h"

enum
{
    HEADER,
    FOOTER,
    TWEET,
    TWEET_PHOTO,
    TWEET_VIDEO,
    QRT,
    TEMPLATE_COUNT
};
const char* template_filenames[TEMPLATE_COUNT] =
{
    "template_header.html",
    "template_footer.html",
    "template_tweet.html",
    "template_tweet_photo.html",
    "template_tweet_video.html",
    "template_qrt.html"
};

#define TEXT_BLUEP   "[TEXT]"
#define DATE_BLUEP   "[DATE]"
#define QUOTED_BLUEP "[QUOTED]"
#define MEDIA_BLUEP  "[URL]"

static char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    char* buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = 0;

    fclose(f);
    return buf;
}

static char* replace(const char* src, const char* pattern, const char* text)
{
    const char* pos = strstr(src, pattern);
    if (!pos) return non_evil_strdup(src);

    size_t before = pos - src;
    size_t after  = strlen(pos + strlen(pattern));

    size_t new_len = before + strlen(text) + after + 1;

    char* out = malloc(new_len);
    memcpy(out, src, before);
    memcpy(out + before, text, strlen(text));
    memcpy(out + before + strlen(text), pos + strlen(pattern), after + 1);
    return out;
}

static void append(FILE* out, const char* filename) {
    char* data = read_file(filename);
    if (!data) return;

    fprintf(out, "%s", data);
    free(data);
}

static char* render_tweet(TweetData* t, TweetData* tweets, size_t count) {
    char* template;
    int is_qrt = 0, is_media_tweet = 0;
    TweetMedia m = parse_media_object(t->media_json);
    if (m.valid)
    {
        is_media_tweet = 1;
        if (strcmp(m.type, "photo") == 0) template = read_file(template_filenames[TWEET_PHOTO]);
        else if (strcmp(m.type, "video") == 0) template = read_file(template_filenames[TWEET_VIDEO]);
    }
    else if (strcmp(t->quote_of_id, "") != 0)
    {
        is_qrt = 1;
        template = read_file(template_filenames[QRT]);
    }
    else template = read_file(template_filenames[TWEET]);
    if (!template) return non_evil_strdup("");

    template = replace(template, TEXT_BLUEP, t->full_text ? t->full_text : "");
    template = replace(template, DATE_BLUEP, t->created_at ? t->created_at : "");
    if (is_media_tweet) template = replace(template, MEDIA_BLUEP, m.original_url ? m.original_url : "");
    if (is_qrt)
    {
        TweetData* quoted = find_by_id(tweets, count, t->quote_of_id);
        if (!quoted) template = replace(template, QUOTED_BLUEP, "");

        char* quoted_html = render_tweet(quoted, tweets, count);
        template = replace(template, QUOTED_BLUEP, quoted_html);
        free(quoted_html);
    }

    return template;
}

static void write_tweet(FILE* out, TweetData* t, TweetData* tweets, size_t count) {
    char* html = render_tweet(t, tweets, count);
    fprintf(out, "%s\n", html);
    free(html);
}

int main(int argc, char** argv) {
    if (argc != 2)
    {
        printf("usage: %s <tweets csv file>", argv[0]);
        return 1;
    }

    FILE* final_webpage = fopen("wasteland.html", "w");
    if (!final_webpage) return 1;

    size_t tweet_count = 0;
    TweetData* tweets = parse_csv_file(argv[1], &tweet_count);
    if (!tweets) return 1; // kill myself

    // ---
    append(final_webpage, template_filenames[HEADER]);

    for (size_t i = 0; i < tweet_count; i++)
    {
        write_tweet(final_webpage, &tweets[i], tweets, tweet_count);
    }

    append(final_webpage, template_filenames[FOOTER]);
    // ---

    fclose(final_webpage);
    free_TweetData_array(tweets, tweet_count);

    return 0;
}
