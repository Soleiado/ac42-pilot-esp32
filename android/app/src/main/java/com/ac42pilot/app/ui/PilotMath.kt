package com.ac42pilot.app.ui

import com.ac42pilot.app.data.Ac42Status
import com.ac42pilot.app.data.ResponseState
import kotlin.math.abs
import kotlin.math.roundToInt
import kotlin.math.roundToLong

data class PilotView(
    val modeKey: String,
    val modeLabel: String,
    val engaged: Boolean,
    val heading: Double?,
    val target: Double?,
    val targetRef: String?,
    val windTarget: Double?,
    val windSide: String?,
    val awa: Double?,
    val response: ResponseState?,
    val nfuActive: Boolean,
)

fun wrap360(value: Double): Double {
    var v = value % 360.0
    if (v < 0) v += 360.0
    return v
}

fun wrap180(value: Double): Double {
    val v = wrap360(value)
    return if (v > 180.0) v - 360.0 else v
}

fun modeKeyOf(mode: String?): String = when (mode?.lowercase()) {
    "auto" -> "auto"
    "wind" -> "wind"
    "nav" -> "nav"
    "no drift" -> "nodrift"
    "nfu" -> "nfu"
    "standby" -> "standby"
    else -> "unknown"
}

fun modeLabelOf(mode: String?, engaged: Boolean, fresh: Boolean): String {
    if (!fresh) return "ÉTAT INCONNU"
    if (!engaged) return "STANDBY"
    return when (modeKeyOf(mode)) {
        "auto" -> "AUTO"
        "wind" -> "VENT"
        "nav" -> "NAV"
        "nodrift" -> "NO DRIFT"
        "nfu" -> "NFU"
        "standby" -> "STANDBY"
        else -> "—"
    }
}

fun alignedTarget(status: Ac42Status): Double? {
    var target = status.pilot?.targetDeg ?: return null
    val variation = status.instruments?.variationDeg
    val targetRef = status.pilot.targetRef
    val headingRef = status.instruments?.headingRef
    if (variation != null && targetRef != null && headingRef != null && targetRef != headingRef) {
        if (headingRef == "M" && targetRef == "T") target -= variation
        if (headingRef == "T" && targetRef == "M") target += variation
    }
    return wrap360(target)
}

fun buildPilotView(state: UiState): PilotView {
    val status = state.status
    val pilot = status?.pilot
    val inst = status?.instruments
    val heading = inst?.headingDeg ?: inst?.headingMagDeg ?: inst?.headingTrueDeg
    val target = status?.let(::alignedTarget)
    val awa = inst?.awaDeg
    val windTarget = pilot?.windTargetDeg
    val windSide = pilot?.windTargetSide
        ?: awa?.let { if (it >= 0) "stbd" else "port" }
    val engaged = pilot?.engaged == true
    return PilotView(
        modeKey = modeKeyOf(pilot?.mode),
        modeLabel = modeLabelOf(pilot?.mode, engaged, pilot?.modeFresh == true),
        engaged = engaged,
        heading = heading,
        target = target,
        targetRef = pilot?.targetRef,
        windTarget = windTarget,
        windSide = windSide,
        awa = awa,
        response = pilot?.response,
        nfuActive = state.nfuActive,
    )
}

fun bigValue(view: PilotView, connected: Boolean): Double? {
    if (!connected) return null
    if (!view.engaged) return view.heading
    if (view.modeKey == "wind") {
        return view.windTarget?.let { abs(it) } ?: view.awa?.let { abs(it) }
    }
    return view.target ?: view.heading
}

fun sideText(view: PilotView, connected: Boolean): String {
    if (!connected) return "HORS LIGNE"
    if (!view.engaged) return "CAP ACTUEL"
    if (view.modeKey == "wind") {
        return when (view.windSide) {
            "stbd" -> "VENT · TRIBORD"
            "port" -> "VENT · BÂBORD"
            else -> "ANGLE AU VENT"
        }
    }
    val ref = view.targetRef?.let { " · $it" } ?: ""
    return if (view.target != null) "CAP CIBLE$ref" else "CAP ACTUEL"
}

fun targetLabel(view: PilotView): String =
    view.target?.let { "${fmt(it, 0)}°${view.targetRef ?: ""}" } ?: "—"

fun windLabel(view: PilotView): String {
    val target = view.windTarget
    if (target != null) {
        val side = when (view.windSide) {
            "stbd" -> "TRIBORD"
            "port" -> "BÂBORD"
            else -> ""
        }
        return "${fmt(abs(target), 0)}° $side".trim()
    }
    return view.awa?.let { "${fmt(abs(it), 0)}° APP." } ?: "—"
}

fun fmt(value: Double?, digits: Int = 0): String {
    if (value == null || !value.isFinite()) return "—"
    if (digits <= 0) return value.roundToInt().toString()
    val factor = if (digits == 1) 10L else 100L
    val rounded = (value * factor).roundToLong()
    val sign = if (rounded < 0) "-" else ""
    val absolute = abs(rounded)
    return if (digits == 1) {
        "$sign${absolute / 10}.${absolute % 10}"
    } else {
        val fraction = (absolute % 100).toString().padStart(2, '0')
        "$sign${absolute / 100}.$fraction"
    }
}

fun fmtInt(value: Long?): String = value?.toString() ?: "—"

fun uptimeLabel(seconds: Long?): String {
    if (seconds == null) return "—"
    val h = seconds / 3600
    val m = (seconds % 3600) / 60
    return if (h > 0) "${h} h ${m} min" else "${m} min"
}

fun canChipState(state: UiState): String {
    val can = state.status?.can ?: return "off"
    if (!state.connected) return "off"
    val fresh = (can.lastRxAgeMs ?: Long.MAX_VALUE) < 5000
    return when {
        can.ready && fresh -> "ok"
        can.ready -> "warn"
        else -> "warn"
    }
}

fun controllerChipState(state: UiState): String {
    if (!state.connected) return "off"
    return if (state.status?.controller?.present == true) "ok" else "off"
}

fun apChipState(state: UiState): String {
    if (!state.connected) return "off"
    val ap = state.status?.ap ?: return "off"
    return when {
        ap.present -> "ok"
        ap.known || ap.forced -> "warn"
        else -> "off"
    }
}

fun wifiAddress(state: UiState): String? {
    val wifi = state.status?.wifi ?: return null
    return if (wifi.staConnected) wifi.staIp else wifi.apIp
}
