package com.ac42pilot.app.data

import android.content.Context

class SettingsStore(context: Context) {
    private val prefs = context.getSharedPreferences("ac42_pilot", Context.MODE_PRIVATE)

    var baseUrl: String
        get() = prefs.getString("base_url", Ac42Api.DEFAULT_BASE_URL)
            ?.takeIf { it.isNotBlank() } ?: Ac42Api.DEFAULT_BASE_URL
        set(value) {
            prefs.edit().putString("base_url", Ac42Api.normalizeBaseUrl(value)).apply()
        }
}
