package com.ac42pilot.app.data

import org.json.JSONArray
import org.json.JSONObject

data class Ac42Status(
    val version: String? = null,
    val uptimeS: Long? = null,
    val freeHeap: Long? = null,
    val simulated: Boolean = false,
    val pilot: Pilot? = null,
    val instruments: Instruments? = null,
    val ap: DeviceAddress = DeviceAddress(),
    val controller: ControllerAddress = ControllerAddress(),
    val can: CanInfo = CanInfo(),
    val wifi: WifiInfo = WifiInfo(),
    val config: StatusConfig = StatusConfig(),
    val warnings: List<String> = emptyList(),
)

data class Pilot(
    val mode: String? = null,
    val modeFresh: Boolean = false,
    val engaged: Boolean = false,
    val engagedFresh: Boolean = false,
    val targetDeg: Double? = null,
    val targetRef: String? = null,
    val windTargetDeg: Double? = null,
    val windTargetSide: String? = null,
    val nfuActive: Boolean = false,
    val response: ResponseState? = null,
)

data class ResponseState(
    val valid: Boolean = false,
    val set: String? = null,
    val auto: Boolean? = null,
    val level: Int? = null,
    val low: Int? = null,
    val high: Int? = null,
    val wind: Int? = null,
)

data class Instruments(
    val headingDeg: Double? = null,
    val headingRef: String? = null,
    val headingMagDeg: Double? = null,
    val headingTrueDeg: Double? = null,
    val variationDeg: Double? = null,
    val awaDeg: Double? = null,
    val awsKn: Double? = null,
    val twaDeg: Double? = null,
    val twsKn: Double? = null,
    val twdMagDeg: Double? = null,
    val twdTrueDeg: Double? = null,
    val sogKn: Double? = null,
    val cogDeg: Double? = null,
    val cogRef: String? = null,
    val cogMagDeg: Double? = null,
    val cogTrueDeg: Double? = null,
    val depthM: Double? = null,
    val rudderDeg: Double? = null,
    val commandedRudderDeg: Double? = null,
)

data class DeviceAddress(
    val known: Boolean = false,
    val present: Boolean = false,
    val address: Int? = null,
    val addressHex: String? = null,
    val effectiveAddressHex: String? = null,
    val forced: Boolean = false,
    val override: String? = null,
    val ageMs: Long? = null,
    val model: Int? = null,
)

data class ControllerAddress(
    val present: Boolean = false,
    val address: Int? = null,
    val addressHex: String? = null,
    val forced: Boolean = false,
    val override: String? = null,
    val ageMs: Long? = null,
)

data class CanInfo(
    val ready: Boolean = false,
    val state: String? = null,
    val bitrate: Int? = null,
    val rxFrames: Long? = null,
    val txFrames: Long? = null,
    val txErrors: Long? = null,
    val busErrors: Long? = null,
    val lastRxAgeMs: Long? = null,
    val txErrorCounter: Int? = null,
    val rxErrorCounter: Int? = null,
)

data class WifiInfo(
    val mode: String? = null,
    val apStarted: Boolean = false,
    val apSsid: String? = null,
    val apIp: String? = null,
    val staConnected: Boolean = false,
    val staConfigured: Boolean = false,
    val fallbackAp: Boolean = false,
    val staIp: String? = null,
    val hostname: String? = null,
    val rssi: Int? = null,
    val clients: Int? = null,
)

data class StatusConfig(
    val controllerOverride: String? = null,
    val apOverride: String? = null,
    val rudderTimeoutMs: Long? = null,
    val rebootPending: Boolean = false,
)

data class DeviceSettings(
    val apSsid: String = "",
    val apPasswordSet: Boolean = false,
    val staSsid: String = "",
    val staPasswordSet: Boolean = false,
    val hostname: String = "ac42-pilot",
    val controllerOverride: String = "auto",
    val apOverride: String = "auto",
    val rudderTimeoutMs: Int = 1000,
    val apIp: String = "",
    val staIp: String = "",
    val wifiMode: String = "",
    val staConnected: Boolean = false,
    val apStarted: Boolean = false,
    val fallbackAp: Boolean = false,
    val rebootPending: Boolean = false,
    val mac: String = "",
    val version: String = "",
)

data class SettingsUpdate(
    val apSsid: String,
    val apPassword: String?,
    val staSsid: String,
    val staPassword: String?,
    val hostname: String,
    val controllerOverride: String,
    val apOverride: String,
    val rudderTimeoutMs: Int,
)

data class CommandResponse(
    val ok: Boolean = true,
    val message: String = "",
    val frames: Int? = null,
    val rebootRequired: Boolean = false,
    val rebootPending: Boolean = false,
)

data class LogEntry(
    val ms: Long,
    val text: String,
)

fun parseStatus(text: String): Ac42Status = parseStatus(JSONObject(text))

fun parseStatus(root: JSONObject): Ac42Status {
    val pilotObj = root.optJSONObject("pilot")
    val instObj = root.optJSONObject("instruments")
    return Ac42Status(
        version = root.stringOrNull("version"),
        uptimeS = root.longOrNull("uptime_s"),
        freeHeap = root.longOrNull("free_heap"),
        simulated = root.optBoolean("simulated", false),
        pilot = pilotObj?.let(::parsePilot),
        instruments = instObj?.let(::parseInstruments),
        ap = root.optJSONObject("ap")?.let(::parseDeviceAddress) ?: DeviceAddress(),
        controller = root.optJSONObject("controller")?.let(::parseControllerAddress) ?: ControllerAddress(),
        can = root.optJSONObject("can")?.let(::parseCanInfo) ?: CanInfo(),
        wifi = root.optJSONObject("wifi")?.let(::parseWifiInfo) ?: WifiInfo(),
        config = root.optJSONObject("config")?.let(::parseStatusConfig) ?: StatusConfig(),
        warnings = root.optJSONArray("warnings").strings(),
    )
}

private fun parsePilot(obj: JSONObject): Pilot = Pilot(
    mode = obj.stringOrNull("mode"),
    modeFresh = obj.optBoolean("mode_fresh", false),
    engaged = obj.optBoolean("engaged", false),
    engagedFresh = obj.optBoolean("engaged_fresh", false),
    targetDeg = obj.doubleOrNull("target_deg"),
    targetRef = obj.stringOrNull("target_ref"),
    windTargetDeg = obj.doubleOrNull("wind_target_deg"),
    windTargetSide = obj.stringOrNull("wind_target_side"),
    nfuActive = obj.optBoolean("nfu_active", false),
    response = obj.optJSONObject("response")?.let(::parseResponse),
)

private fun parseResponse(obj: JSONObject): ResponseState = ResponseState(
    valid = obj.optBoolean("valid", false),
    set = obj.stringOrNull("set"),
    auto = obj.booleanOrNull("auto"),
    level = obj.intOrNull("level"),
    low = obj.intOrNull("low"),
    high = obj.intOrNull("high"),
    wind = obj.intOrNull("wind"),
)

private fun parseInstruments(obj: JSONObject): Instruments = Instruments(
    headingDeg = obj.doubleOrNull("heading_deg"),
    headingRef = obj.stringOrNull("heading_ref"),
    headingMagDeg = obj.doubleOrNull("heading_mag_deg"),
    headingTrueDeg = obj.doubleOrNull("heading_true_deg"),
    variationDeg = obj.doubleOrNull("variation_deg"),
    awaDeg = obj.doubleOrNull("awa_deg"),
    awsKn = obj.doubleOrNull("aws_kn"),
    twaDeg = obj.doubleOrNull("twa_deg"),
    twsKn = obj.doubleOrNull("tws_kn"),
    twdMagDeg = obj.doubleOrNull("twd_mag_deg"),
    twdTrueDeg = obj.doubleOrNull("twd_true_deg"),
    sogKn = obj.doubleOrNull("sog_kn"),
    cogDeg = obj.doubleOrNull("cog_deg"),
    cogRef = obj.stringOrNull("cog_ref"),
    cogMagDeg = obj.doubleOrNull("cog_mag_deg"),
    cogTrueDeg = obj.doubleOrNull("cog_true_deg"),
    depthM = obj.doubleOrNull("depth_m"),
    rudderDeg = obj.doubleOrNull("rudder_deg"),
    commandedRudderDeg = obj.doubleOrNull("commanded_rudder_deg"),
)

private fun parseDeviceAddress(obj: JSONObject): DeviceAddress = DeviceAddress(
    known = obj.optBoolean("known", false),
    present = obj.optBoolean("present", false),
    address = obj.intOrNull("address"),
    addressHex = obj.stringOrNull("address_hex"),
    effectiveAddressHex = obj.stringOrNull("effective_address_hex"),
    forced = obj.optBoolean("forced", false),
    override = obj.stringOrNull("override"),
    ageMs = obj.longOrNull("age_ms"),
    model = obj.intOrNull("model"),
)

private fun parseControllerAddress(obj: JSONObject): ControllerAddress = ControllerAddress(
    present = obj.optBoolean("present", false),
    address = obj.intOrNull("address"),
    addressHex = obj.stringOrNull("address_hex"),
    forced = obj.optBoolean("forced", false),
    override = obj.stringOrNull("override"),
    ageMs = obj.longOrNull("age_ms"),
)

private fun parseCanInfo(obj: JSONObject): CanInfo = CanInfo(
    ready = obj.optBoolean("ready", false),
    state = obj.stringOrNull("state"),
    bitrate = obj.intOrNull("bitrate"),
    rxFrames = obj.longOrNull("rx_frames"),
    txFrames = obj.longOrNull("tx_frames"),
    txErrors = obj.longOrNull("tx_errors"),
    busErrors = obj.longOrNull("bus_errors"),
    lastRxAgeMs = obj.longOrNull("last_rx_age_ms"),
    txErrorCounter = obj.intOrNull("tx_error_counter"),
    rxErrorCounter = obj.intOrNull("rx_error_counter"),
)

private fun parseWifiInfo(obj: JSONObject): WifiInfo = WifiInfo(
    mode = obj.stringOrNull("mode"),
    apStarted = obj.optBoolean("ap_started", false),
    apSsid = obj.stringOrNull("ap_ssid"),
    apIp = obj.stringOrNull("ap_ip"),
    staConnected = obj.optBoolean("sta_connected", false),
    staConfigured = obj.optBoolean("sta_configured", false),
    fallbackAp = obj.optBoolean("fallback_ap", false),
    staIp = obj.stringOrNull("sta_ip"),
    hostname = obj.stringOrNull("hostname"),
    rssi = obj.intOrNull("rssi"),
    clients = obj.intOrNull("clients"),
)

private fun parseStatusConfig(obj: JSONObject): StatusConfig = StatusConfig(
    controllerOverride = obj.stringOrNull("controller_override"),
    apOverride = obj.stringOrNull("ap_override"),
    rudderTimeoutMs = obj.longOrNull("rudder_timeout_ms"),
    rebootPending = obj.optBoolean("reboot_pending", false),
)

fun parseSettings(text: String): DeviceSettings {
    val obj = JSONObject(text)
    return DeviceSettings(
        apSsid = obj.optString("ap_ssid", ""),
        apPasswordSet = obj.optBoolean("ap_password_set", false),
        staSsid = obj.optString("sta_ssid", ""),
        staPasswordSet = obj.optBoolean("sta_password_set", false),
        hostname = obj.optString("hostname", "ac42-pilot"),
        controllerOverride = obj.optString("controller_override", "auto"),
        apOverride = obj.optString("ap_override", "auto"),
        rudderTimeoutMs = obj.optInt("rudder_timeout_ms", 1000),
        apIp = obj.optString("ap_ip", ""),
        staIp = obj.optString("sta_ip", ""),
        wifiMode = obj.optString("wifi_mode", ""),
        staConnected = obj.optBoolean("sta_connected", false),
        apStarted = obj.optBoolean("ap_started", false),
        fallbackAp = obj.optBoolean("fallback_ap", false),
        rebootPending = obj.optBoolean("reboot_pending", false),
        mac = obj.optString("mac", ""),
        version = obj.optString("version", ""),
    )
}

fun parseCommandResponse(text: String): CommandResponse {
    val obj = JSONObject(text)
    return CommandResponse(
        ok = obj.optBoolean("ok", true),
        message = obj.optString("message", obj.optString("error", "")),
        frames = obj.intOrNull("frames"),
        rebootRequired = obj.optBoolean("reboot_required", false),
        rebootPending = obj.optBoolean("reboot_pending", false),
    )
}

fun parseLog(text: String): List<LogEntry> {
    val array = JSONArray(text)
    return buildList(array.length()) {
        for (i in 0 until array.length()) {
            val item = array.optJSONObject(i) ?: continue
            add(LogEntry(item.optLong("ms", 0), item.optString("t", "")))
        }
    }
}

private fun JSONObject.stringOrNull(key: String): String? =
    if (!has(key) || isNull(key)) null else optString(key).takeIf { it.isNotEmpty() }

private fun JSONObject.doubleOrNull(key: String): Double? =
    if (!has(key) || isNull(key)) null else optDouble(key, Double.NaN).takeIf { it.isFinite() }

private fun JSONObject.intOrNull(key: String): Int? =
    if (!has(key) || isNull(key)) null else optInt(key, Int.MIN_VALUE).takeIf { it != Int.MIN_VALUE }

private fun JSONObject.longOrNull(key: String): Long? =
    if (!has(key) || isNull(key)) null else optLong(key, Long.MIN_VALUE).takeIf { it != Long.MIN_VALUE }

private fun JSONObject.booleanOrNull(key: String): Boolean? =
    if (!has(key) || isNull(key)) null else optBoolean(key)

private fun JSONArray?.strings(): List<String> {
    if (this == null) return emptyList()
    return buildList(length()) {
        for (i in 0 until length()) {
            val value = optString(i, "")
            if (value.isNotEmpty()) add(value)
        }
    }
}
