package com.android.bba.util

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.core.content.FileProvider
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.net.URL

class AppUpdate(
    val versionCode: Int,
    val tag: String,
    val apkUrl: String,
    val apkName: String,
) {
    companion object {
        private const val LATEST = "https://api.github.com/repos/openbba0/bba/releases/latest"

        fun fetchLatest(): AppUpdate {
            val conn = open(LATEST, api = true)
            try {
                val text = conn.inputStream.bufferedReader().use { it.readText() }
                val json = JSONObject(text)
                val tag = json.getString("tag_name")
                val code = tag.trim().removePrefix("V").removePrefix("v").toInt()
                val assets = json.getJSONArray("assets")
                val preferred = "com.android.bba_${code}_release.apk"
                var url: String? = null
                var name: String? = null
                for (i in 0 until assets.length()) {
                    val asset = assets.getJSONObject(i)
                    val assetName = asset.getString("name")
                    if (!assetName.endsWith(".apk")) continue
                    if (url == null || assetName == preferred) {
                        url = asset.getString("browser_download_url")
                        name = assetName
                    }
                }
                if (url == null || name == null) error("release has no apk")
                return AppUpdate(code, tag, url, name)
            } finally {
                conn.disconnect()
            }
        }

        fun download(update: AppUpdate, dest: File, onProgress: (Int) -> Unit) {
            dest.parentFile?.mkdirs()
            val part = File(dest.parentFile, dest.name + ".part")
            val conn = open(update.apkUrl)
            try {
                val total = conn.contentLengthLong
                conn.inputStream.use { input ->
                    part.outputStream().use { output ->
                        val buf = ByteArray(16 * 1024)
                        var got = 0L
                        var last = -1
                        while (true) {
                            val n = input.read(buf)
                            if (n < 0) break
                            output.write(buf, 0, n)
                            got += n
                            if (total > 0) {
                                val pct = ((got * 100) / total).toInt().coerceIn(0, 100)
                                if (pct != last) {
                                    last = pct
                                    onProgress(pct)
                                }
                            } else if (last < 0) {
                                last = 0
                                onProgress(-1)
                            }
                        }
                    }
                }
            } finally {
                conn.disconnect()
            }
            if (dest.exists() && !dest.delete()) error("replace failed")
            if (!part.renameTo(dest)) error("save failed")
        }

        fun canInstall(context: Context): Boolean {
            return Build.VERSION.SDK_INT < Build.VERSION_CODES.O ||
                context.packageManager.canRequestPackageInstalls()
        }

        fun openInstallPermission(context: Context) {
            val intent = Intent(
                Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                Uri.parse("package:${context.packageName}"),
            )
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            context.startActivity(intent)
        }

        fun install(context: Context, file: File) {
            val uri = FileProvider.getUriForFile(context, "${context.packageName}.updates", file)
            val intent = Intent(Intent.ACTION_VIEW).apply {
                setDataAndType(uri, "application/vnd.android.package-archive")
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            }
            context.startActivity(intent)
        }

        private fun open(url: String, api: Boolean = false): HttpURLConnection {
            val conn = (URL(url).openConnection() as HttpURLConnection).apply {
                instanceFollowRedirects = true
                connectTimeout = 15_000
                readTimeout = 60_000
                setRequestProperty("User-Agent", "BBA")
                if (api) {
                    setRequestProperty("Accept", "application/vnd.github+json")
                }
            }
            conn.connect()
            if (conn.responseCode == HttpURLConnection.HTTP_OK) return conn
            val err = conn.errorStream?.bufferedReader()?.use { it.readText() }.orEmpty()
            val code = conn.responseCode
            conn.disconnect()
            error("HTTP $code $err")
        }
    }
}
