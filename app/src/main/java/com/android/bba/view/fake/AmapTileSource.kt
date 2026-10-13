package com.android.bba.view.fake

import org.osmdroid.tileprovider.tilesource.OnlineTileSourceBase
import org.osmdroid.util.MapTileIndex

/** Chinese road map. Tile indexes are GCJ-02, not WGS-84. */
internal class AmapTileSource : OnlineTileSourceBase(
    "Amap",
    3,
    18,
    256,
    "",
    arrayOf(
        "https://webrd01.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8",
        "https://webrd02.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8",
        "https://webrd03.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8",
        "https://webrd04.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8",
    ),
) {
    override fun getTileURLString(pMapTileIndex: Long): String {
        return baseUrl +
            "&x=" + MapTileIndex.getX(pMapTileIndex) +
            "&y=" + MapTileIndex.getY(pMapTileIndex) +
            "&z=" + MapTileIndex.getZoom(pMapTileIndex)
    }
}
