package com.android.bba.view.fake

import android.app.Activity
import android.os.Bundle
import android.view.inputmethod.InputMethodManager
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceManager
import com.android.bb.model.location.BLocation
import com.android.bba.databinding.ActivityOsmdroidBinding
import com.android.bba.util.inflate
import org.osmdroid.config.Configuration
import org.osmdroid.events.MapListener
import org.osmdroid.events.ScrollEvent
import org.osmdroid.events.ZoomEvent
import org.osmdroid.util.GeoPoint
import org.osmdroid.views.CustomZoomButtonsController
import java.util.Locale

class FollowMyLocationOverlay : AppCompatActivity() {

    private val binding: ActivityOsmdroidBinding by inflate()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val config = Configuration.getInstance()
        config.load(this, PreferenceManager.getDefaultSharedPreferences(this))
        config.userAgentValue = packageName
        setContentView(binding.root)

        val saved: BLocation? = intent.getParcelableExtra("location")
        val wgsLat = saved?.latitude ?: 30.2736
        val wgsLon = saved?.longitude ?: 120.1563
        val (gcjLat, gcjLon) = Gcj02.wgsToGcj(wgsLat, wgsLon)

        val map = binding.map
        map.setTileSource(AmapTileSource())
        map.setMultiTouchControls(true)
        map.isTilesScaledToDpi = true
        map.zoomController.setVisibility(CustomZoomButtonsController.Visibility.SHOW_AND_FADEOUT)
        map.controller.setZoom(16.0)
        map.controller.setCenter(GeoPoint(gcjLat, gcjLon))
        map.addMapListener(object : MapListener {
            override fun onScroll(event: ScrollEvent): Boolean {
                showCenter()
                return true
            }

            override fun onZoom(event: ZoomEvent): Boolean {
                showCenter()
                return true
            }
        })
        showCenter()
        binding.confirm.setOnClickListener {
            val center = map.mapCenter
            val (lat, lon) = Gcj02.gcjToWgs(center.latitude, center.longitude)
            finishWithResult(lat, lon)
        }
    }

    override fun onResume() {
        super.onResume()
        binding.map.onResume()
    }

    override fun onPause() {
        super.onPause()
        binding.map.onPause()
    }

    private fun showCenter() {
        val center = binding.map.mapCenter
        val (lat, lon) = Gcj02.gcjToWgs(center.latitude, center.longitude)
        binding.coord.text = String.format(Locale.US, "%.6f, %.6f", lat, lon)
    }

    private fun finishWithResult(latitude: Double, longitude: Double) {
        intent.putExtra("latitude", latitude)
        intent.putExtra("longitude", longitude)
        setResult(Activity.RESULT_OK, intent)
        val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
        window.peekDecorView()?.let { imm.hideSoftInputFromWindow(it.windowToken, 0) }
        finish()
    }
}
