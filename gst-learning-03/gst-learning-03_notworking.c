//
//  gst-learning-03.c
//  gst-learning-03
//
//  Created by Wenhao Zhang on 2021/12/23.
//
#include <gst/gst.h>

/* Structures to contain all our informaion, so we can pass it to callbacks */
typedef struct _CustomData
{
    GstElement *pipeline;
    GstElement *source;
    GstElement *a_convert;
    GstElement *a_resample;
    GstElement *a_sink;
    GstElement *v_convert;
    GstElement *v_sink;
} CustomData;

/* Handler for the pad-added signal */
static void pad_added_handler_audio(GstElement *src, GstPad *new_pad, CustomData *data);
static void pad_added_handler_video(GstElement *src, GstPad *new_pad, CustomData *data);
static void pad_added_handler_debug(GstElement *src, GstPad *new_pad, CustomData *data);

int main(int argc, char *argv[])
{
    CustomData data;
    GMainLoop *main_loop;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);
    main_loop = g_main_loop_new(NULL, FALSE);

    /* Create the elements */
    data.source = gst_element_factory_make("uridecodebin", "source");
    data.a_convert = gst_element_factory_make("audioconvert", "a_convert");
    data.a_resample = gst_element_factory_make("audioresample", "a_resample");
    data.a_sink = gst_element_factory_make("autoaudiosink", "a_sink");
    data.v_convert = gst_element_factory_make("videoconvert", "v_convert");
    data.v_sink = gst_element_factory_make("autovideosink", "v_sink");

    /* Create the empty pipeline */
    data.pipeline = gst_pipeline_new("test-pipeline");

    if (!data.pipeline || !data.source
    || !data.a_convert || !data.a_resample || !data.a_sink
    || !data.v_convert || !data.v_sink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    /* Build the pipeline */
    /* Note that we are NOT linking the source at this point. We will do it later */
    gst_bin_add_many(GST_BIN(data.pipeline), data.source,
                     data.a_convert, data.a_resample, data.a_sink,
                     data.v_convert, data.v_sink, NULL);
    if (!gst_element_link_many(data.a_convert, data.a_resample, data.a_sink, NULL))
    {
        g_printerr("Audio elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    /*
    if (!gst_element_link_many(data.v_convert, data.v_sink, NULL))
    {
        g_printerr("Video elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    */

    /* Set the URI to play */
    g_object_set(data.source, "uri", "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);

    /* Connect to the pad-added signal */
    // g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);
    g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler_video), &data);

    /* Start playing */
    ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    g_main_loop_run(main_loop);

    /* Listen to the bus */
    bus = gst_element_get_bus(data.pipeline);
    do
    {
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                         GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
        /* Parse message */
        if (msg != NULL)
        {
            GError *err;
            gchar *debug_info;
            switch (GST_MESSAGE_TYPE(msg))
            {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug_info);
                g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
                g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_free(debug_info);
                break;
            case GST_MESSAGE_EOS:
                g_print("End of stream reached.\n");
                terminate = TRUE;
                break;
            case GST_MESSAGE_STATE_CHANGED:
                /* We are only interested in state-changed messages from the pipeline */
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data.pipeline))
                {
                    GstState old_state, new_state, pending_state;
                    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                    g_print("Pipeline state changed from %s to %s:\n",
                            gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
                }
                break;
            default:
                g_printerr("Unexpected message recieved.");
                break;
            }
            gst_message_unref(msg);
        }
    } while (!terminate);

    g_main_loop_quit(main_loop);

    /* Free resources */
    gst_object_unref(bus);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
    return 0;
}

/* This function will be called by the pad-added signal */
static void pad_added_handler_debug(GstElement *src, GstPad *new_pad, CustomData *data)
{
    GstPad *a_sink_pad = gst_element_get_static_pad(data->a_convert, "a_sink");
    GstPad *v_sink_pad = gst_element_get_static_pad(data->v_convert, "v_sink");
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    /* Check the new pad's type */
    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);
    if (g_str_has_prefix(new_pad_type, "audio/x-raw"))
    {
        /* link the audio pad to audio convertor */
        /* If our converter is already linked, we have nothing to do */
        if (gst_pad_is_linked(a_sink_pad))
        {
            g_print("We are already linked. Ignoring.\n");
            goto exit;
        }

        /* Attempt the link */
        ret = gst_pad_link(new_pad, a_sink_pad);
        if (GST_PAD_LINK_FAILED(ret))
            g_print("Type is '%s' but link failed.\n", new_pad_type);
        else
            g_print("Link succeeded (type '%s').\n", new_pad_type);

        goto exit;
    }
    else if (g_str_has_prefix(new_pad_type, "vidio/x-raw"))
    {
        /* link the video pad to video convertor */
        /* If our converter is already linked, we have nothing to do */
        if (gst_pad_is_linked(v_sink_pad))
        {
            g_print("We are already linked. Ignoring.\n");
            goto exit;
        }

        /* Attempt the link */
        ret = gst_pad_link(new_pad, v_sink_pad);
        if (GST_PAD_LINK_FAILED(ret))
            g_print("Type is '%s' but link failed.\n", new_pad_type);
        else
            g_print("Link succeeded (type '%s').\n", new_pad_type);

        goto exit;
    }
    else
    {
        g_print("It has type '%s' which is not supported. Ignoring.\n", new_pad_type);
    }
    
exit:
    /* Unreference the new pad's caps, if we got them */
    if (new_pad_caps != NULL)
        gst_caps_unref(new_pad_caps);
    
    /* Unreference the sink pad */
    gst_object_unref(a_sink_pad);
    gst_object_unref(v_sink_pad);
}

static void pad_added_handler_debug_audio(GstElement *src, GstPad *new_pad, CustomData *data)
{
    GstPad *sink_pad = gst_element_get_static_pad(data->a_convert, "sink");
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    /* If our converter is already linked, we have nothing to do here */
    if (gst_pad_is_linked(sink_pad))
    {
        g_print("We are already linked. Ignoring.\n");
        goto exit;
    }

    /* Check the new pad's type */
    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);
    if (!g_str_has_prefix(new_pad_type, "audio/x-raw"))
    {
        g_print("It has type '%s' which is not raw audio. Ignoring.\n", new_pad_type);
        goto exit;
    }

    /* Attempt the link */
    ret = gst_pad_link(new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED(ret))
    {
        g_print("Type is '%s' but link failed.\n", new_pad_type);
    }
    else
    {
        g_print("Link succeeded (type '%s').\n", new_pad_type);
    }

exit:
    /* Unreference the new pad's caps, if we got them */
    if (new_pad_caps != NULL)
        gst_caps_unref(new_pad_caps);

    /* Unreference the sink pad */
    gst_object_unref(sink_pad);
}

static void pad_added_handler_video(GstElement *src, GstPad *new_pad, CustomData *data)
{
    GstPad *sink_pad = gst_element_get_static_pad(data->v_convert, "sink");
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    /* If our converter is already linked, we have nothing to do here */
    if (gst_pad_is_linked(sink_pad))
    {
        g_print("We are already linked. Ignoring.\n");
        goto exit;
    }

    /* Check the new pad's type */
    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);
    if (!g_str_has_prefix(new_pad_type, "video/x-raw"))
    {
        g_print("It has type '%s' which is not raw video. Ignoring.\n", new_pad_type);
        goto exit;
    }

    /* Attempt the link */
    ret = gst_pad_link(new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED(ret))
    {
        g_print("Type is '%s' but link failed.\n", new_pad_type);
    }
    else
    {
        g_print("Link succeeded (type '%s').\n", new_pad_type);
    }

exit:
    /* Unreference the new pad's caps, if we got them */
    if (new_pad_caps != NULL)
        gst_caps_unref(new_pad_caps);

    /* Unreference the sink pad */
    gst_object_unref(sink_pad);
}