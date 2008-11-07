/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 *  Copyright (C) 2008  Kouhei Sutou <kou@cozmixng.org>
 *
 *  This library is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "../../config.h"
#endif /* HAVE_CONFIG_H */

#include <gcutter.h>
#include "milter-manager-test-server.h"
#include "milter-manager-test-utils.h"

#define MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(obj)                    \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj),                                \
                                 MILTER_TYPE_MANAGER_TEST_SERVER,      \
                                 MilterManagerTestServerPrivate))

typedef struct _MilterManagerTestServerPrivate	MilterManagerTestServerPrivate;
struct _MilterManagerTestServerPrivate
{
    guint n_add_headers;
    guint n_change_headers;
    guint n_insert_headers;
    guint n_change_froms;
    guint n_add_recipients;
    guint n_delete_recipients;
    guint n_replace_bodies;
    guint n_progresses;
    guint n_quarantines;
    guint n_reply_codes;

    GList *received_add_headers;
    GList *received_change_headers;
    GList *received_insert_headers;
    GList *received_add_recipients;
    GList *received_delete_recipients;
    GList *received_replace_bodies;
    GList *received_change_froms;
    GList *received_quarantine_reasons;
    GList *received_reply_codes;
};

enum
{
    PROP_0
};

static MilterReplySignals *reply_parent;
static void reply_init (MilterReplySignalsClass *reply);

G_DEFINE_TYPE_WITH_CODE(MilterManagerTestServer,
                        milter_manager_test_server,
                        MILTER_TYPE_SERVER_CONTEXT,
                        G_IMPLEMENT_INTERFACE(MILTER_TYPE_REPLY_SIGNALS,
                                              reply_init))

static void dispose        (GObject         *object);
static void set_property   (GObject         *object,
                            guint            prop_id,
                            const GValue    *value,
                            GParamSpec      *pspec);
static void get_property   (GObject         *object,
                            guint            prop_id,
                            GValue          *value,
                            GParamSpec      *pspec);
static void add_header     (MilterReplySignals *reply,
                            const gchar        *name,
                            const gchar        *value);
static void insert_header  (MilterReplySignals *reply,
                            guint32             index,
                            const gchar        *name,
                            const gchar        *value);
static void change_header  (MilterReplySignals *reply,
                            const gchar        *name,
                            guint32             index,
                            const gchar        *value);
static void change_from    (MilterReplySignals *reply,
                            const gchar        *from,
                            const gchar        *parameters);
static void add_recipient  (MilterReplySignals *reply,
                            const gchar        *recipient,
                            const gchar        *parameters);
static void delete_recipient
                           (MilterReplySignals *reply,
                            const gchar        *recipient);
static void replace_body   (MilterReplySignals *reply,
                            const gchar        *body,
                            gsize               body_size);
static void progress       (MilterReplySignals *reply);
static void quarantine     (MilterReplySignals *reply,
                            const gchar        *reason);
static void reply_code     (MilterReplySignals *reply,
                            guint               code,
                            const gchar        *extended_code,
                            const gchar        *message);

static void
milter_manager_test_server_class_init (MilterManagerTestServerClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose      = dispose;
    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;

    g_type_class_add_private(gobject_class,
                             sizeof(MilterManagerTestServerPrivate));
}

static void
reply_init (MilterReplySignalsClass *reply)
{
    reply_parent = g_type_interface_peek_parent(reply);

    reply->add_header       = add_header;
    reply->change_header    = change_header;
    reply->insert_header    = insert_header;
    reply->change_from      = change_from;
    reply->add_recipient    = add_recipient;
    reply->delete_recipient = delete_recipient;
    reply->replace_body     = replace_body;
    reply->progress         = progress;
    reply->quarantine       = quarantine;
    reply->reply_code       = reply_code;
}

static void
milter_manager_test_server_init (MilterManagerTestServer *milter)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(milter);

    priv->received_add_headers = NULL;
    priv->received_change_headers = NULL;
    priv->received_insert_headers = NULL;
    priv->received_add_recipients = NULL;
    priv->received_delete_recipients = NULL;
    priv->received_change_froms = NULL;
    priv->received_replace_bodies = NULL;
    priv->received_quarantine_reasons = NULL;
    priv->received_reply_codes = NULL;

    priv->n_add_headers = 0;
    priv->n_change_headers = 0;
    priv->n_insert_headers = 0;
    priv->n_change_froms = 0;
    priv->n_add_recipients = 0;
    priv->n_delete_recipients = 0;
    priv->n_replace_bodies = 0;
    priv->n_progresses = 0;
    priv->n_quarantines = 0;
    priv->n_reply_codes = 0;
}

static void
dispose (GObject *object)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(object);

    if (priv->received_add_headers) {
        g_list_foreach(priv->received_add_headers,
                       (GFunc)milter_manager_test_header_free, NULL);
        g_list_free(priv->received_add_headers);
        priv->received_add_headers = NULL;
    }

    if (priv->received_change_headers) {
        g_list_foreach(priv->received_change_headers,
                       (GFunc)milter_manager_test_header_free, NULL);
        g_list_free(priv->received_change_headers);
        priv->received_change_headers = NULL;
    }

    if (priv->received_insert_headers) {
        g_list_foreach(priv->received_insert_headers,
                       (GFunc)milter_manager_test_header_free, NULL);
        g_list_free(priv->received_insert_headers);
        priv->received_insert_headers = NULL;
    }

    if (priv->received_add_recipients) {
        g_list_foreach(priv->received_add_recipients,
                       (GFunc)milter_manager_test_header_free,
                       NULL);
        g_list_free(priv->received_add_recipients);
        priv->received_add_recipients = NULL;
    }

    if (priv->received_delete_recipients) {
        g_list_foreach(priv->received_delete_recipients, (GFunc)g_free, NULL);
        g_list_free(priv->received_delete_recipients);
        priv->received_delete_recipients = NULL;
    }

    if (priv->received_change_froms) {
        g_list_foreach(priv->received_change_froms,
                       (GFunc)milter_manager_test_header_free,
                       NULL);
        g_list_free(priv->received_change_froms);
        priv->received_change_froms = NULL;
    }

    if (priv->received_replace_bodies) {
        g_list_foreach(priv->received_replace_bodies, (GFunc)g_free, NULL);
        g_list_free(priv->received_replace_bodies);
        priv->received_replace_bodies = NULL;
    }

    if (priv->received_quarantine_reasons) {
        g_list_foreach(priv->received_quarantine_reasons, (GFunc)g_free, NULL);
        g_list_free(priv->received_quarantine_reasons);
        priv->received_quarantine_reasons = NULL;
    }

    if (priv->received_reply_codes) {
        g_list_foreach(priv->received_reply_codes, (GFunc)g_free, NULL);
        g_list_free(priv->received_reply_codes);
        priv->received_reply_codes = NULL;
    }

    G_OBJECT_CLASS(milter_manager_test_server_parent_class)->dispose(object);
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(object);
    switch (prop_id) {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(object);
    switch (prop_id) {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

MilterManagerTestServer *
milter_manager_test_server_new (void)
{
    return  g_object_new(MILTER_TYPE_MANAGER_TEST_SERVER,
                         NULL);
}

static void
add_header (MilterReplySignals *reply,
            const gchar        *name,
            const gchar        *value)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(reply);
    priv->n_add_headers++;
    priv->received_add_headers =
        g_list_append(priv->received_add_headers,
                      milter_manager_test_header_new(0, name, value));
}

static void
insert_header (MilterReplySignals *reply,
               guint32             index,
               const gchar        *name,
               const gchar        *value)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(reply);
    priv->n_insert_headers++;
    priv->received_insert_headers =
        g_list_append(priv->received_insert_headers,
                      milter_manager_test_header_new(index, name, value));
}

static void
change_header (MilterReplySignals *reply,
               const gchar        *name,
               guint32             index,
               const gchar        *value)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(reply);
    priv->n_change_headers++;
    priv->received_change_headers =
        g_list_append(priv->received_change_headers,
                      milter_manager_test_header_new(index, name, value));
}

static void
change_from (MilterReplySignals *reply,
             const gchar        *from,
             const gchar        *parameters)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(reply);
    priv->n_change_froms++;

    priv->received_change_froms =
        g_list_prepend(priv->received_change_froms,
                       milter_manager_test_pair_new(from, parameters));
}

static void
add_recipient (MilterReplySignals *reply,
               const gchar        *recipient,
               const gchar        *parameters)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(reply);
    priv->n_add_recipients++;

    priv->received_add_recipients =
        g_list_prepend(priv->received_add_recipients,
                       milter_manager_test_pair_new(recipient, parameters));
}

static void
delete_recipient (MilterReplySignals *reply, const gchar *recipient)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(reply);
    priv->n_delete_recipients++;

    priv->received_delete_recipients =
        g_list_append(priv->received_delete_recipients, g_strdup(recipient));
}

static void
replace_body (MilterReplySignals *reply,
              const gchar        *body,
              gsize               body_size)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(reply);
    priv->n_replace_bodies++;

    priv->received_replace_bodies =
        g_list_append(priv->received_replace_bodies,
                      g_strndup(body, body_size));
}

static void
progress (MilterReplySignals *reply)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(reply);
    priv->n_progresses++;
}

static void
quarantine (MilterReplySignals *reply, const gchar *reasons)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(reply);

    priv->n_quarantines++;

    priv->received_quarantine_reasons =
        g_list_append(priv->received_quarantine_reasons,
                      g_strdup(reasons));
}

static void
reply_code(MilterReplySignals *reply,
           guint               code,
           const gchar        *extended_code,
           const gchar        *message)
{
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(reply);

    priv->n_reply_codes++;

    priv->received_reply_codes =
        g_list_append(priv->received_reply_codes,
                      milter_utils_format_reply_code(code,
                                                     extended_code,
                                                     message));
}

guint
milter_manager_test_server_get_n_add_headers (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->n_add_headers;
}

guint
milter_manager_test_server_get_n_insert_headers (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->n_insert_headers;
}

guint
milter_manager_test_server_get_n_change_headers (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->n_change_headers;
}

guint
milter_manager_test_server_get_n_change_froms (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->n_change_froms;
}

guint
milter_manager_test_server_get_n_add_recipients (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->n_add_recipients;
}

guint
milter_manager_test_server_get_n_delete_recipients (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->n_delete_recipients;
}

guint
milter_manager_test_server_get_n_replace_bodies (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->n_replace_bodies;
}

guint
milter_manager_test_server_get_n_progresses (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->n_progresses;
}

guint
milter_manager_test_server_get_n_quarantines (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->n_quarantines;
}

guint
milter_manager_test_server_get_n_reply_codes (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->n_reply_codes;
}

static gboolean
cb_waiting (gpointer data)
{
    gboolean *waiting = data;

    *waiting = FALSE;
    return FALSE;
}

const GList *
milter_manager_test_server_get_received_add_headers (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->received_add_headers;
}

const GList *
milter_manager_test_server_get_received_change_headers (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->received_change_headers;
}

const GList *
milter_manager_test_server_get_received_insert_headers (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->received_insert_headers;
}

const GList *
milter_manager_test_server_get_received_add_recipients (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->received_add_recipients;
}

const GList *
milter_manager_test_server_get_received_delete_recipients (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->received_delete_recipients;
}

const GList *
milter_manager_test_server_get_received_change_froms (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->received_change_froms;
}

const GList *
milter_manager_test_server_get_received_replace_bodies (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->received_replace_bodies;
}

const GList *
milter_manager_test_server_get_received_quarantine_reasons (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->received_quarantine_reasons;
}

const GList *
milter_manager_test_server_get_received_reply_codes (MilterManagerTestServer *server)
{
    return MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server)->received_reply_codes;
}

void
milter_manager_test_server_wait_signal (MilterManagerTestServer *server)
{
    gboolean timeout_waiting = TRUE;
    gboolean idle_waiting = TRUE;
    guint timeout_waiting_id, idle_id;
    MilterManagerTestServerPrivate *priv;

    priv = MILTER_MANAGER_TEST_SERVER_GET_PRIVATE(server);

    idle_id = g_idle_add_full(G_PRIORITY_DEFAULT,
                              cb_waiting, &idle_waiting, NULL);

    timeout_waiting_id = g_timeout_add_seconds(1, cb_waiting,
                                               &timeout_waiting);
    while (idle_waiting) {
        g_main_context_iteration(NULL, FALSE);
    }

    g_source_remove(idle_id);
    g_source_remove(timeout_waiting_id);

    cut_assert_true(timeout_waiting, "timeout");
}

/*
vi:ts=4:nowrap:ai:expandtab:sw=4
*/
