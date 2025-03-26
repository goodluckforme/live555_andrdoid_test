package com.mq.qrtspclient

import android.annotation.SuppressLint
import android.graphics.SurfaceTexture
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
import com.mq.qrtspclient.ui.theme.Dimmen_8dp
import com.mq.qrtspclient.ui.theme.QRtspClientTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import java.io.File
import java.io.FileInputStream


class MainActivity : ComponentActivity() {
    companion object {
        init {
            System.loadLibrary("rtspclient")  // 加载生成的 .so 库
        }

        // 获取版本相关信息
        external fun getVersion()
        external fun start(filename: String): Int
        external fun getUrl(): String

    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            QRtspClientTheme {
                // A surface container using the 'background' color from the theme
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    Greeting("Android")
                }
            }
        }
        getVersion()
    }

    fun getScreenWidth(): Int {
        val displayMetrics = resources.displayMetrics
        return displayMetrics.widthPixels
    }

}

@SuppressLint("UnrememberedMutableState")
@Composable
fun Greeting(name: String, modifier: Modifier = Modifier) {
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
                    val ret = MainActivity.start(file.absolutePath)
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
                    url = MainActivity.getUrl()
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
                    val file = File(context.getExternalFilesDir(null), "test.h264")
                    decodeH264File(file.absolutePath, decoder!!)
                }
            },
            modifier = Modifier
                .padding(Dimmen_8dp)
        ) {
            Text("解码本地文件")
        }
        H264PlayerView({ surface ->
            decoder = H264Decoder(surface, 1920, 1080).apply {
                startDecoder()
            }
        }, { surface ->
            decoder!!.stopDecoder()
        })
    }

}

/**
 * 开始解码文件
 */
fun decodeH264File(filePath: String, decoder: H264Decoder) {
    val file = File(filePath)
    val inputStream = FileInputStream(file)
    val buffer = ByteArray(4096)

    while (true) {
        val bytesRead = inputStream.read(buffer)
        if (bytesRead == -1) break // 读取完成
        decoder.decodeFrame(buffer, 0, bytesRead, System.nanoTime() / 1000)
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
                            height = (1f * width / 1920 * 1080).toInt()
                            Logger.d("maqi","width:"+width)
                            Logger.d("maqi","height:"+height)
                        }
                        surface = Surface(surfaceTexture)
                        onSurfaceAvailable(surface!!)
                    }

                    override fun onSurfaceTextureSizeChanged(
                        surface: SurfaceTexture,
                        width: Int,
                        height: Int
                    ) {
                        Logger.d("maqi","width:"+width)
                        Logger.d("maqi","height:"+height)
                    }

                    override fun onSurfaceTextureDestroyed(surfaceTexture: SurfaceTexture): Boolean {
                        onSurfaceDestroyed(surface!!)
                        return true
                    }

                    override fun onSurfaceTextureUpdated(surface: SurfaceTexture) {}
                }
            }
        },
        modifier = Modifier.fillMaxSize()
    )
}


@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    QRtspClientTheme {
        Greeting("Android")
    }
}
