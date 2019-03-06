package com.rtsp.poe.rtsplive555;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;

/**
 * @author liyaotang
 * @date 2019/1/23
 */
public class SecondActivity extends AppCompatActivity implements PreviewView {

    private PreviewPresenter mPreviewPresenter;

    /** 预览渲染的View */
    private SurfaceView mSurfaceView;


    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_second);

        initView();

        mPreviewPresenter = new PreviewPresenter(this);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }

    private void initView() {
        mSurfaceView = findViewById(R.id.surface_view);
        mSurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                mPreviewPresenter.setPreviewAddress("rtsp://192.168.1.127/stream1");
//                mPreviewPresenter.setPreviewAddress("rtsp://192.168.1.108/media/live/1/1");
                mPreviewPresenter.startPreview();
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                mPreviewPresenter.stopPreview();
            }
        });

        EditText et = findViewById(R.id.et_url);
        et.setText(RTSPClient.RTSP_PATH);
    }

    public void startPreview(View view) {
        EditText ed = findViewById(R.id.et_url);
        mPreviewPresenter.setPreviewAddress(ed.getText().toString());
        mPreviewPresenter.startPreview();
    }

    public void stopPreview(View view) {
        mPreviewPresenter.stopPreview();
    }

    @Override
    public SurfaceView getSurfaceView() {
        return mSurfaceView;
    }

    @Override
    public void showInfo(String info) {
        Toast.makeText(this, info, Toast.LENGTH_SHORT).show();
    }
}
