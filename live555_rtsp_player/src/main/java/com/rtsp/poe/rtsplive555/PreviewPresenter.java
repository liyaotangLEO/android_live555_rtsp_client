package com.rtsp.poe.rtsplive555;

import android.content.Context;
import android.os.Handler;
import android.util.Log;
import android.view.SurfaceView;

import org.easydarwin.blogdemos.H264Decoder;

/**
 * @author liyaotang
 * @date 2019/1/23
 *
 * 预览
 *
 */
public class PreviewPresenter {

    private final String TAG = getClass().getSimpleName();

    private PreviewView mPreviewView;

    private Context context;

    private Handler mHandler = new Handler();

    private H264Decoder decoder;

    private SurfaceView surfaceView;

    /** rtsp地址 */
    private String previewAddress = RTSPClient.RTSP_PATH;

    /** 是否开始了预览 */
    private boolean isStartPreview = false;

    private final String MINE_TYPE = "video/avc";
    private int previewWidth = 1920;
    private int previewHeight = 1080;

    private RTSPClient.Callback callback = new RTSPClient.Callback() {

        private byte[] mPpsSps;

        @Override
        public void onFrame(byte[] frameData, int len) {
            // 记录pps和sps
            if ((frameData[0] == 0 && frameData[1] == 0 && frameData[2] == 1 && (frameData[3] & 0x1f) == 7) || (frameData[0] == 0
                    && frameData[1] == 0 && frameData[2] == 0 && frameData[3] == 1 && (frameData[4] & 0x1f) == 7)) {
                mPpsSps = frameData;
            } else if ((frameData[0] == 0 && frameData[1] == 0 && frameData[2] == 1 && (frameData[3] & 0x1f) == 5)
                    || (frameData[0] == 0 && frameData[1] == 0 && frameData[2] == 0 && frameData[3] == 1
                    && (frameData[4] & 0x1f) == 5)) {
                // 在关键帧前面加上pps和sps数据
                byte[] data = new byte[mPpsSps.length + frameData.length];
                System.arraycopy(mPpsSps, 0, data, 0, mPpsSps.length);
                System.arraycopy(frameData, 0, data, mPpsSps.length, frameData.length);
                frameData = data;
            }

            // 帧数据喂给解码器
            decoder.onFrame(frameData, 0, len);
        }

        @Override
        public void onInfo(final String info) {
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    mPreviewView.showInfo(info);
                }
            });
        }

        @Override
        public void onResolution(int width, int height) {

        }
    };

    public PreviewPresenter(Context context) {
        this.context = context;
        if (context instanceof PreviewView)
            mPreviewView = (PreviewView) context;
    }

    /** 设置渲染预览的SurfaceView */
    public void setSurfaceView(SurfaceView surfaceView) {
        this.surfaceView = surfaceView;
    }

    /** 开始预览 */
    public void startPreview() {
        if (isStartPreview) {
            Log.e(TAG, "startPreview: preview has started ");
            return;
        }

        if (surfaceView == null)
            surfaceView = mPreviewView.getSurfaceView();
        if (surfaceView == null) {
            Log.e(TAG, "surfaceView == null");
            return;
        }

        decoder = new H264Decoder(surfaceView.getHolder(), MINE_TYPE, previewWidth, previewHeight, 0);
        RTSPClient.setCallback(callback);
        // start() 阻塞，所以用子线程
        new Thread() {
            @Override
            public void run() {
                RTSPClient.start(previewAddress);
            }
        }.start();

        isStartPreview = true;
    }

    /** 停止预览 */
    public void stopPreview() {
        if (isStartPreview) {
            RTSPClient.stop();
            isStartPreview = false;
        }
        if (decoder != null)
            decoder.DecoderClose();
    }

    public void setPreviewAddress(String address) {
        this.previewAddress = address;
    }

    /** 是否正在播放预览 */
    public boolean isPlayingPreview() {
        return  isStartPreview;
    }
}
