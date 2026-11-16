/* The goal of this example is to create a pipeline with an aravis source, and while keeping the pipeline alive, remove
 * the source and replace it with a newly created  aravis source. */

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <unistd.h>
#include <arv.h>

#define USE_FAKE_SOURCE 0

static void
drain_bus (GstElement *pipeline)
{
        GstBus *bus = gst_element_get_bus (pipeline);
        GstMessage *msg;

        while ((msg = gst_bus_pop (bus)) != NULL) {
                if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR) {
                        GError *err = NULL;
                        gchar *dbg = NULL;
                        gst_message_parse_error (msg, &err, &dbg);
                        g_printerr ("Error from %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
                        g_clear_error (&err);
                        g_free (dbg);
                }
                gst_message_unref (msg);   /* releases the reference to the source */
        }
        gst_object_unref (bus);
}

static void
on_source_finalized (gpointer user_data, GObject *where_the_object_was)
{
        gchar *name = user_data;
        g_print ("*** Element '%s' (%p) has been finalized\n",
                 name, (void *) where_the_object_was);
        g_free (name);
}

static GstElement *
make_source (const gchar *name)
#if USE_FAKE_SOURCE
{
        GstElement *source;

       source = gst_element_factory_make ("fakesrc", name);
        if (!source) {
                g_printerr ("Failed to create the fakesrc element\n");
                return NULL;
        }
        g_object_weak_ref (G_OBJECT (source), on_source_finalized, g_strdup (name));

        g_object_set (source,
                      "sync", TRUE,
                      "datarate", 10240,
                      "sizemax", 1024,
                      NULL);
        gst_util_set_object_arg (G_OBJECT (source), "sizetype", "fixed");

        return source;
}
#else
{
        GstElement *source;

        source = gst_element_factory_make ("aravissrc", name);
        if (!source) {
                g_printerr ("Failed to create the aravissrc element\n");
                return NULL;
        }
        g_object_weak_ref (G_OBJECT (source), on_source_finalized, g_strdup (name));

        g_object_set(G_OBJECT(source),
                     "camera-name", "Fake_1",
                     "num-arv-buffers", 5,
                     "packet-resend", FALSE,
                     "packet-size", 9000,
                     "auto-packet-size", FALSE,
                     NULL);

        return source;
}
#endif

static void
on_handoff (GstElement *sink, GstBuffer *buffer, GstPad *pad, gpointer user_data)
{
        static unsigned buffer_count = 0;

        buffer_count++;
        g_print ("  fakesink got buffer #%u (%" G_GSIZE_FORMAT " bytes)\n",
                 buffer_count, gst_buffer_get_size (buffer));
}

static gboolean
wait_for_state (GstElement *pipeline, GstState target)
{
        GstState state;
        GstStateChangeReturn ret;

        ret= gst_element_get_state (pipeline, &state, NULL, 5 * GST_SECOND);

        return ret != GST_STATE_CHANGE_FAILURE && state == target;
}

static void
swap_source_test (void)
{
        GstElement *pipeline;
        GstElement *source;
        GstElement *sink;
        int i;

        /* Create the initial pipeline */

        g_print ("Pipeline creation\n");

        pipeline = gst_pipeline_new ("aravis-pipeline");
        source   = make_source ("source-0");
        sink = gst_element_factory_make ("fakesink", "fake_sink");
        g_object_set (sink,
                      "sync", TRUE,
                      "signal-handoffs", TRUE,
                      NULL);
        g_signal_connect (sink, "handoff", G_CALLBACK (on_handoff), NULL);

        if (!pipeline || !source || !sink) {
                g_printerr ("Not all elements could be created.\n");
                g_assert_not_reached();
        }

        gst_bin_add_many (GST_BIN (pipeline), source, sink, NULL);
        if (!gst_element_link (source, sink)) {
                g_printerr ("Elements could not be linked.\n");
                gst_object_unref (pipeline);
                g_assert_not_reached();
        }

        /* Start it */

        g_print ("Start the pipeline\n");
        if (gst_element_set_state (pipeline, GST_STATE_PLAYING)
            == GST_STATE_CHANGE_FAILURE || !wait_for_state (pipeline, GST_STATE_PLAYING)) {
                g_printerr ("Failed to start pipeline\n");
                gst_object_unref (pipeline);
                g_assert_not_reached();
        }

        g_print ("Wait for a few frames\n");
        sleep (1);

        for (i = 1; i <= 5; i++) {
                char *name;

                /* Now we are trying to swap the source of the pipeline. First, we stop the source */

                g_print ("Remove the source\n");
                g_assert_cmpint (gst_element_set_state (source, GST_STATE_NULL), !=, GST_STATE_CHANGE_FAILURE);
                gst_element_unlink (source, sink);
                g_assert (gst_bin_remove (GST_BIN (pipeline), source));
                source = NULL;

                /* Allow the destuction of the removed source */

                drain_bus (pipeline);

                /* Then we create a new source */

                g_print ("Create a new source\n");

                name = g_strdup_printf ("source-%i", i);
                source   = make_source (name);
                g_free (name);

                /* Add it to the pipeline */

                g_assert (gst_bin_add(GST_BIN(pipeline), source));
                if (!gst_element_link (source, sink)) {
                        g_printerr ("Could not link new source to sink\n");
                        gst_element_set_state (pipeline, GST_STATE_NULL);
                        gst_object_unref (pipeline);
                        g_assert_not_reached();
                }

                if (!gst_element_sync_state_with_parent (source)) {
                        g_printerr ("Could not sync new source state with pipeline\n");
                        gst_element_set_state (pipeline, GST_STATE_NULL);
                        gst_object_unref (pipeline);
                        g_assert_not_reached();
                }

                g_print ("Wait for a few frames\n");
                sleep (1);
        }

        /* Free everything now */

        g_print ("Stop the pipeline\n");
        g_assert_cmpint (gst_element_set_state (pipeline, GST_STATE_NULL), ==, GST_STATE_CHANGE_SUCCESS);

        g_print ("Free the pipeline\n");
        gst_object_unref (pipeline);
}

/* use-downstream-pool: frames are received directly into buffers of the pool
 * the next element proposes (here videoconvert's input pool). */

typedef struct {
        int n_frames;
        int n_from_pool;
        int n_ramp;
        GType pool_type;
} PoolCount;

static GstPadProbeReturn
count_pool_buffers (GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
        PoolCount *count = user_data;
        GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);
        GstMapInfo map;

        count->n_frames++;
        if (buffer->pool != NULL) {
                count->n_from_pool++;
                count->pool_type = G_OBJECT_TYPE (buffer->pool);
        }
        /* Real image data: the fake camera's rows are ramps (x + y + frame id). */
        if (gst_buffer_map (buffer, &map, GST_MAP_READ)) {
                if (map.size > 3 && map.data[1] != map.data[0] && map.data[2] != map.data[1])
                        count->n_ramp++;
                gst_buffer_unmap (buffer, &map);
        }

        return GST_PAD_PROBE_OK;
}

static void
run_pool_pipeline (const char *downstream, PoolCount *count)
{
        GstElement *pipeline = gst_pipeline_new ("pool");
        GstElement *source = make_source ("pool-source");
        GstElement *rest;
        GstElement *sink;
        GstPad *pad;
        GError *error = NULL;
        int i;

        g_object_set (source, "num-arv-buffers", 4, "use-downstream-pool", TRUE, NULL);
        rest = gst_parse_bin_from_description (downstream, TRUE, &error);
        g_assert_no_error (error);
        gst_bin_add_many (GST_BIN (pipeline), source, rest, NULL);
        g_assert_true (gst_element_link (source, rest));
        sink = gst_bin_get_by_name (GST_BIN (rest), "sink");
        g_assert_nonnull (sink);

        pad = gst_element_get_static_pad (source, "src");
        gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, count_pool_buffers, count, NULL);
        gst_object_unref (pad);

        g_assert_cmpint (gst_element_set_state (pipeline, GST_STATE_PLAYING), !=, GST_STATE_CHANGE_FAILURE);
        for (i = 0; i < 20; i++) {
                GstSample *sample = gst_app_sink_try_pull_sample (GST_APP_SINK (sink), 3 * GST_SECOND);

                g_assert_nonnull (sample);
                gst_sample_unref (sample);
        }
        g_assert_cmpint (gst_element_set_state (pipeline, GST_STATE_NULL), ==, GST_STATE_CHANGE_SUCCESS);
        gst_object_unref (sink);
        gst_object_unref (pipeline);
}

static void
downstream_pool_test (void)
{
        PoolCount count = { 0 };

        run_pool_pipeline ("videoconvert ! video/x-raw,format=RGBA ! appsink name=sink sync=false", &count);
        g_print ("%d frames, %d from the downstream pool (%s), %d with image data\n",
                 count.n_frames, count.n_from_pool, g_type_name (count.pool_type), count.n_ramp);
        g_assert_cmpint (count.n_frames, >=, 20);
        g_assert_cmpint (count.n_from_pool, ==, count.n_frames);
        g_assert_true (g_type_is_a (count.pool_type, GST_TYPE_BUFFER_POOL));
        g_assert_cmpint (count.n_ramp, ==, count.n_frames);
}

static void
downstream_pool_fallback_test (void)
{
        PoolCount count = { 0 };

        /* appsink proposes no pool: own buffers, frames still flow. */
        run_pool_pipeline ("appsink name=sink sync=false", &count);
        g_assert_cmpint (count.n_frames, >=, 20);
        g_assert_cmpint (count.n_from_pool, ==, 0);
        g_assert_cmpint (count.n_ramp, ==, count.n_frames);
}

int
main (int argc, char *argv[])
{
	int result;

        g_setenv ("GST_PLUGIN_PATH", GST_PLUGIN_PATH, TRUE);
	g_test_init (&argc, &argv, NULL);

        gst_init (&argc, &argv);

	arv_set_fake_camera_genicam_filename (GENICAM_FILENAME);
	arv_enable_interface ("Fake");

	g_test_add_func ("/gst/swap-source-test", swap_source_test);
	g_test_add_func ("/gst/downstream-pool", downstream_pool_test);
	g_test_add_func ("/gst/downstream-pool-fallback", downstream_pool_fallback_test);

	result = g_test_run();

	arv_shutdown ();
        gst_deinit();

	return result;
}

