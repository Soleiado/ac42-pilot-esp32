package com.ac42pilot.app.ui

import android.app.Application
import android.os.SystemClock
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.ac42pilot.app.data.Ac42Api
import com.ac42pilot.app.data.Ac42Status
import com.ac42pilot.app.data.CommandResponse
import com.ac42pilot.app.data.DeviceSettings
import com.ac42pilot.app.data.LogEntry
import com.ac42pilot.app.data.SettingsStore
import com.ac42pilot.app.data.SettingsUpdate
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

data class HeadingSample(val timeMs: Long, val deg: Float)

data class UiEvent(val message: String, val error: Boolean = false)

data class UiState(
    val baseUrl: String,
    val status: Ac42Status? = null,
    val connected: Boolean = false,
    val everConnected: Boolean = false,
    val failures: Int = 0,
    val nfuActive: Boolean = false,
    val nfuSide: String? = null,
    val commandBusy: Boolean = false,
    val settings: DeviceSettings? = null,
    val settingsLoading: Boolean = false,
    val settingsSaving: Boolean = false,
    val settingsMessage: String? = null,
    val settingsError: Boolean = false,
    val settingsRebootRequired: Boolean = false,
    val settingsRebootPending: Boolean = false,
    val log: List<LogEntry> = emptyList(),
    val headingHistory: List<HeadingSample> = emptyList(),
)

data class SettingsForm(
    val baseUrl: String,
    val apSsid: String,
    val apPassword: String = "",
    val staSsid: String,
    val staPassword: String = "",
    val hostname: String,
    val controllerOverride: String,
    val apOverride: String,
    val rudderTimeoutMs: String,
) {
    fun toUpdate(): SettingsUpdate = SettingsUpdate(
        apSsid = apSsid.trim(),
        apPassword = apPassword.takeIf { it.isNotEmpty() },
        staSsid = staSsid.trim(),
        staPassword = staPassword.takeIf { it.isNotEmpty() },
        hostname = hostname.trim(),
        controllerOverride = controllerOverride.trim().ifEmpty { "auto" },
        apOverride = apOverride.trim().ifEmpty { "auto" },
        rudderTimeoutMs = rudderTimeoutMs.toIntOrNull() ?: 1000,
    )

    companion object {
        fun from(settings: DeviceSettings, baseUrl: String): SettingsForm = SettingsForm(
            baseUrl = baseUrl,
            apSsid = settings.apSsid,
            staSsid = settings.staSsid,
            hostname = settings.hostname,
            controllerOverride = settings.controllerOverride,
            apOverride = settings.apOverride,
            rudderTimeoutMs = settings.rudderTimeoutMs.toString(),
        )
    }
}

class Ac42ViewModel(application: Application) : AndroidViewModel(application) {
    private val store = SettingsStore(application)
    private val api = Ac42Api().apply { baseUrl = store.baseUrl }

    private val _state = MutableStateFlow(UiState(baseUrl = api.baseUrl))
    val state: StateFlow<UiState> = _state.asStateFlow()

    private val _events = MutableSharedFlow<UiEvent>(extraBufferCapacity = 16)
    val events: SharedFlow<UiEvent> = _events.asSharedFlow()

    private val refreshMutex = Mutex()
    private var pollJob: Job? = null
    private var nfuJob: Job? = null
    private val headingSamples = mutableListOf<HeadingSample>()
    private var lastHeadingSampleAt = 0L

    fun startPolling() {
        if (pollJob?.isActive == true) return
        pollJob = viewModelScope.launch {
            while (isActive) {
                refresh()
                delay(900)
            }
        }
    }

    fun stopPolling() {
        pollJob?.cancel()
        pollJob = null
    }

    fun refreshNow() {
        viewModelScope.launch { refresh() }
    }

    private suspend fun refresh() {
        refreshMutex.withLock {
            try {
                val status = api.getStatus()
                updateHeadingHistory(status)
                val nfuActive = _state.value.nfuActive
                if (nfuActive && status.pilot?.engaged == true) {
                    stopNfuInternal("Pilote engagé : barre arrêtée", true)
                }
                _state.update { current ->
                    current.copy(
                        status = status,
                        connected = true,
                        everConnected = true,
                        failures = 0,
                        settingsRebootPending = status.config.rebootPending,
                    )
                }
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                _state.update { current ->
                    val failures = current.failures + 1
                    current.copy(
                        failures = failures,
                        connected = if (failures >= 2) false else current.connected,
                    )
                }
            }
        }
    }

    private fun updateHeadingHistory(status: Ac42Status) {
        val heading = status.instruments?.headingDeg
            ?: status.instruments?.headingMagDeg
            ?: status.instruments?.headingTrueDeg
            ?: return
        val now = System.currentTimeMillis()
        if (headingSamples.isEmpty()) {
            headingSamples.add(HeadingSample(now - 60_000L, heading.toFloat()))
        }
        if (lastHeadingSampleAt == 0L || now - lastHeadingSampleAt >= 2_500L) {
            headingSamples.add(HeadingSample(now, heading.toFloat()))
            lastHeadingSampleAt = now
        }
        val cutoff = now - 60_000L
        headingSamples.removeAll { it.timeMs < cutoff }
        _state.update { it.copy(headingHistory = headingSamples.toList()) }
    }

    fun connectTo(rawUrl: String) {
        val normalized = Ac42Api.normalizeBaseUrl(rawUrl)
        stopNfuInternal(null, true)
        store.baseUrl = normalized
        api.updateBaseUrl(normalized)
        headingSamples.clear()
        lastHeadingSampleAt = 0L
        _state.update {
            it.copy(
                baseUrl = normalized,
                status = null,
                connected = false,
                failures = 0,
                settings = null,
                settingsLoading = true,
                settingsMessage = "Connexion au boîtier…",
                settingsError = false,
                settingsRebootPending = false,
                settingsRebootRequired = false,
                headingHistory = emptyList(),
            )
        }
        startPolling()
        viewModelScope.launch { loadSettingsInternal() }
        viewModelScope.launch { refresh() }
    }

    fun loadSettings() {
        viewModelScope.launch { loadSettingsInternal() }
    }

    private suspend fun loadSettingsInternal() {
        _state.update {
            it.copy(settingsLoading = true, settingsMessage = "Chargement des réglages…", settingsError = false)
        }
        try {
            val settings = api.getSettings()
            _state.update {
                it.copy(
                    settings = settings,
                    settingsLoading = false,
                    settingsMessage = null,
                    settingsError = false,
                    settingsRebootPending = settings.rebootPending,
                    settingsRebootRequired = settings.rebootPending,
                    baseUrl = api.baseUrl,
                )
            }
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            _state.update {
                it.copy(
                    settingsLoading = false,
                    settingsMessage = e.message ?: "Réglages indisponibles",
                    settingsError = true,
                )
            }
        }
    }

    fun saveSettings(form: SettingsForm) {
        if (_state.value.settingsSaving) return
        viewModelScope.launch {
            _state.update {
                it.copy(
                    settingsSaving = true,
                    settingsMessage = "Enregistrement des réglages…",
                    settingsError = false,
                )
            }
            try {
                val response = api.saveSettings(form.toUpdate())
                val pending = response.rebootRequired ||
                    response.rebootPending ||
                    _state.value.settingsRebootPending
                val message = if (pending) {
                    "Réglages enregistrés. Un redémarrage est nécessaire pour appliquer les paramètres réseau."
                } else {
                    response.message.ifBlank { "Réglages enregistrés." }
                }
                _state.update {
                    it.copy(
                        settingsSaving = false,
                        settingsMessage = message,
                        settingsError = false,
                        settingsRebootRequired = pending,
                        settingsRebootPending = pending,
                    )
                }
                _events.emit(UiEvent(if (pending) "Réglages enregistrés — redémarrage requis" else "Réglages enregistrés"))
                runCatching { api.getSettings() }.onSuccess { settings ->
                    _state.update {
                        it.copy(
                            settings = settings,
                            settingsRebootPending = settings.rebootPending || pending,
                        )
                    }
                }
                refresh()
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                _state.update {
                    it.copy(
                        settingsSaving = false,
                        settingsMessage = e.message ?: "Réglages refusés",
                        settingsError = true,
                    )
                }
                _events.emit(UiEvent(e.message ?: "Réglages refusés", true))
            }
        }
    }

    fun rebootDevice() {
        viewModelScope.launch {
            try {
                api.reboot()
                _events.emit(UiEvent("Redémarrage…"))
                _state.update {
                    it.copy(connected = false, status = null, settingsRebootPending = false)
                }
            } catch (e: Exception) {
                _events.emit(UiEvent(e.message ?: "Redémarrage impossible", true))
            }
        }
    }

    fun loadLog() {
        viewModelScope.launch {
            runCatching { api.getLog() }.onSuccess { log ->
                _state.update { it.copy(log = log) }
            }
        }
    }

    fun setMode(mode: String) {
        runCommand("Mode") { api.setMode(mode) }
    }

    fun changeCourse(side: String, degrees: Int) {
        runCommand("Correction") { api.changeCourse(side, degrees) }
    }

    fun tack(side: String) {
        runCommand("Virement") { api.tack(side) }
    }

    fun cycleResponse() {
        val response = _state.value.status?.pilot?.response
        val next = when {
            response?.valid != true -> "auto"
            response.auto == true -> "low"
            response.set == "low" -> "high"
            else -> "auto"
        }
        runCommand("Réponse") { api.setResponse(next) }
    }

    private fun runCommand(label: String, block: suspend () -> CommandResponse) {
        viewModelScope.launch {
            _state.update { it.copy(commandBusy = true) }
            try {
                val response = block()
                _events.emit(UiEvent(response.message.ifBlank { label }))
                delay(250)
                refresh()
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                _events.emit(UiEvent(e.message ?: "$label refusé", true))
            } finally {
                _state.update { it.copy(commandBusy = false) }
            }
        }
    }

    fun startNfu(side: String) {
        val current = _state.value
        val pilot = current.status?.pilot
        if (!current.connected || current.status?.controller?.present != true) {
            _events.tryEmit(UiEvent("Pupitre B&G non détecté", true))
            return
        }
        if (pilot?.engaged == true) {
            _events.tryEmit(UiEvent("Barre directe uniquement en standby", true))
            return
        }
        if (nfuJob?.isActive == true) return

        _state.update { it.copy(nfuActive = true, nfuSide = side) }
        _events.tryEmit(UiEvent(if (side == "stbd") "Barre tribord" else "Barre bâbord"))
        nfuJob = viewModelScope.launch {
            val started = SystemClock.elapsedRealtime()
            try {
                while (isActive) {
                    api.rudder(side)
                    val elapsed = SystemClock.elapsedRealtime() - started
                    if (elapsed >= 8_000L) {
                        _events.emit(UiEvent("Barre : arrêt de sécurité (8 s)", true))
                        break
                    }
                    delay(300)
                }
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                _events.emit(UiEvent("Liaison perdue : barre arrêtée", true))
            } finally {
                if (_state.value.nfuActive) {
                    stopNfuInternal(null, true)
                }
            }
        }
    }

    fun stopNfu() {
        stopNfuInternal(null, true)
    }

    private fun stopNfuInternal(reason: String?, sendStop: Boolean) {
        val wasActive = _state.value.nfuActive
        nfuJob?.cancel()
        nfuJob = null
        if (wasActive) {
            _state.update { it.copy(nfuActive = false, nfuSide = null) }
        }
        if (wasActive && sendStop) {
            viewModelScope.launch {
                runCatching { api.rudder("stop") }
                delay(180)
                runCatching { api.rudder("stop") }
            }
        }
        if (reason != null) {
            _events.tryEmit(UiEvent(reason))
        }
    }

    override fun onCleared() {
        stopPolling()
        nfuJob?.cancel()
        super.onCleared()
    }
}
