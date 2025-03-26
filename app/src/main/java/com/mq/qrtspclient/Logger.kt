package com.mq.qrtspclient

import android.util.Log

object Logger {
    /**
     * ERROR
     */
    fun e(clazz: Class<*>, msg: String) {
        if (true) {
            Log.e(clazz.simpleName, msg + "")
        }
    }

    fun e(tag: String?, msg: String) {
        if (true) {
            Log.e(tag, msg + "")
        }
    }

    /**
     * WARN
     */
    fun w(clazz: Class<*>, msg: String) {
        if (true) {
            Log.w(clazz.simpleName, msg + "")
        }
    }

    fun w(tag: String?, msg: String) {
        if (true) {
            Log.w(tag, msg + "")
        }
    }

    /**
     * INFO
     */
    fun i(clazz: Class<*>, msg: String) {
        if (true) {
            Log.i(clazz.simpleName, msg + "")
        }
    }

    fun i(tag: String?, msg: String) {
        if (true) {
            Log.i(tag, msg + "")
        }
    }

    /**
     * DEBUG
     */
    fun d(clazz: Class<*>, msg: String) {
        if (true) {
            Log.d(clazz.simpleName, msg + "")
        }
    }

    fun d(tag: String?, msg: String) {
        if (true) {
            Log.d(tag, msg + "")
        }
    }

    /**
     * VERBOSE
     */
    fun v(clazz: Class<*>, msg: String) {
        if (true) {
            Log.v(clazz.simpleName, msg + "")
        }
    }

    fun v(tag: String?, msg: String) {
        if (true) {
            Log.v(tag, msg + "")
        }
    }
}
