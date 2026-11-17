package com.ac42pilot.app.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.ac42pilot.app.data.Ac42Api
import kotlinx.coroutines.delay

@Composable
fun SettingsDialog(
    state: UiState,
    onDismiss: () -> Unit,
    onConnect: (String) -> Unit,
    onLoadSettings: () -> Unit,
    onSaveSettings: (SettingsForm) -> Unit,
    onReboot: () -> Unit,
    onLoadLog: () -> Unit,
) {
    var form by remember { mutableStateOf<SettingsForm?>(null) }
    var baseUrlInput by remember { mutableStateOf(state.baseUrl) }
    var showRebootConfirm by remember { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        onLoadSettings()
        while (true) {
            onLoadLog()
            delay(3_000)
        }
    }

    LaunchedEffect(state.settings) {
        form = state.settings?.let { SettingsForm.from(it, state.baseUrl) }
    }

    LaunchedEffect(state.baseUrl) {
        baseUrlInput = state.baseUrl
    }

    val normalizedInput = Ac42Api.normalizeBaseUrl(baseUrlInput)
    val baseUrlChanged = normalizedInput != state.baseUrl

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(
            usePlatformDefaultWidth = false,
            dismissOnClickOutside = false,
            decorFitsSystemWindows = false,
        ),
    ) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color(0xE601060A))
                .padding(8.dp)
                .imePadding(),
            contentAlignment = Alignment.Center,
        ) {
            Surface(
                modifier = Modifier
                    .fillMaxSize()
                    .widthIn(max = 780.dp)
                    .windowInsetsPadding(WindowInsets.safeDrawing),
                shape = RoundedCornerShape(20.dp),
                color = Ac42Colors.Panel,
                border = androidx.compose.foundation.BorderStroke(1.dp, Ac42Colors.Line2),
            ) {
                Column(Modifier.fillMaxSize()) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 16.dp, vertical = 14.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Text(
                            "RÉGLAGES & DIAGNOSTIC",
                            color = Ac42Colors.Text,
                            fontSize = 12.sp,
                            fontWeight = FontWeight.Black,
                            letterSpacing = 1.5.sp,
                        )
                        Box(
                            modifier = Modifier
                                .size(34.dp)
                                .clickable(onClick = onDismiss),
                            contentAlignment = Alignment.Center,
                        ) {
                            Text("×", color = Ac42Colors.Muted2, fontSize = 26.sp)
                        }
                    }
                    HorizontalDivider(color = Ac42Colors.Line)

                    LazyColumn(
                        modifier = Modifier.weight(1f).fillMaxWidth(),
                        contentPadding = androidx.compose.foundation.layout.PaddingValues(
                            start = 16.dp,
                            end = 16.dp,
                            top = 12.dp,
                            bottom = 12.dp,
                        ),
                        verticalArrangement = Arrangement.spacedBy(14.dp),
                    ) {
                        item {
                            SectionTitle("CONNEXION AU BOÎTIER")
                            SettingsTextField(
                                value = baseUrlInput,
                                onValueChange = { baseUrlInput = it },
                                label = "Adresse IP ou URL",
                                hint = "Exemple : http://192.168.4.1. Utilisez l’IP affichée dans le moniteur série.",
                                keyboardType = KeyboardType.Uri,
                            )
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.spacedBy(8.dp),
                                verticalAlignment = Alignment.CenterVertically,
                            ) {
                                SettingsButton(
                                    text = "Tester / charger",
                                    enabled = !state.settingsLoading,
                                    modifier = Modifier.weight(1f),
                                ) {
                                    onConnect(baseUrlInput)
                                }
                                Text(
                                    when {
                                        state.settingsLoading -> "Connexion…"
                                        baseUrlChanged -> "Testez cette adresse avant d’enregistrer."
                                        state.status != null -> "Connecté"
                                        else -> "—"
                                    },
                                    color = if (baseUrlChanged) Ac42Colors.Amber else Ac42Colors.Muted,
                                    fontSize = 8.sp,
                                    modifier = Modifier.weight(1f),
                                )
                            }
                        }

                        item {
                            propertyStatus(state)
                        }
                        form?.let { current ->
                            item {
                                SectionTitle("RÉSEAU WI-FI")
                                Text(
                                    "Aucun réseau STA n’est configuré par défaut. Renseignez un réseau local pour l’utiliser en priorité.",
                                    color = Ac42Colors.Muted,
                                    fontSize = 9.sp,
                                    lineHeight = 13.sp,
                                )
                                SettingsTextField(
                                    value = current.apSsid,
                                    onValueChange = { form = current.copy(apSsid = it) },
                                    label = "Nom du réseau du boîtier (AP)",
                                    hint = "Par défaut : AC42-Pilot-XXXX. Mot de passe AP par défaut : ac42pilot.",
                                )
                                SettingsTextField(
                                    value = current.apPassword,
                                    onValueChange = { form = current.copy(apPassword = it) },
                                    label = "Mot de passe AP",
                                    hint = "Laisser vide pour conserver. 8 à 63 caractères.",
                                    password = true,
                                )
                                SettingsTextField(
                                    value = current.staSsid,
                                    onValueChange = { form = current.copy(staSsid = it) },
                                    label = "Réseau local (STA)",
                                    hint = "Vide = point d’accès seul.",
                                )
                                SettingsTextField(
                                    value = current.staPassword,
                                    onValueChange = { form = current.copy(staPassword = it) },
                                    label = "Mot de passe STA",
                                    hint = "Laisser vide pour conserver. Si le réseau est inaccessible, l’AP de secours démarre après ~15 s.",
                                    password = true,
                                )
                                SettingsTextField(
                                    value = current.hostname,
                                    onValueChange = { form = current.copy(hostname = it) },
                                    label = "Nom d’hôte mDNS",
                                    hint = "http://nom.local — lettres minuscules, chiffres et tirets.",
                                )
                            }
                            item {
                                SectionTitle("NMEA 2000")
                                SettingsTextField(
                                    value = current.controllerOverride,
                                    onValueChange = { form = current.copy(controllerOverride = it) },
                                    label = "Adresse du pupitre / contrôleur",
                                    hint = "« auto » ou adresse hexadécimale, ex. 0x07.",
                                )
                                SettingsTextField(
                                    value = current.apOverride,
                                    onValueChange = { form = current.copy(apOverride = it) },
                                    label = "Adresse du calculateur AC42",
                                    hint = "« auto » ou adresse hexadécimale, ex. 0x02.",
                                )
                                SettingsTextField(
                                    value = current.rudderTimeoutMs,
                                    onValueChange = { form = current.copy(rudderTimeoutMs = it) },
                                    label = "Délai de sécurité barre (ms)",
                                    hint = "Sans keepalive, la barre est arrêtée automatiquement (300–5000).",
                                    keyboardType = KeyboardType.Number,
                                )
                            }
                        }

                        item {
                            SectionTitle("DIAGNOSTIC")
                            DiagnosticsBlock(state)
                        }
                        item {
                            SectionTitle("JOURNAL")
                            Text(
                                state.log.takeLast(24).joinToString("\n") { entry ->
                                    val seconds = entry.ms / 1000.0
                                    "[%8.1fs] %s".format(java.util.Locale.US, seconds, entry.text)
                                }.ifBlank { "Journal vide." },
                                color = Ac42Colors.Muted2,
                                fontFamily = FontFamily.Monospace,
                                fontSize = 8.sp,
                                lineHeight = 12.sp,
                                maxLines = 28,
                                overflow = TextOverflow.Ellipsis,
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .border(1.dp, Ac42Colors.Line, RoundedCornerShape(8.dp))
                                    .background(Ac42Colors.Background)
                                    .padding(9.dp),
                            )
                        }
                    }

                    HorizontalDivider(color = Ac42Colors.Line)
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 16.dp, vertical = 12.dp),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Button(
                            onClick = { showRebootConfirm = true },
                            colors = ButtonDefaults.buttonColors(
                                containerColor = Ac42Colors.RedDim,
                                contentColor = Color(0xFFFFB0B8),
                            ),
                            shape = RoundedCornerShape(10.dp),
                        ) {
                            Text("Redémarrer", fontSize = 10.sp, fontWeight = FontWeight.Bold)
                        }
                        Box(Modifier.weight(1f))
                        TextButton(onClick = onDismiss) {
                            Text("Annuler", color = Ac42Colors.Muted2, fontSize = 10.sp)
                        }
                        Button(
                            onClick = { form?.let(onSaveSettings) },
                            enabled = form != null && !state.settingsSaving &&
                                !state.settingsLoading && !baseUrlChanged,
                            colors = ButtonDefaults.buttonColors(
                                containerColor = Ac42Colors.CyanDim,
                                contentColor = Color(0xFFBFF8FF),
                                disabledContainerColor = Ac42Colors.Panel2,
                                disabledContentColor = Ac42Colors.Muted,
                            ),
                            shape = RoundedCornerShape(10.dp),
                        ) {
                            Text(
                                if (state.settingsSaving) "Enregistrement…" else "Enregistrer",
                                fontSize = 10.sp,
                                fontWeight = FontWeight.Bold,
                            )
                        }
                    }
                }
            }
        }
    }

    if (showRebootConfirm) {
        AlertDialog(
            onDismissRequest = { showRebootConfirm = false },
            title = { Text("Redémarrer le boîtier ?") },
            text = { Text("La connexion Wi-Fi et l’interface seront brièvement interrompues.") },
            confirmButton = {
                TextButton(onClick = {
                    showRebootConfirm = false
                    onReboot()
                    onDismiss()
                }) {
                    Text("Redémarrer", color = Ac42Colors.Red)
                }
            },
            dismissButton = {
                TextButton(onClick = { showRebootConfirm = false }) {
                    Text("Annuler", color = Ac42Colors.Muted2)
                }
            },
            containerColor = Ac42Colors.Panel,
            titleContentColor = Ac42Colors.Text,
            textContentColor = Ac42Colors.Muted2,
        )
    }
}

@Composable
private fun propertyStatus(state: UiState) {
    val message = state.settingsMessage
    if (message == null && !state.settingsRebootPending) return
    val color = when {
        state.settingsError -> Color(0xFFFFB0B8)
        state.settingsRebootPending -> Color(0xFFFFD89C)
        state.settingsMessage != null -> Color(0xFFB8F7D2)
        else -> Ac42Colors.Muted2
    }
    val border = when {
        state.settingsError -> Ac42Colors.RedDim
        state.settingsRebootPending -> Ac42Colors.AmberDim
        else -> Ac42Colors.GreenDim
    }
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(border, RoundedCornerShape(9.dp))
            .border(1.dp, border, RoundedCornerShape(9.dp))
            .padding(horizontal = 10.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(3.dp),
    ) {
        message?.let {
            Text(it, color = color, fontSize = 10.sp, lineHeight = 14.sp)
        }
        if (state.settingsRebootPending) {
            Text(
                "Un redémarrage est nécessaire pour appliquer les paramètres réseau.",
                color = Ac42Colors.Amber,
                fontSize = 9.sp,
            )
        }
    }
}

@Composable
private fun DiagnosticsBlock(state: UiState) {
    val status = state.status
    val wifi = status?.wifi
    val ap = status?.ap
    val controller = status?.controller
    val can = status?.can
    val text = buildString {
        appendLine("Version        : ${status?.version ?: "—"}")
        appendLine("Uptime         : ${uptimeLabel(status?.uptimeS)}")
        appendLine("Mémoire libre  : ${status?.freeHeap?.let { "${it / 1024} Ko" } ?: "—"}")
        appendLine("Wi-Fi mode     : ${wifi?.mode ?: "—"}${if (wifi?.fallbackAp == true) " (AP secours)" else ""}")
        appendLine("Wi-Fi AP       : ${if (wifi?.apStarted == true) "${wifi.apIp} (${wifi.apSsid})" else "désactivé"}")
        appendLine("Wi-Fi STA      : ${if (wifi?.staConnected == true) wifi.staIp else if (wifi?.staConfigured == false) "non configuré" else "déconnecté"}")
        appendLine("CAN            : ${can?.state ?: "—"} · RX ${can?.rxFrames ?: "—"} · TX ${can?.txFrames ?: "—"}")
        appendLine("AC42           : ${ap?.addressHex ?: "inconnu"}${if (ap?.forced == true) " (forcé)" else ""}")
        appendLine("Pupitre        : ${controller?.addressHex ?: "absent"}${if (controller?.forced == true) " (forcé)" else ""}")
        appendLine("Dernière trame : ${can?.lastRxAgeMs?.let { "$it ms" } ?: "—"}")
        appendLine("Adresse app    : ${state.baseUrl}")
    }
    Column(verticalArrangement = Arrangement.spacedBy(5.dp)) {
        Text(
            text.trimEnd(),
            color = Ac42Colors.Muted2,
            fontFamily = FontFamily.Monospace,
            fontSize = 8.sp,
            lineHeight = 12.sp,
        )
        state.status?.warnings?.forEach { warning ->
            Text("⚠ $warning", color = Ac42Colors.Amber, fontSize = 9.sp, lineHeight = 12.sp)
        }
    }
}

@Composable
private fun SectionTitle(text: String) {
    Text(
        text,
        color = Ac42Colors.Cyan,
        fontSize = 9.sp,
        fontWeight = FontWeight.Black,
        letterSpacing = 1.5.sp,
    )
}

@Composable
private fun SettingsTextField(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    hint: String? = null,
    password: Boolean = false,
    keyboardType: KeyboardType = KeyboardType.Text,
) {
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        modifier = Modifier.fillMaxWidth(),
        label = { Text(label, fontSize = 10.sp) },
        supportingText = if (hint == null) {
            null
        } else {
            { Text(hint, fontSize = 8.sp, lineHeight = 11.sp) }
        },
        singleLine = true,
        visualTransformation = if (password) PasswordVisualTransformation() else androidx.compose.ui.text.input.VisualTransformation.None,
        keyboardOptions = KeyboardOptions(keyboardType = keyboardType),
        textStyle = TextStyle(fontSize = 11.sp),
        shape = RoundedCornerShape(9.dp),
        colors = OutlinedTextFieldDefaults.colors(
            focusedTextColor = Ac42Colors.Text,
            unfocusedTextColor = Ac42Colors.Text,
            focusedBorderColor = Ac42Colors.Cyan,
            unfocusedBorderColor = Ac42Colors.Line,
            focusedLabelColor = Ac42Colors.Cyan,
            unfocusedLabelColor = Ac42Colors.Muted2,
            focusedContainerColor = Ac42Colors.Background,
            unfocusedContainerColor = Ac42Colors.Background,
            cursorColor = Ac42Colors.Cyan,
            focusedSupportingTextColor = Ac42Colors.Muted,
            unfocusedSupportingTextColor = Ac42Colors.Muted,
        ),
    )
}

@Composable
private fun SettingsButton(text: String, enabled: Boolean, modifier: Modifier = Modifier, onClick: () -> Unit) {
    Button(
        onClick = onClick,
        modifier = modifier.heightIn(min = 38.dp),
        enabled = enabled,
        colors = ButtonDefaults.buttonColors(
            containerColor = Ac42Colors.Panel2,
            contentColor = Ac42Colors.Muted2,
            disabledContainerColor = Ac42Colors.Panel,
            disabledContentColor = Ac42Colors.Muted,
        ),
        shape = RoundedCornerShape(9.dp),
    ) {
        Text(text, fontSize = 10.sp, fontWeight = FontWeight.Bold)
    }
}
