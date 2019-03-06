# android_live555_rtsp_client

## 描述
- 使用 Live555 库，实现 Android 平台下 rtsp 客户端

## 参考

jdpxiaoming 参考 Live555 例程实现 RtspLive555 ，笔者再基于 RtspLive555 项目优化
- https://github.com/jdpxiaoming/RtspLive555
- live555 官方例程 testRTSPClient

## 优化点

- 修复视频下半部花屏的问题
    > 增加缓冲区 DUMMY_SINK_RECEIVE_BUFFER_SIZE 的大小，笔者测试用 1080P 视频，
    > 缓冲区设置为 50 * 10000
    
    
- 修复读帧函数 DummySink::afterGettingFrame() 没有收到 sps 和 pps，使用了默认的 sps 和 pps，引起了绿屏的问题
    > 从sdp中解析出 sps 和 pps。
    > sdp 是 rtsp 初始化过程当中可以获取到的一个字串，包含很多明文的信息，也包括了 base64 加密的 sps 和 pps。
    > 以下函数被回调
    ```
    // Implementation of the RTSP 'response handlers':
    void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode, char *resultString);
    ```
    > resultString 即为 sdp。从 sdp 解析出 sps 和 pps 的代码，请查看 RTSPClient.cpp 文件中的函数 get_sps_pps_from_sdp()。
    > 原理参考 RTSPClient.cpp 中的注释。
   
## 知识点总结
- sps 和 pps 包含了解码的关键信息，必须先把 sps 和 pps 这两种帧数据喂给解码器，才能解码成功
- rtsp 服务器有可能，不会把 sps 和 pps 帧打包进码流当中，需要我们从 sdp 中解析出来
- 网上的例程中，一般会有默认的 sps 和 pps ，其实是针对他们当时调试的编码平台，并不具有通用性

---


作者：黎耀棠   邮箱： 316477169@qq.com
欢迎交流学习
