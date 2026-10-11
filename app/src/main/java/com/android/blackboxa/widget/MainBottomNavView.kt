package com.android.blackboxa.widget

import android.content.Context
import android.content.res.ColorStateList
import android.graphics.drawable.GradientDrawable
import android.os.Parcel
import android.os.Parcelable
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.widget.PopupMenu
import androidx.core.content.ContextCompat
import androidx.core.graphics.ColorUtils
import androidx.core.widget.ImageViewCompat
import com.android.blackboxa.R

class MainBottomNavView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : LinearLayout(context, attrs) {

    private var selectedId = 0
    var onTabSelected: ((Int) -> Unit)? = null

    private val tabColors = mapOf(
        R.id.nav_apps to R.color.nav_apps,
        R.id.nav_setting to R.color.nav_setting,
        R.id.nav_me to R.color.nav_me,
    )

    init {
        orientation = HORIZONTAL
        val popup = PopupMenu(context, this)
        popup.inflate(R.menu.bottom_nav)
        val menu = popup.menu
        val inflater = LayoutInflater.from(context)
        for (i in 0 until menu.size()) {
            val item = menu.getItem(i)
            val tab = inflater.inflate(R.layout.item_main_nav_tab, this, false)
            val icon = tab.findViewById<ImageView>(R.id.icon)
            icon.setImageDrawable(item.icon)
            tab.findViewById<TextView>(R.id.label).text = item.title
            tab.contentDescription = item.title
            tab.tag = item.itemId
            tab.setOnClickListener {
                if (item.itemId != selectedId) {
                    select(item.itemId, notify = true)
                }
            }
            addView(tab, LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f))
        }
        if (childCount > 0) {
            select(getChildAt(0).tag as Int, notify = false)
        }
    }

    fun selectedTabId(): Int = selectedId

    fun select(id: Int, notify: Boolean) {
        selectedId = id
        val muted = ContextCompat.getColor(context, R.color.secondary_text)
        for (i in 0 until childCount) {
            val child = getChildAt(i)
            val selected = child.tag == id
            child.isSelected = selected
            val colorRes = tabColors[child.tag] ?: R.color.nav_apps
            val color = ContextCompat.getColor(context, colorRes)
            val icon = child.findViewById<ImageView>(R.id.icon)
            val label = child.findViewById<TextView>(R.id.label)
            val pill = child.findViewById<FrameLayout>(R.id.iconPill)
            ImageViewCompat.setImageTintList(icon, ColorStateList.valueOf(color))
            icon.alpha = if (selected) 1f else 0.55f
            label.setTextColor(if (selected) color else muted)
            pill.background = if (selected) colorPill(color) else null
        }
        if (notify) onTabSelected?.invoke(id)
    }

    private fun colorPill(color: Int): GradientDrawable {
        val radius = 14f * resources.displayMetrics.density
        return GradientDrawable().apply {
            cornerRadius = radius
            setColor(ColorUtils.setAlphaComponent(color, 0x2E))
        }
    }

    override fun onSaveInstanceState(): Parcelable {
        val superState = super.onSaveInstanceState()
        return SavedState(superState).also { it.selectedId = selectedId }
    }

    override fun onRestoreInstanceState(state: Parcelable?) {
        if (state is SavedState) {
            super.onRestoreInstanceState(state.superState)
            select(state.selectedId, notify = true)
        } else {
            super.onRestoreInstanceState(state)
        }
    }

    private class SavedState : BaseSavedState {
        var selectedId: Int = 0

        constructor(superState: Parcelable?) : super(superState)

        private constructor(source: Parcel) : super(source) {
            selectedId = source.readInt()
        }

        override fun writeToParcel(out: Parcel, flags: Int) {
            super.writeToParcel(out, flags)
            out.writeInt(selectedId)
        }

        companion object {
            @JvmField
            val CREATOR = object : Parcelable.Creator<SavedState> {
                override fun createFromParcel(source: Parcel) = SavedState(source)
                override fun newArray(size: Int) = arrayOfNulls<SavedState>(size)
            }
        }
    }
}
