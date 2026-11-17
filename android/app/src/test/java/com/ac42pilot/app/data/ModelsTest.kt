package com.ac42pilot.app.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ModelsTest {
    @Test
    fun parsesStatusPayload() {
        val json = """
            {
              "ok": true,
              "version": "0.1.0",
              "uptime_s": 3600,
              "free_heap": 180000,
              "simulated": false,
              "pilot": {
                "mode": "Wind",
                "mode_fresh": true,
                "engaged": true,
                "engaged_fresh": true,
                "target_deg": 322.5,
                "target_ref": "M",
                "wind_target_deg": 34.0,
                "wind_target_side": "stbd",
                "nfu_active": false,
                "response": {
                  "valid": true,
                  "set": "low",
                  "auto": true,
                  "level": 4,
                  "low": 4,
                  "high": 2,
                  "wind": 3
                }
              },
              "instruments": {
                "heading_deg": 318.2,
                "heading_ref": "M",
                "awa_deg": 33.8,
                "aws_kn": 14.1,
                "twa_deg": 40.2,
                "tws_kn": 11.7,
                "sog_kn": 6.2,
                "cog_deg": 317.2,
                "rudder_deg": -2.1
              },
              "ap": {
                "known": true,
                "present": true,
                "address": 2,
                "address_hex": "0x02",
                "forced": false,
                "override": "auto",
                "age_ms": 120
              },
              "controller": {
                "present": true,
                "address": 7,
                "address_hex": "0x07",
                "forced": false,
                "override": "auto",
                "age_ms": 90
              },
              "can": {
                "ready": true,
                "state": "running",
                "rx_frames": 18432,
                "tx_frames": 24,
                "last_rx_age_ms": 60
              },
              "wifi": {
                "mode": "AP",
                "ap_started": true,
                "ap_ssid": "AC42-Pilot-1A2B",
                "ap_ip": "192.168.4.1",
                "sta_connected": false,
                "sta_configured": false,
                "fallback_ap": false,
                "sta_ip": "",
                "rssi": 0
              },
              "config": {
                "controller_override": "auto",
                "ap_override": "auto",
                "rudder_timeout_ms": 1000,
                "reboot_pending": false
              },
              "warnings": []
            }
        """.trimIndent()

        val status = parseStatus(json)
        assertEquals("Wind", status.pilot?.mode)
        assertTrue(status.pilot?.engaged == true)
        assertEquals(322.5, status.pilot?.targetDeg ?: 0.0, 0.001)
        assertEquals(318.2, status.instruments?.headingDeg ?: 0.0, 0.001)
        assertEquals(4, status.pilot?.response?.level)
        assertEquals("0x02", status.ap.addressHex)
        assertEquals("0x07", status.controller.addressHex)
        assertTrue(status.can.ready)
        assertEquals("AP", status.wifi.mode)
        assertFalse(status.config.rebootPending)
        assertTrue(status.warnings.isEmpty())
    }

    @Test
    fun parsesSettingsAndLog() {
        val settings = parseSettings(
            """
            {
              "ok": true,
              "ap_ssid": "AC42-Pilot-1A2B",
              "ap_password_set": true,
              "sta_ssid": "",
              "sta_password_set": false,
              "hostname": "ac42-pilot",
              "controller_override": "auto",
              "ap_override": "0x02",
              "rudder_timeout_ms": 1200,
              "ap_ip": "192.168.4.1",
              "sta_ip": "",
              "wifi_mode": "AP",
              "sta_connected": false,
              "ap_started": true,
              "fallback_ap": false,
              "reboot_pending": false,
              "mac": "AA:BB:CC:DD:EE:FF",
              "version": "0.1.0"
            }
            """.trimIndent()
        )
        assertEquals("AC42-Pilot-1A2B", settings.apSsid)
        assertEquals("0x02", settings.apOverride)
        assertEquals(1200, settings.rudderTimeoutMs)

        val log = parseLog(
            """[{"ms":1200,"t":"CAN prêt"},{"ms":3400,"t":"Pupitre détecté"}]"""
        )
        assertEquals(2, log.size)
        assertEquals("CAN prêt", log[0].text)
        assertNotNull(log[1].text)
    }

    @Test
    fun parsesCommandResponse() {
        val response = parseCommandResponse(
            """{"ok":true,"message":"Mode appliqué","frames":2,"reboot_required":true,"reboot_pending":true}"""
        )
        assertTrue(response.ok)
        assertEquals("Mode appliqué", response.message)
        assertEquals(2, response.frames)
        assertTrue(response.rebootRequired)
        assertTrue(response.rebootPending)
    }
}
