package org.easydarwin.blogdemos;

import android.annotation.TargetApi;
import android.media.MediaCodec;
import android.media.MediaFormat;
import android.os.Build;
import android.util.Log;
import android.view.SurfaceHolder;

import java.nio.ByteBuffer;

public class H264Decoder {
    private MediaCodec mCodec;
    // private long mBaseTick = 0;
    // private long mNowTick = 0;

    private static int HEAD_OFFSET = 512;
    private MediaCodec.BufferInfo mBufferInfo;
    private ByteBuffer[] mInputBuffers;
    private boolean flag = false;
    private ByteBuffer mInputBuffer;
    // private int timeInternal = 0;
    // boolean f = true;

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    public H264Decoder(SurfaceHolder holder, String mimeType, int w, int h, int timeInternal) {
        try {
            // mCodec =
            // MediaCodec.createByCodecName("OMX.allwinner.video.decoder.avc");
            mCodec = MediaCodec.createDecoderByType(mimeType);
            // mBaseTick = System.currentTimeMillis();
            // this.timeInternal = timeInternal;
            MediaFormat mediaFormat = MediaFormat.createVideoFormat(mimeType, w, h);

            // mediaFormat.setByteBuffer("csd-0" , ByteBuffer.wrap(sps));
            // mediaFormat.setByteBuffer("csd-1", ByteBuffer.wrap(pps));
            mCodec.configure(mediaFormat, holder.getSurface(), null, 0);
            mCodec.start();

            flag = true;
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @SuppressWarnings("deprecation")
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    public boolean onFrame(byte[] buf, int offset, int length) {
        try {
            if (mCodec != null && flag) {
                Log.e("Media", "onFrame start：length " + length);
                // Get input buffer index
                mInputBuffers = mCodec.getInputBuffers();
                int inputBufferIndex = mCodec.dequeueInputBuffer(100000);

                // if (mCount == 0) {
                // mBaseTick = System.currentTimeMillis();
                // }
                if (inputBufferIndex >= 0 && flag && mCodec != null) {
                    mInputBuffer = mInputBuffers[inputBufferIndex];
                    mInputBuffer.clear();
                    mInputBuffer.put(buf, offset, length);
                    mCodec.queueInputBuffer(inputBufferIndex, 0, length, 0, 0);
                    // mCount++;
                } else {
                    Log.e("Media", "onFrame index:" + inputBufferIndex);
                    return false;
                }

                // Get output buffer index
                mBufferInfo = new MediaCodec.BufferInfo();
                int outputBufferIndex = mCodec.dequeueOutputBuffer(mBufferInfo, 0);
                while (outputBufferIndex >= 0 && flag && mCodec != null) {
                    mCodec.releaseOutputBuffer(outputBufferIndex, true);
                    outputBufferIndex = mCodec.dequeueOutputBuffer(mBufferInfo, 0);
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        /*
         * do { mNowTick = System.currentTimeMillis();
         *
         * if ((mNowTick - mBaseTick) < (mCount*(1000/this.timeInternal))) { try
         * { Thread.sleep(10); } catch (InterruptedException e) { // TODO
         * Auto-generated catch block e.printStackTrace(); }
         *
         * } else break; }while(true);
         */
         //Log.e("Media", "onFrame end");

        // Log.d("Media", "one frame take time:"+ (mNowTick - mBaseTick));

        return true;
    }

    /**
     * Find H264 frame head
     *
     * @param buffer
     * @param len
     * @return the offset of frame head, return 0 if can not find one
     */

    public int findHead(byte[] buffer, int len) {
        int i;
        for (i = HEAD_OFFSET; i < len; i++) {
            if (checkHead(buffer, i))
                break;
        }
        if (i == len)
            return 0;
        if (i == HEAD_OFFSET)
            return 0;
        return i;
    }

    /**
     * Check if is H264 frame head
     *
     * @param buffer
     * @param offset
     * @return whether the src buffer is frame head
     */
    public boolean checkHead(byte[] buffer, int offset) {
        // 00 00 00 01
        if (buffer[offset] == 0 && buffer[offset + 1] == 0 && buffer[offset + 2] == 0 && buffer[3] == 1)
            return true;
        // 00 00 01
        if (buffer[offset] == 0 && buffer[offset + 1] == 0 && buffer[offset + 2] == 1)
            return true;
        return false;
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    public void DecoderClose() {
        flag = false;

        if (mCodec != null) {
            mCodec.stop();
            mCodec.release();
            mCodec = null;
        }
    }
}