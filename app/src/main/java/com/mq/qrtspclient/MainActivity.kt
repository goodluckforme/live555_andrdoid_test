package com.mq.qrtspclient

import android.content.Context
import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.tooling.preview.Preview
import com.mq.qrtspclient.ui.theme.Dimmen_16dp
import com.mq.qrtspclient.ui.theme.Dimmen_21dp
import com.mq.qrtspclient.ui.theme.Dimmen_6dp
import com.mq.qrtspclient.ui.theme.Dimmen_80dp
import com.mq.qrtspclient.ui.theme.Dimmen_8dp
import com.mq.qrtspclient.ui.theme.QRtspClientTheme
import java.io.File


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
                Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
                    Greeting("Android")
                }
            }
            getVersion()
        }
    }


}

@Composable
fun Greeting(name: String, modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val view = LocalView.current
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(Dimmen_16dp),
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
                val url = MainActivity.getUrl()
                Log.d("maqi", "url  :$url")
            },
            modifier = Modifier
                .padding(Dimmen_8dp)
        ) {
            Text("getUrl")
        }
    }
}


@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    QRtspClientTheme {
        Greeting("Android")
    }
}
