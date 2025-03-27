package com.mq.qrtspclient

import android.annotation.SuppressLint
import android.graphics.SurfaceTexture
import android.media.MediaCodec
import android.os.Bundle
import android.util.Log
import android.view.Surface
import android.view.TextureView
import android.view.ViewGroup
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.updateLayoutParams
import com.mq.qrtspclient.ui.theme.Dimmen_20dp
import com.mq.qrtspclient.ui.theme.Dimmen_8dp
import com.mq.qrtspclient.ui.theme.QRtspClientTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import java.io.File
import java.io.FileInputStream
import java.lang.Thread.sleep
import java.nio.ByteBuffer


class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            QRtspClientTheme {
                // A surface container using the 'background' color from the theme
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    Greeting()
                }
            }
        }
        RTSPClient.getVersion()
    }
}

@SuppressLint("UnrememberedMutableState")
@Composable
fun Greeting() {
    val context = LocalContext.current
    var url by mutableStateOf("getUrl")
//    val view = LocalView.current
    Column(
        modifier = Modifier
            .fillMaxSize(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text("rtsp client")
        Button(
            onClick = {
                Thread {
                    Log.d("maqi", "Button clicked!")
                    val file = File(context.getExternalFilesDir(null), "test.h264")
                    Log.d("maqi", "file " + file.path)
                    val ret = RTSPClient.start(file.absolutePath)
                    Log.d("maqi", "start  ret:$ret")
                }.start()
            },
            modifier = Modifier
                .padding(Dimmen_8dp)
        ) {
            Text("start url")
        }

        Button(
            onClick = {
                MainScope().launch {
                    url = RTSPClient.getUrl()
                    Log.d("maqi", "url  :$url")
                }
            },
            modifier = Modifier
                .padding(Dimmen_8dp)
        ) {
            Text(url)
        }


        VideoPlayerScreen()
    }
}

@Composable
fun VideoPlayerScreen() {
    var decoder: H264Decoder? by remember { mutableStateOf(null) }
    val context = LocalContext.current
    Column(
        modifier = Modifier
            .fillMaxSize(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Button(
            onClick = {
                GlobalScope.launch(Dispatchers.IO) {
                    RTSPClient.startStream(
                        "rtsp://77.110.228.219/axis-media/media.amp",
//                        "rtsp://192.168.3.54:8554/v",
//                        "rtsp://192.168.3.54:8554/mystream",
                        object : FrameCallback {
                            override fun onFrameReceived(data: ByteArray?, timestampMs: Long) {
                                Log.d("maqi", "data[0]  :${data!!.size} timestampMs  ：${timestampMs} ")
                                Log.d("maqi", "data[0]  ~data[10] ${ data[0]},${ data[1]},${ data[2]},${ data[3]}")
//                                val ret = PpsSps.makeSpsPps(data)
                                decoder!!.decodeFrame(data, 0, data.size, System.nanoTime() / 1000)
                            }

                            override fun onError(code: Int, message: String?) {
                                Log.d("maqi", "code  :${code} message  ：${message} ")
                            }
                        })
                }
            },
            modifier = Modifier
                .padding(Dimmen_8dp)
        ) {
            Text("先 start开启服务，然后点此获取流数据")
        }

        Button(
            onClick = {
                GlobalScope.launch(Dispatchers.IO) {
                    RTSPClient.stopStream()
                }
            },
            modifier = Modifier
                .padding(Dimmen_8dp)
        ) {
            Text("销毁")
        }
        Button(
            onClick = {
                GlobalScope.launch(Dispatchers.IO) {
                    val file = File(context.getExternalFilesDir(null), "test.h264")
                    decodeH264File(file.absolutePath, decoder!!)
                }.start()
            },
            modifier = Modifier
                .padding(Dimmen_8dp)
        ) {
            Text("解码本地文件")
        }
        H264PlayerView({ surface ->
//            decoder = H264Decoder(surface, 1920, 1080).apply {
//                startDecoder()
//            }
            decoder = H264Decoder(surface, 1280, 800).apply {
                startDecoder()
            }
        }, { surface ->
            decoder!!.stopDecoder()
        })
    }

}

/**
 * 开始解码文件
 * @param filePath 文件路径
 * @param decoder H264 解码器
 * @param frameRate 视频帧率
 */
fun decodeH264File(filePath: String, decoder: H264Decoder, frameRate: Int = 25) {
    val file = File(filePath)
    val inputStream = FileInputStream(file)
    val buffer = ByteArray(1024 * 64)
    // 计算两帧之间的时间间隔（毫秒）
    val frameInterval = (1000.0 / frameRate).toLong()
    var lastDecodeTime = System.currentTimeMillis()
    while (true) {
        val bytesRead = inputStream.read(buffer)
        if (bytesRead == -1) break // 读取完
        // 计算需要延时的时间
        val currentTime = System.currentTimeMillis()
        val elapsedTime = currentTime - lastDecodeTime
        if (elapsedTime < frameInterval) {
            sleep(frameInterval - elapsedTime)
        }
        Log.d("maqi", "buffer[0]  :${buffer!!.size} ")
        Log.d("maqi", "buffer[0]  ~buffer[10] ${ buffer[0]},${ buffer[1]},${ buffer[2]},${ buffer[3]}")
        decoder.decodeFrame(buffer, 0, bytesRead, System.nanoTime() / 1000)
        lastDecodeTime = System.currentTimeMillis()
    }
    inputStream.close()
}


@Composable
fun H264PlayerView(onSurfaceAvailable: (Surface) -> Unit, onSurfaceDestroyed: (Surface) -> Unit) {
    AndroidView(
        factory = { context ->
            TextureView(context).apply {
                surfaceTextureListener = object : TextureView.SurfaceTextureListener {
                    var surface: Surface? = null
                    override fun onSurfaceTextureAvailable(
                        surfaceTexture: SurfaceTexture,
                        videoWidth: Int,
                        videoHeight: Int
                    ) {
                        updateLayoutParams<ViewGroup.LayoutParams> {
                            width = resources.displayMetrics.widthPixels
//                            height = (1f * width / 1920 * 1080).toInt()
                            height = (1f * width / 1280 * 800).toInt()
                            Logger.d("maqi", "width:" + width)
                            Logger.d("maqi", "height:" + height)
                        }
                        surface = Surface(surfaceTexture)
                        onSurfaceAvailable(surface!!)
                    }

                    override fun onSurfaceTextureSizeChanged(
                        surface: SurfaceTexture,
                        width: Int,
                        height: Int
                    ) {
                        Logger.d("maqi", "width:" + width)
                        Logger.d("maqi", "height:" + height)
                    }

                    override fun onSurfaceTextureDestroyed(surfaceTexture: SurfaceTexture): Boolean {
                        onSurfaceDestroyed(surface!!)
                        return true
                    }

                    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {}
                }
            }
        },
        modifier = Modifier
            .fillMaxSize()
            .padding(Dimmen_20dp)
            .wrapContentSize(Alignment.Center)
    )
}


@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    QRtspClientTheme {
        Greeting()
    }
}
