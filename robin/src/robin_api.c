/*
 * robin_api.c
 *
 * Robin API, used as static library from clients.
 *
 * Luca Zulberti <l.zulberti@studenti.unipi.it>
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "robin.h"
#include "robin_api.h"
#include "lib/socket.h"

/*
 * Log shortcuts
 */

#define err(fmt, args...)  robin_log_err(ROBIN_LOG_ID_API, fmt, ## args)
#define warn(fmt, args...) robin_log_warn(ROBIN_LOG_ID_API, fmt, ## args)
#define info(fmt, args...) robin_log_info(ROBIN_LOG_ID_API, fmt, ## args)
#define dbg(fmt, args...)  robin_log_dbg(ROBIN_LOG_ID_API, fmt, ## args)


/*
 * Local types and macros
 */

#define ROBIN_REPLY_LINE_MAX_LEN 300

static int _ra_send(const char *fmt, ...);
#define ra_send(fmt, args...) _ra_send(fmt "\n", ## args)


/*
 * Local data
 */

static int client_fd;
static char *msg_buf = NULL, *reply_buf = NULL;


/*
 * Local functions
 */

static int _ra_send(const char *fmt, ...)
{
    va_list args;
    int msg_len;

    va_start(args, fmt);
    msg_len = vsnprintf(NULL, 0, fmt, args) + 1;
    va_end(args);

    dbg("ra_send: msg_len=%d", msg_len);

    msg_buf = realloc(msg_buf, msg_len * sizeof(char));
    if (!msg_buf) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    va_start(args, fmt);
    if (vsnprintf(msg_buf, msg_len, fmt, args) < 0) {
        err("vsnprintf: %s", strerror(errno));
        return -1;
    }
    va_end(args);

    dbg("ra_send: msg_buf=%.*s", msg_len - 2, msg_buf);

    /* do not send '\0' in msg */
    if (socket_sendn(client_fd, msg_buf, msg_len - 1) < 0) {
        err("socket_sendn: failed to send data to socket");
        return -1;
    }

    return 0;
}

void ra_free_reply(char **reply)
{
    int i = 0;

    while(reply[i])
        free(reply[i++]);

    free(reply);
}

static int ra_wait_reply(char ***replies, int *nrep)
{
    char vbuf[ROBIN_REPLY_LINE_MAX_LEN], **l;
    int nbuf;
    int reply_ret;
    size_t reply_len = 0;

    nbuf = socket_recvline(&reply_buf, &reply_len, client_fd,
                          vbuf, ROBIN_REPLY_LINE_MAX_LEN);
    if (nbuf < 0) {
        err("wait_reply: failed to receive a line from the server");
        return -1;
    }

    /* first line contains the number of lines in reply (except the first),
     * or error code if < 0
     */
    reply_ret = strtol(vbuf, NULL, 10);
    if (reply_ret < 0) {
        l = calloc(2, sizeof(char *));  /* last line is terminator */
    } else {
        l = calloc(reply_ret + 2, sizeof(char *));  /* last line is terminator */
    }

    if (!l) {
        err("calloc: %s", strerror(errno));
        return -1;
    }

    dbg("wait_reply: reply_ret=%d", reply_ret);

    /* always store first line (it is not counted in nrep) */
    l[0] = malloc(nbuf * sizeof(char));
    if (!l[0]) {
        err("malloc: %s", strerror(errno));
        ra_free_reply(l);
        return -1;
    }

    memcpy(l[0], vbuf, nbuf - 1);   /* do not copy '\n' */
    l[0][nbuf - 1] = '\0';

    if (reply_ret > 0) {
        for (int i = 0; i < reply_ret; i++) {
            nbuf = socket_recvline(&reply_buf, &reply_len, client_fd,
                                   vbuf, ROBIN_REPLY_LINE_MAX_LEN);
            if (nbuf < 0) {
                err("wait_reply: failed to receive a line from the server");
                ra_free_reply(l);
                return -1;
            }

            l[i + 1] = malloc(nbuf * sizeof(char));
            if (!l[i + 1]) {
                err("malloc: %s", strerror(errno));
                ra_free_reply(l);
                return -1;
            }

            memcpy(l[i + 1], vbuf, nbuf - 1);  /* do not copy '\n' */
            l[i + 1][nbuf - 1] = '\0';
        }
    }

    *replies = l;
    *nrep = reply_ret;

    return 0;
}


/*
 * Exported functions
 */

int robin_api_init(int fd)
{
    client_fd = fd;

    return 0;
}

void robin_api_free(void)
{
    if (msg_buf) {
        dbg("free: msg_buf=%p", msg_buf);
        free(msg_buf);
    }

    if (reply_buf) {
        dbg("free: reply_buf=%p", reply_buf);
        free(reply_buf);
    }
}

int robin_api_register(const char *email, const char *password)
{
    char **replies;
    int nrep, ret;

    ret = ra_send("register %s %s", email, password);
    if (ret) {
        err("register: could not send the message to the server");
        return -1;
    }

    ret = ra_wait_reply(&replies, &nrep);
    if (ret) {
        err("register: could not retrieve the reply from the server");
        return -1;
    }

    dbg("register: reply: %s", replies[0]);

    ra_free_reply(replies);

    /* check for errors */
    if (nrep < 0)
        return nrep;

    return 0;
}

int robin_api_login(const char *email, const char *password)
{
    char **replies;
    int nrep, ret;

    ret = ra_send("login %s %s", email, password);
    if (ret) {
        err("login: could not send the message to the server");
        return -1;
    }

    ret = ra_wait_reply(&replies, &nrep);
    if (ret) {
        err("login: could not retrieve the reply from the server");
        return -1;
    }

    dbg("login: reply: %s", replies[0]);

    ra_free_reply(replies);

    /* check for errors */
    if (nrep < 0)
        return nrep;

    return 0;
}

int robin_api_logout(void)
{
    char **replies;
    int nrep, ret;

    ret = ra_send("logout");
    if (ret) {
        err("logout: could not send the message to the server");
        return -1;
    }
    ret = ra_wait_reply(&replies, &nrep);
    if (ret) {
        err("logout: could not retrieve the reply from the server");
        return -1;
    }

    dbg("logout: reply: %s", replies[0]);

    /* check for errors */
    if (nrep < 0)
        return nrep;

    return 0;
}

int robin_api_follow(const char *emails, robin_reply_t *reply)
{
    char **replies;
    int nrep, ret;
    int *results;

    ret = ra_send("follow %s", emails);
    if (ret) {
        err("follow: could not send the message to the server");
        return -1;
    }

    ret = ra_wait_reply(&replies, &nrep);
    if (ret) {
        err("follow: could not retrieve the reply from the server");
        return -1;
    }

    dbg("follow: reply: %s", replies[0]);

    /* check for errors */
    if (nrep < 0)
        return nrep;

    results = malloc(nrep * sizeof(int));
    if (!results) {
        err("malloc: %s", strerror(errno));
        return -1;
    }

    for (int i = 0; i < nrep; i++) {
        char *res = strchr(replies[i + 1], ' ');
        *(res++) = '\0';

        results[i] = strtol(res, NULL, 10);

        dbg("follow: user=%s res=%d", replies[i + 1], results[i]);
    }

    reply->n = nrep;
    reply->data = results;
    reply->free_ptr = results;

    ra_free_reply(replies);

    return nrep;
}

int robin_api_cip(const char *msg)
{
    char **replies, *msg_to_send, *next;
    char const *last;
    int nrep, len, delta, ret;

    last = msg;
    msg_to_send = NULL;
    len = 0;

    do {
        next = strchr(last, '\n');
        if (next)
            delta = next - last + 1;  /* '\n' -> "\\n" */
        else
            delta = strlen(last);

        msg_to_send = realloc(msg_to_send, len + delta);
        if (!msg_to_send) {
            err("realloc: %s", strerror(errno));
            return -1;
        }

        if (next) {
            memcpy(msg_to_send + len, last, delta - 2);
            memcpy(msg_to_send + len + delta - 2, "\\n", 2);
        } else
            memcpy(msg_to_send + len, last, delta);

        len += delta;

        last = next + 1;
    } while (next);

    ret = ra_send("cip \"%.*s\"", len, msg_to_send);
    if (ret) {
        err("follow: could not send the message to the server");
        return -1;
    }

    ret = ra_wait_reply(&replies, &nrep);
    if (ret) {
        err("follow: could not retrieve the reply from the server");
        return -1;
    }

    if (nrep < 0)
        return nrep;

    return 0;
}

int robin_api_followers(robin_reply_t *reply)
{
    char **replies;
    int nrep, ret;

    replies = NULL;

    ret = ra_send("followers");
    if (ret) {
        err("followers: could not send the message to the server");
        return -1;
    }

    ret = ra_wait_reply(&replies, &nrep);
    if (ret) {
        err("follow: could not retrieve the reply from the server");
        return -1;
    }

    if (nrep < 0)
        return nrep;

    /* free up first line and terminator pointer */
    free(replies[0]);
    free(replies[nrep + 1]);

    reply->n = nrep;
    reply->data = &replies[1];
    reply->free_ptr = replies;

    return 0;
}

