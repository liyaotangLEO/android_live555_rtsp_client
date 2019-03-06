package com.rtsp.poe.rtsplive555;

import android.view.SurfaceView;

/**
 * @author liyaotang
 * @date 2019/1/23
 */
public interface PreviewView {

    SurfaceView getSurfaceView();

    void showInfo(String info);

}
